/**
 * @file SysFSHelper.hpp
 * @brief Public API for discovering USB functions and VID:PID pairs via Linux
 * sysfs.
 * @details
 * Exposes helpers to enumerate device nodes from common sysfs class roots
 * (e.g., /sys/class/tty), resolve a /dev node back to its USB ancestor, and
 * extract vendor/product identifiers from the ancestor's `uevent`
 * (PRODUCT=...).
 */
/**
 * @defgroup usb_helpers USB Helpers
 * @brief Group of utilities for discovering USB devices and functions.
 * @details
 * Provides C++ utilities to interact with Linux sysfs, enumerate USB device
 * nodes, and extract vendor/product identifiers (VID:PID). All members belong
 * to this group.
 */
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fs_tools.hpp"

namespace fs_tools {
/** @ingroup usb_helpers */
/**
 * @brief Helper class to discover USB device functions and extract VID:PID.
 * @note All functions use real sysfs paths and are intended for Linux-like
 * systems.
 */
class SysFSHelper {
 public:
  /** @ingroup usb_helpers */

  /**
   * @ingroup usb_helpers
   * @brief Represents a single USB-related device function discovered in sysfs.
   * @details Each instance corresponds to a class device (e.g. a tty, hidraw,
   * or sound node) that can be traced back to a USB ancestor via its sysfs
   * path. Contains the resolved VID/PID, sysfs and /dev locations, and class
   * metadata.
   */
  struct UsbFunction {
    /** Vendor ID in normalized lowercase hexadecimal form (no prefix, no
     * leading zeros). */
    std::string m_vid;

    /** Product ID in normalized lowercase hexadecimal form (no prefix, no
     * leading zeros). */
    std::string m_pid;

    /** Absolute sysfs path of the corresponding USB ancestor (e.g.
     * "/sys/bus/usb/devices/1-1"). */
    std::string m_usbNode;

    /** Class name (subsystem) that provided the device node — for example:
     * "tty", "hidraw", "video4linux", "sound", "block", "usblp", or "drm". */
    std::string m_class_name;

    /** Device node name as found in the class device's `uevent` (DEVNAME=...),
     * for example: "ttyUSB0", "hidraw0", "video0", "snd/controlC0", or "sda".
     */
    std::string m_dev_name;

    /** Full /dev path when available — for example:
     * "/dev/ttyUSB0", "/dev/hidraw0", "/dev/video0", "/dev/sda", or
     * "/dev/snd/controlC0". */
    std::string m_dev_path;
  };

  static constexpr size_t MAX_DEV_NUMBER = 100;

  /**
   * @ingroup usb_helpers
   * @brief List USB functions discovered from sysfs.
   * @details
   * Scans the provided sysfs class roots (defaults to common classes such as
   * tty, hidraw, video4linux, sound, block, usblp, drm). For each class entry,
   * reads its `uevent` to obtain `DEVNAME=...`, resolves the `device` symlink
   * to the underlying sysfs node, then ascends to the nearest USB ancestor and
   * parses `PRODUCT=vid/pid/...` from its `uevent`. Results are deduplicated by
   * (dev path, VID, PID). No path normalization beyond canonicalization is
   * performed.
   *
   * @param dev_usb_root USB sysfs root to use (default: default_usb_root()).
   * @param classRoots   Sysfs class roots to scan (default:
   * default_class_roots()).
   * @return Vector of discovered functions with resolved dev path, class, and
   * VID:PID.
   */
  static auto list_functions(
      const std::string& dev_usb_root = default_usb_root(),
      const std::vector<std::string>& classRoots = default_class_roots())
      -> std::vector<UsbFunction>;

  /**
   * @ingroup usb_helpers
   * @brief Find USB functions by vendor/product identifiers.
   * @details Normalizes both @p vid_raw and @p pid_raw (accepts hex
   * with/without `0x` prefix, any case, and leading zeros) and filters the
   * result of `list_functions()`.
   * @param vid_raw Vendor ID string.
   * @param pid_raw Product ID string.
   * @return All functions whose normalized VID and PID match the inputs.
   */
  static auto find_by_id(const std::string& vid_raw, const std::string& pid_raw)
      -> std::vector<UsbFunction>;

  /**
   * @ingroup usb_helpers
   * @brief Resolve a device node to its USB function (VID:PID, class, sysfs
   * path).
   * @details Accepts forms like "/dev/ttyUSB0", "ttyUSB0" or "snd/controlC0".
   * Strips a leading "/dev/" if present, scans known class roots for a matching
   * `DEVNAME`, follows the `device` symlink, and ascends to the nearest USB
   * ancestor to extract `PRODUCT=vid/pid/...`.
   * @param dev_node Device node name or path.
   * @return Matching USB function, or `std::nullopt` if not found.
   */
  static auto find(std::string dev_node) -> std::optional<UsbFunction>;

  /**
   * @ingroup usb_helpers
   * @brief List all unique (VID, PID) pairs present under the USB sysfs root.
   * @details Iterates child directories of @p sysUsbRoot (default from
   * `default_usb_root()`), reads each `uevent`, and parses
   * `PRODUCT=vid/pid/...`. Returns a deduplicated list of pairs.
   * @return Vector of unique (VID, PID) pairs.
   */
  static auto list_ids() -> std::vector<std::pair<std::string, std::string>>;

  /**
   * @ingroup usb_helpers
   * @brief Normalize a hexadecimal identifier string.
   * @details Accepts `0x`/`0X` prefixes, any case, and leading zeros, and
   * returns lowercase hex without prefix and without leading zeros ("0" if the
   * result would be empty).
   * @param str Hex string to normalize.
   * @return Normalized lowercase hex.
   */
  static auto normalize_id(std::string str) -> std::string;

 private:
  // ===== Internal helpers and variants with explicit roots (for tests) =====

  /**
   * @ingroup usb_helpers
   * @brief Read entire file into a string; returns false if it cannot be
   * opened.
   */
  static auto read_file(const std::string&, std::string&) -> bool;

  /**
   * @ingroup usb_helpers
   * @brief Extract VID and PID from a sysfs `uevent` payload.
   * @details Parses the `PRODUCT=vid/pid/...` line; outputs normalized
   * lowercase hex via `normalize_id()`.
   */
  static auto parse_ids_from_uevent(const std::string& content,
                                    std::basic_string<char>& out_vid,
                                    std::string& out_pid) -> bool;

  /**
   * @ingroup usb_helpers
   * @brief Ascend the sysfs tree to find the nearest USB ancestor that exposes
   * VID:PID.
   * @details Canonicalizes @p start; checks `uevent` in each parent directory
   * up to `MAX_DEV_NUMBER` ascents and returns the first parsed pair.
   */
  static auto usb_ids_for(const std::string& start)
      -> std::optional<std::pair<std::string, std::string>>;

  /**
   * @ingroup usb_helpers
   * @brief Enumerate USB functions by scanning the given sysfs class roots.
   * @details For each class entry: read `uevent` → get `DEVNAME` → follow
   * `device` symlink → obtain `(VID, PID)` via `usb_ids_for()` → deduplicate.
   */
  static auto list_functions_at(const std::string& sysUsbRoot,
                                const std::vector<std::string>& classRoots)
      -> std::vector<UsbFunction>;

  /**
   * @ingroup usb_helpers
   * @brief List unique (VID, PID) pairs under a specific USB sysfs root.
   * @details Reads every `uevent` under @p sysUsbRoot and parses `PRODUCT=...`.
   */
  static auto list_ids_at(const std::string& sysUsbRoot)
      -> std::vector<std::pair<std::string, std::string>>;

  /**
   * @ingroup usb_helpers
   * @brief Default sysfs class roots (tty, hidraw, v4l, sound, block, usblp,
   * drm).
   */
  static auto default_class_roots() -> std::vector<std::string>;

  /**
   * @ingroup usb_helpers
   * @brief Default sysfs USB device root ("/sys/bus/usb/devices").
   */
  static auto default_usb_root() -> std::string;
};
}  // namespace fs_tools