/**
 * @file fs_tools.hpp
 * @brief Small POSIX-oriented filesystem helpers for environments without
 * &lt;filesystem&gt; (e.g., GCC 7 on ARMv7).
 * @details
 * Provides thin wrappers over POSIX syscalls to query paths and perform basic
 * operations:
 *   - Existence/type checks via lstat(2)
 *   - Simple path join
 *   - Directory listing (non-recursive)
 *   - One-shot symlink read
 *   - Single-directory creation
 *
 * All functions are header-only and intend to be minimal and portable across
 * typical Linux targets.
 * @note The results of these functions are inherently racy against concurrent
 * filesystem changes (TOCTTOU).
 */
#pragma once
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

namespace fs_tools {
/**
 * @brief Return a lowercased copy of the input string.
 * @param str Input string (copied by value).
 * @return Lowercased copy of @p str.
 * @details Uses `std::tolower` on each byte; behavior for non-ASCII depends on
 * the current C locale.
 */
[[maybe_unused]]  inline auto to_lower(std::string str) -> std::string {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](const unsigned char VAL) {
                   return static_cast<char>(std::tolower(VAL));
                 });
  return str;
}

[[maybe_unused]] inline auto canonical_path(const std::string& input, std::error_code& error)
    -> std::string {
  std::array<char, PATH_MAX> buf;
  if (realpath(input.c_str(), buf.data()) == nullptr) {
    error = std::error_code(errno, std::generic_category());
    return {};
  }
  error.clear();
  return buf.data();
}

/**
 * @brief Check whether a path exists as any filesystem object.
 * @param val Path (absolute or relative).
 * @return `true` if the path exists, `false` otherwise.
 * @details
 * Uses `lstat(2)`, so the function returns `true` for existing symlinks even if
 * their targets are broken. This checks *any* object type: regular files,
 * directories, symlinks, sockets, fifos, etc.
 * @note Does not follow symlinks. For “followed” semantics use `stat(2)`
 * instead.
 */
[[maybe_unused]] inline auto path_exists(const std::string& val) -> bool {
  struct stat stt{};
  return ::lstat(val.c_str(), &stt) == 0;
}

/**
 * @brief Test whether a path denotes a directory.
 * @param val Path to test.
 * @return `true` if the path exists and is a directory, `false` otherwise.
 * @details
 * Uses `lstat(2)`, therefore returns `false` for symlinks to directories (it
 * inspects the link itself). If you need to follow symlinks, prefer `stat(2)`
 * in your own code.
 */
[[maybe_unused]] inline auto is_dir(const std::string& val) -> bool {
  struct stat stt{};
  return ::lstat(val.c_str(), &stt) == 0 && S_ISDIR(stt.st_mode);
}

/**
 * @brief Test whether a path is a symbolic link.
 * @param val Path to test.
 * @return `true` if the path exists and is a symlink, `false` otherwise.
 * @details Uses `lstat(2)` and inspects the link object itself.
 */
[[maybe_unused]] inline auto is_symlink(const std::string& val) -> bool {
  struct stat stt{};
  return ::lstat(val.c_str(), &stt) == 0 && S_ISLNK(stt.st_mode);
}

/**
 * @brief Test whether a path is a regular file.
 * @param val Path to test.
 * @return `true` if the path exists and is a regular file, `false` otherwise.
 * @details Uses `lstat(2)`; symlinks to regular files return `false` (link is
 * not followed).
 */
[[maybe_unused]] inline auto is_reg(const std::string& val) -> bool {
  struct stat stt{};
  return ::lstat(val.c_str(), &stt) == 0 && S_ISREG(stt.st_mode);
}

/**
 * @brief Join two path segments using '/' if needed.
 * @param head Left-hand path segment.
 * @param tail Right-hand path segment.
 * @return Concatenated path.
 * @details
 * If @p head is empty, returns @p tail. If @p head already ends with '/', it
 * simply appends @p tail. No normalization or cleanup (e.g., duplicate
 * separators, `..`) is performed.
 */
[[maybe_unused]] inline auto join_path(const std::string& head, const std::string& tail)
    -> std::string {
  if (head.empty() || head.back() == '/') {
    return head + tail;
  }
  return head + "/" + tail;
}

/**
 * @brief Extract the directory component of a given path.
 * @param path The input path.
 * @return The directory component; attempts to return an absolute path when
 *         the input is relative (by prefixing the current working directory),
 *         falling back to "." if the CWD cannot be obtained.
 */
[[maybe_unused]] inline auto dir_name(const std::string& path) -> std::string {
  if (path.empty()) {
    std::array<char, PATH_MAX> cwd;
    if (::getcwd(cwd.data(), sizeof(cwd)) != nullptr) {
      return cwd.data();
    }
    return ".";
  }
  const size_t POS = path.find_last_of('/');
  std::string dir;
  if (POS == std::string::npos) {
    dir = ".";
  } else if (POS == 0) {
    dir = "/";
  } else {
    dir = path.substr(0, POS);
  }
  if (dir.empty()) {
    dir = ".";
  }
  if (dir[0] != '/') {
    std::array<char, PATH_MAX> cwd;
    if (::getcwd(cwd.data(), sizeof(cwd)) != nullptr) {
      dir = std::string(cwd.data()) + "/" + dir;
    }
  }
  return dir;
}

/**
 * @brief List regular files in a directory that match a POSIX pattern
 * (non-recursive).
 * @param dir Directory path to scan.
 * @param pattern POSIX fnmatch pattern (e.g., "*.bin", "config_*",
 * "file?.txt").
 * @return Vector of file paths matching the pattern.
 * @details
 * - Only includes regular files (not directories).
 * - Uses POSIX `fnmatch(3)` with `FNM_PATHNAME` for matching.
 * - Important: an empty @p pattern matches **nothing**; pass "*" to match all
 * entries.
 * - Common patterns:
 *   - "*.bin" — all `.bin` files
 *   - "file_*" — files starting with `file_`
 *   - "config_??.txt" — config files with 2 chars between
 * - Returns an empty vector on error.
 */
[[maybe_unused]] inline auto list_dir_fs(const std::string& dir,
                        const std::string& pattern = "*")
    -> std::vector<std::string> {
  std::vector<std::string> out;
  DIR* fs_dir = ::opendir(dir.c_str());
  if (fs_dir == nullptr) {
    return out;
  }

  while (const auto* subdir = ::readdir(fs_dir)) {
    if (std::strcmp(subdir->d_name, ".") == 0 ||
        std::strcmp(subdir->d_name, "..") == 0) {
      continue;
    }

    // Check pattern match
    if (::fnmatch(pattern.c_str(), subdir->d_name, FNM_PATHNAME) != 0) {
      continue;
    }

    std::string path = dir;
    if (!path.empty() && path.back() != '/') {
      path.push_back('/');
    }
    path += subdir->d_name;

    // if (is_reg(path)) {
    out.push_back(path);
    // }
  }
  ::closedir(fs_dir);
  return out;
}

/**
 * @brief List entries in a directory (non-recursive).
 * @param dir Directory path to scan.
 * @return Vector of entry paths joined with the input directory.
 * @details
 * - Skips "." and "..".
 * - Includes hidden entries (dotfiles) other than the two above.
 * - Returns an empty vector on error (e.g., if the directory cannot be opened).
 * - The result is **not** sorted; call `std::sort` if you need ordering.
 * @note This function does not recurse into subdirectories.
 */
[[maybe_unused]] inline auto list_dirs(const std::string& dir) -> std::vector<std::string> {
  std::vector<std::string> out;
  DIR* fs_dir = ::opendir(dir.c_str());
  if (fs_dir == nullptr) {
    return out;
  }
  while (const auto* subdir = ::readdir(fs_dir)) {
    if (subdir->d_name[0] == '.' &&
        (subdir->d_name[1] == '\0' ||
         (subdir->d_name[1] == '.' && subdir->d_name[2] == '\0'))) {
      continue;
    }
    out.push_back(join_path(dir, subdir->d_name));
  }
  ::closedir(fs_dir);
  return out;
}

/**
 * @brief Read a symbolic link target once.
 * @param path Path to the symlink.
 * @return Target path of the symlink, or empty string on error.
 * @details
 * - Uses `readlink(2)` with a 4096-byte buffer and null-terminates the result.
 * - If the returned target is relative, it is made absolute relative to the
 *   parent directory of @p path.
 * - No further normalization is performed.
 * @note Some systems may allow symlink targets longer than 4096 bytes; such
 * targets will be truncated.
 */
[[maybe_unused]] inline auto readlink_once(const std::string& path) -> std::string {
  std::string buf;
  buf.resize(256U);
  const ssize_t NUM = ::readlink(path.c_str(), buf.data(), buf.size() - 1);
  if (NUM < 0) {
    return {};
  }
  buf[NUM] = '\0';
  buf.resize(static_cast<size_t>(NUM));
  // readlink may return a relative path — make it absolute if needed
  if (!buf.empty() && buf[0] != '/') {
    // make absolute relative to the parent directory of the input path
    auto pos = path.find_last_of('/');
    const std::string BASE =
        (pos == std::string::npos) ? "." : path.substr(0, pos);
    return join_path(BASE, buf);
  }
  return buf;
}

/**
 * @brief Create a directory if it does not already exist.
 * @param path Directory path to create.
 * @param mode POSIX permissions used on creation (ignored on some platforms).
 * @return `true` if the directory exists after the call, `false` otherwise.
 * @details
 * - Succeeds if the directory already exists.
 * - If a non-directory object exists at @p path, returns `false`.
 * - Not recursive; use a separate helper if you need `mkdir -p` semantics.
 * @warning Subject to TOCTTOU races if multiple processes create the same path
 * concurrently.
 */
[[maybe_unused]] inline auto make_dir_once(const std::string& path, mode_t mode = 0755U)
    -> bool {
  if (is_dir(path)) {
    return true;
  }
  if (::mkdir(path.c_str(), mode) == 0) {
    return true;
  }
  if (errno == EEXIST) {
    // Something exists at this path; confirm it's a directory.
    return is_dir(path);
  }
  return false;
}
}  // namespace fs_tools
