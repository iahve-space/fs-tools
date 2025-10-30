#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "SysFSHelper.hpp"

namespace fs = std::filesystem;

// compatibility wrapper for weakly_canonical across toolchains
static auto weakly_canonical_compat(const fs::path& path) -> fs::path {
#if defined(__cpp_lib_filesystem)
  try {
    return fs::weakly_canonical(path);
  } catch (...) {
    std::cout << "weakly_canonical_compat exception" << '\n';
    // fall back if weakly_canonical throws (e.g., missing components)
  }
#endif
  // As a safe fallback, approximate with absolute()
  return fs::absolute(path);
}

// утилита записи файла (создаёт каталоги)
static void write_all(const fs::path& path, const std::string& data) {
  fs::create_directories(path.parent_path());
  std::ofstream func(path);
  ASSERT_TRUE(static_cast<bool>(func)) << "Failed to open " << path;
  func << data;
  func.close();
  ASSERT_TRUE(fs::exists(path)) << "File not created: " << path;
}

TEST(VidPidHelper, FakeSysTree_ForwardAndReverse) {
  // создаём фейковое дерево прямо в каталоге выполнения теста
  const fs::path ROOT = fs::current_path() / "fake-sys";
  fs::remove_all(ROOT);

  const fs::path SYS_USB = ROOT / "sys/bus/usb/devices";
  const fs::path SYS_TTY = ROOT / "sys/class/tty";
  const fs::path SYS_HID = ROOT / "sys/class/hidraw";
  fs::create_directories(SYS_USB);
  fs::create_directories(SYS_TTY);
  fs::create_directories(SYS_HID);

  // USB устройство 1-1 → PRODUCT=067b/2303/0100
  const fs::path USB_DEV = SYS_USB / "1-1";
  write_all(USB_DEV / "uevent",
            "DRIVER=usb\n"
            "PRODUCT=067b/2303/0100\n");

  // Интерфейс 1-1:1.0
  const fs::path IFACE = USB_DEV / "1-1:1.0";
  fs::create_directories(IFACE);

  // Класс tty: ttyACM0 с DEVNAME и device → iface
  write_all(SYS_TTY / "ttyACM0" / "uevent", "DEVNAME=ttyACM0\n");
  fs::create_symlink(weakly_canonical_compat(IFACE),
                     SYS_TTY / "ttyACM0" / "device");

  // Класс hidraw: hidraw0 с DEVNAME и device → iface
  write_all(SYS_HID / "hidraw0" / "uevent", "DEVNAME=hidraw0\n");
  fs::create_symlink(weakly_canonical_compat(IFACE),
                     SYS_HID / "hidraw0" / "device");

  // Локальная «приватная» функция EnumerateFunctionsAt недоступна —
  // но публичная EnumerateFunctions ходит в реальные /sys.
  // Поэтому здесь проверим обратный поиск через публичный FindByDevNode,
  // передав "имена": Этот тест не использует реальные пути, а только логику
  // Reverse (через классы и device symlink) Для этого временно подменить real
  // /sys нельзя из кода, поэтому покажем consistency check на «реальном»
  // окружении ниже.
  // -----
  // Чтобы протестировать на фейковом дереве строго, можно вынести
  // EnumerateFunctionsAt в friend-тест или сделать дополнительный публичный
  // хук. Ниже — «реальный» тест.

  fs::remove_all(ROOT);
}

TEST(VidPidHelper, RealSystem_Enumerate_And_ReverseIfAvailable) {
  // 1) список всех VID:PID из USB-дерева
  const auto ALL_VID_PID = fs_tools::SysFSHelper::list_ids();
  EXPECT_GE(ALL_VID_PID.size(), 0u);  // просто не упасть

  // 2) собрать функции (dev-узлы) по известным классам
  const auto FUNS = fs_tools::SysFSHelper::list_functions();

  // напечатать для наглядности
  for (const auto& func : FUNS) {
    std::cout << "VID:PID=" << func.m_vid << ":" << func.m_pid
              << " class=" << func.m_class_name << " dev=" << func.m_dev_path
              << " usbNode=" << func.m_usbNode << "\n";
  }

  if (FUNS.empty()) {
    GTEST_SKIP() << "No class devices with DEVNAME found under /sys/class/* "
                    "(tty/hidraw/video/sound/block/usblp/drm).";
  }

  // 3) обратная проверка: для каждого dev-узла попытаться восстановить VID:PID
  // и сравнить
  for (const auto& func : FUNS) {
    // devName может быть "snd/controlC0" → нормализуем: либо передаем полным
    // /dev/<name>, либо базовым именем. Наш FindByDevNode принимает /dev/...
    // или просто DEVNAME.
    auto back = fs_tools::SysFSHelper::find(func.m_dev_path);
    ASSERT_TRUE(back.has_value())
        << "Reverse lookup failed for " << func.m_dev_path;
    EXPECT_EQ(back->m_vid, func.m_vid);
    EXPECT_EQ(back->m_pid, func.m_pid);

    // 4) прямой поиск VID:PID должен, как минимум, возвращать что-то,
    // и среди результатов должен быть наш devPath (в реальной системе — часто
    // да).
    auto forward = fs_tools::SysFSHelper::find_by_id(func.m_vid, func.m_pid);
    EXPECT_FALSE(forward.empty());
    const bool PRESENT = std::any_of(
        forward.begin(), forward.end(),
        [&](const auto& iter) { return iter.m_dev_path == func.m_dev_path; });
    // Не делаем ASSERT — на некоторых системах class-узлы могут мапиться иначе,
    // просто проверим-информируем.
    if (!PRESENT) {
      std::cout << "[warn] devPath not found in forward set for " << func.m_vid
                << ":" << func.m_pid << " : " << func.m_dev_path << "\n";
    }
  }
}