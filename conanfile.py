from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
import re
import os


class ProtoLibConan(ConanFile):
    name = "fs_tools"
    license = "MIT"
    author = "iahve1991"
    url = "https://gitlab.insitechdev.ru/comfort/embedded/libraries/fs_tools"
    description = "little file system tools for embedded systems on the linux"
    topics = ("sysfs", "aarch64", "linux", "embedded")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False]}
    default_options = {"shared": False}

    exports_sources = (
        "CMakeLists.txt", "install.cmake", "include/**", "cmake/**", "SysFSHelper.cpp",
    )

    def layout(self):
        cmake_layout(self, src_folder=".")

    def _compute_versions(self):
        full = (self.version or "0.0.0").lstrip("v")
        m = re.match(r"^(\d+\.\d+\.\d+)", full)
        core = m.group(1) if m else "0.0.0"
        return core, full

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        core, full = self._compute_versions()
        cmake = CMake(self)
        cmake.configure(variables={
            "BUILD_TESTING": "OFF",
            "SOFTWARE_VERSION": full,
            "PROJECT_SEMVER": core,
        })
        cmake.build()
        self.output.info(f"[fs_tools] generate(): SOFTWARE_VERSION={self.version}")

    def package(self):
        CMake(self).install()

    def package_info(self):
        # Базовые пути
        self.cpp_info.set_property("cmake_file_name", "fs_tools")
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.builddirs.append(f"lib/cmake/fs_tools-{self.version}")

        c = self.cpp_info.components

        c["fs_tools"].set_property("cmake_target_name", "fs_tools::fs_tools")
        c["fs_tools"].libs = ["fs_tools"]  # <-- ВАЖНО: имя файла libfs_tools.a без префикса/расширения
