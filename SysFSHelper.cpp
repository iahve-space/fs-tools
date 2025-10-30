#include "SysFSHelper.hpp"

#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace fs_tools {
using namespace std::string_literals;

auto SysFSHelper::list_functions(const std::string& /*dev_usb_root*/,
                                 const std::vector<std::string>& classRoots)
    -> std::vector<UsbFunction> {
  std::vector<UsbFunction> out;

  for (const auto& classRoot : classRoots) {
    if (!path_exists(classRoot) || !is_dir(classRoot)) {
      continue;
    }

    for (const auto& entryPath : list_dirs(classRoot)) {
      const std::string UEVENT = entryPath + "/uevent";
      if (!path_exists(UEVENT)) {
        continue;
      }

      std::string content;
      if (!read_file(UEVENT, content)) {
        continue;
      }

      // DEVNAME
      std::string devname;
      {
        size_t pos = 0;
        while (true) {
          auto eol = content.find('\n', pos);
          if (std::string line = content.substr(
                  pos, (eol == std::string::npos ? content.size() : eol) - pos);
              line.rfind("DEVNAME="s, 0) == 0) {
            devname = line.substr(size("DEVNAME="s));
            break;
          }
          if (eol == std::string::npos) {
            break;
          }
          pos = eol + 1;
        }
      }
      if (devname.empty()) {
        continue;
      }

      // Разыменовать device → подняться к USB и взять VID:PID
      const std::string DEVICE_LINK = entryPath + "/device";
      if (!path_exists(DEVICE_LINK)) {
        continue;
      }
      std::error_code error;
      auto node = fs_tools::canonical_path(DEVICE_LINK, error);
      if (error || node.empty()) {
        continue;
      }

      auto vid_pid = usb_ids_for(node);
      if (!vid_pid) {
        continue;
      }

      UsbFunction func;
      func.m_vid = vid_pid->first;
      func.m_pid = vid_pid->second;
      func.m_usbNode = node;

      auto slash = classRoot.find_last_of('/');
      func.m_class_name =
          slash == std::string::npos ? classRoot : classRoot.substr(slash + 1);
      func.m_dev_name = devname;
      func.m_dev_path = "/dev/" + devname;
      out.push_back(std::move(func));
    }
  }

  // dedup по devPath (бывают дубли через разные симлинки)
  std::sort(out.begin(), out.end(), [](const auto& first, const auto& second) {
    if (first.m_dev_path != second.m_dev_path) {
      return first.m_dev_path < second.m_dev_path;
    }
    if (first.m_vid != second.m_vid) {
      return first.m_vid < second.m_vid;
    }
    if (first.m_pid != second.m_pid) {
      return first.m_pid < second.m_pid;
    }
    return first.m_class_name < second.m_class_name;
  });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const auto& first, const auto& second) {
                          return first.m_dev_path == second.m_dev_path &&
                                 first.m_vid == second.m_vid &&
                                 first.m_pid == second.m_pid;
                        }),
            out.end());

  return out;
}

auto SysFSHelper::find_by_id(const std::string& vid_raw,
                             const std::string& pid_raw)
    -> std::vector<UsbFunction> {
  const auto VID = normalize_id(vid_raw);
  const auto PID = normalize_id(pid_raw);
  std::vector<UsbFunction> out;
  for (auto& func : list_functions()) {
    if (func.m_vid == VID && func.m_pid == PID) {
      out.push_back(func);
    }
  }
  return out;
}

auto SysFSHelper::find(std::string dev_node) -> std::optional<UsbFunction> {
  // принять как "/dev/ttyUSB0" или "ttyUSB0" или "snd/controlC0"
  auto dev_path = "/dev/"s;
  if (dev_node.rfind(dev_path, 0) == 0) {
    dev_node = dev_node.substr(size(dev_path));
  }

  // найти в известных классах: ищем элемент класса, чей uevent содержит
  // DEVNAME=<devNode> затем по его device-ссылке/иерархии поднимаемся до
  // USB-узла и читаем VID:PID
  auto classes = default_class_roots();
  for (const auto& classRoot : classes) {
    if (!path_exists(classRoot) || !is_dir(classRoot)) {
      continue;
    }
    for (const auto& entryPath : list_dir_fs(classRoot)) {
      const std::string UEVENT = entryPath + "/uevent";
      std::string content;
      if (!path_exists(UEVENT) || !read_file(UEVENT, content)) {
        continue;
      }

      // Проверим DEVNAME=
      bool match = false;
      size_t pos = 0;
      while (true) {
        auto eol = content.find('\n', pos);
        if (std::string line = content.substr(
                pos, (eol == std::string::npos ? content.size() : eol) - pos);
            line.rfind("DEVNAME="s, 0) == 0) {
          if (auto val = line.substr(size("DEVNAME="s)); val == dev_node) {
            match = true;
          }
          break;
        }
        if (eol == std::string::npos) {
          break;
        }
        pos = eol + 1;
      }
      if (!match) {
        continue;
      }

      // symlink device -> …/usb… интерфейс/узел
      const std::string DEVICE_LINK = entryPath + "/device";
      if (!path_exists(DEVICE_LINK)) {
        continue;
      }
      std::error_code error;
      auto node = canonical_path(DEVICE_LINK, error);
      if (error || node.empty()) {
        continue;
      }

      auto vid_pid = usb_ids_for(node);
      if (!vid_pid) {
        continue;
      }

      UsbFunction func;
      func.m_vid = vid_pid->first;
      func.m_pid = vid_pid->second;
      func.m_usbNode = node;
      // classRoot like "/sys/class/tty" → take tail after last '/'
      auto slash = classRoot.find_last_of('/');
      func.m_class_name =
          slash == std::string::npos ? classRoot : classRoot.substr(slash + 1);
      func.m_dev_name = dev_node;
      func.m_dev_path = "/dev/" + dev_node;
      return func;
    }
  }
  return std::nullopt;
}

auto SysFSHelper::list_ids()
    -> std::vector<std::pair<std::string, std::string>> {
  return list_ids_at(default_usb_root());
}

auto SysFSHelper::normalize_id(std::string str) -> std::string {
  str = to_lower(std::move(str));
  if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    str = str.substr(2);
  }
  size_t iter = 0;
  while (iter < str.size() && str[iter] == '0') {
    ++iter;
  }
  str = str.substr(iter);
  if (str.empty()) {
    str = "0";
  }
  return str;
}

auto SysFSHelper::read_file(const std::string& path, std::string& out) -> bool {
  std::ifstream stream(path);
  if (!stream) {
    return false;
  }
  out.assign(std::istreambuf_iterator(stream),
             std::istreambuf_iterator<char>());
  return true;
}

auto SysFSHelper::parse_ids_from_uevent(const std::string& content,
                                        std::basic_string<char>& out_vid,
                                        std::string& out_pid) -> bool {
  size_t pos = 0;
  while (true) {
    const auto EOL = content.find('\n', pos);
    if (std::string line = content.substr(
            pos, (EOL == std::string::npos ? content.size() : EOL) - pos);
        line.rfind("PRODUCT="s, 0) == 0) {
      auto val = line.substr(size("PRODUCT="s));
      const auto STRING1 = val.find('/');
      const auto STRING2 = STRING1 == std::string::npos
                               ? std::string::npos
                               : val.find('/', STRING1 + 1);
      const std::string VID =
          STRING1 == std::string::npos ? val : val.substr(0, STRING1);
      const std::string PID =
          STRING1 == std::string::npos ? ""
          : STRING2 == std::string::npos
              ? val.substr(STRING1 + 1)
              : val.substr(STRING1 + 1, STRING2 - STRING1 - 1);
      out_vid = normalize_id(VID);
      out_pid = normalize_id(PID);
      return true;
    }
    if (EOL == std::string::npos) {
      break;
    }
    pos = EOL + 1;
  }
  return false;
}

auto SysFSHelper::usb_ids_for(const std::string& start)
    -> std::optional<std::pair<std::string, std::string>> {
  std::error_code error;
  std::string cur = canonical_path(start, error);
  for (size_t i = 0; i < MAX_DEV_NUMBER && !cur.empty(); ++i) {
    if (const std::string UEVENT = cur + "/uevent"; path_exists(UEVENT)) {
      if (std::string pid, content, vid;
          read_file(UEVENT, content) &&
          parse_ids_from_uevent(content, vid, pid)) {
        if (!vid.empty() && !pid.empty()) {
          return std::make_pair(vid, pid);
        }
      }
    }
    const auto SLASH = cur.find_last_of('/');
    if (SLASH == std::string::npos) {
      break;
    }
    cur.erase(SLASH);
  }
  return std::nullopt;
}

auto SysFSHelper::list_ids_at(const std::string& sysUsbRoot)
    -> std::vector<std::pair<std::string, std::string>> {
  std::set<std::pair<std::string, std::string>> uniq;
  if (!path_exists(sysUsbRoot) || !is_dir(sysUsbRoot)) {
    return {};
  }

  for (const auto& entryPath : list_dirs(sysUsbRoot)) {
    if (!is_dir(entryPath)) {
      continue;
    }
    const std::string UEVENT = entryPath + "/uevent";
    if (!path_exists(UEVENT)) {
      continue;
    }

    if (std::string content, vid, pid;
        read_file(UEVENT, content) &&
        parse_ids_from_uevent(content, vid, pid) && !vid.empty() &&
        !pid.empty()) {
      uniq.emplace(std::move(vid), std::move(pid));
    }
  }
  return {uniq.begin(), uniq.end()};
}

auto SysFSHelper::default_class_roots() -> std::vector<std::string> {
  // Популярные классы, у которых есть DEVNAME в uevent
  return {
      "/sys/class/tty",   "/sys/class/hidraw", "/sys/class/video4linux",
      "/sys/class/sound", "/sys/class/block",  "/sys/class/usblp",
      "/sys/class/drm",
  };
}

auto SysFSHelper::default_usb_root() -> std::string {
  return "/sys/bus/usb/devices";
}
}  // namespace fs_tools