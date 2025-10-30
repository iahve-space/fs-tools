# fs_tools

[![Release](https://img.shields.io/github/v/release/iahve-space/fs-tools?include_prereleases&label=release)](https://github.com/iahve-space/fs-tools/releases)
![Coverage](badges/coverage.svg)
[![License](https://img.shields.io/github/license/iahve-space/protolib)](LICENSE)
[![Docs](https://github.com/iahve-space/fs-tools/actions/workflows/docs.yml/badge.svg)](https://iahve-space.github.io/protolib/)

**fs_tools** is a cross-platform C++17 library that provides a set of POSIX helpers for working with the filesystem and the **sysfs** subsystem on Linux. It simplifies access to devices, directories, and symlinks without using `<filesystem>` and includes the **SysFSHelper** class for discovering and analyzing USB devices via sysfs.

---

## 🚀 Features

- 📂 Working with POSIX filesystem (existence checks, listing, symlinks)
- 🔍 Scanning **sysfs** to find USB devices and their VID/PID
- 🧭 Determining which physical device corresponds to `/dev/...`
- ⚙️ Does not require root privileges
- 🧱 Fully header-only implementation

---

## 🧩 Structure

The library consists of two main parts:

1. **fs_tools.hpp** — basic POSIX helpers (an alternative to `<filesystem>` for older systems)
2. **SysFSHelper.hpp** — a class for analyzing the `/sys` structure and mapping USB devices to their device nodes.

---

## 🧠 How SysFSHelper Works

The `SysFSHelper` class uses the virtual filesystem **sysfs** (`/sys`) to establish a connection between `/dev/...` and physical devices.

Example chain for `/dev/ttyUSB0`:

```
/dev/ttyUSB0
  ↓
/sys/class/tty/ttyUSB0 → ../../devices/pci0000:00/.../usb1/1-1/1-1.1/1-1.1:1.0/ttyUSB0
  ↓
/sys/bus/usb/devices/1-1/uevent  →  PRODUCT=1a86/7523/2600
```

The library traverses up the sysfs tree until it finds an ancestor with `PRODUCT=vid/pid/...` and returns the pair `1a86:7523`.

---

## 💻 Usage Example

```cpp
#include "SysFSHelper.hpp"
#include <iostream>

int main() {
    auto functions = SysFSHelper::list_functions();
    for (auto &f : functions) {
        std::cout << f.m_dev_path << " -> "
                  << f.m_vid << ":" << f.m_pid
                  << " (" << f.m_class_name << ")" << std::endl;
    }
    return 0;
}
```

Example output:

```
/dev/ttyUSB0 -> 1a86:7523 (tty)
/dev/hidraw0 -> 046d:c534 (hidraw)
/dev/video0  -> 05a3:9331 (video4linux)
```

---

## 📦 Installation

### CMake + FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
        fs_tools
        QUIET
        GIT_REPOSITORY https://github.com/iahve-space/fs-tools.git
        GIT_TAG v1.0.11
)
FetchContent_MakeAvailable(fs_tools)

target_link_libraries(your_target PRIVATE fs_tools)
```

### Conan

```bash
conan install fs_tools/[>=1.0.0]@
```

---

## 📚 Main API

### Class `SysFSHelper`

#### `list_functions()`

Returns a list of all USB functions found on the system.

#### `find(const std::string &dev)`

Returns information about a specific `/dev/...` node.

#### `find_by_id(const std::string &vid, const std::string &pid)`

Finds all devices with the specified VID/PID.

#### `list_ids()`

Returns all unique `(VID, PID)` pairs of found USB devices.

#### `UsbFunction`

Structure describing a found device:

```cpp
struct UsbFunction {
    std::string m_vid;        // Vendor ID (VID)
    std::string m_pid;        // Product ID (PID)
    std::string m_usbNode;    // Path in /sys/bus/usb/devices
    std::string m_class_name; // Subsystem (tty, sound, hidraw...)
    std::string m_dev_name;   // Device name (DEVNAME)
    std::string m_dev_path;   // Full path /dev/...
};
```

---

## 📘 fs_tools Module

`fs_tools` is a helper part of the library implementing basic POSIX filesystem functions without `<filesystem>`.

### Main functions:

- `path_exists(path)` — check existence
- `is_dir(path)` — check if it's a directory
- `is_symlink(path)` — check if the path is a symlink
- `is_reg(path)` — check if the path is a regular file
- `join_path(head, tail)` — join paths
- `dir_name(path)` — extract directory from path
- `list_dirs(dir)` / `list_dir_fs(dir, pattern)` — directory listing
- `readlink_once(path)` — read symlink target
- `make_dir_once(path)` — create directory (like `mkdir -p`)

### Usage example

```cpp
#include "fs_tools.hpp"
#include <iostream>
using namespace fs_tools;

int main() {
    if (!path_exists("/sys")) {
        std::cerr << "/sys not found" << std::endl;
        return 1;
    }

    for (auto &p : list_dirs("/sys/class/tty")) {
        std::cout << p << std::endl;
    }
}
```

---

## 🧩 Requirements

- Linux with **sysfs** support (`/sys`)
- C++17 or higher
- No external dependencies

---
