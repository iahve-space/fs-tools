include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED SOFTWARE_VERSION)
    message(FATAL_ERROR "SOFTWARE_VERSION is not defined before including install.cmake")
endif ()

set(FS_TOOLS_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fs_tools-${SOFTWARE_VERSION}")

set(FS_TOOLS_TARGETS
        fs_tools
)

install(TARGETS ${FS_TOOLS_TARGETS}
        EXPORT fs_toolsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)


install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

install(EXPORT fs_toolsTargets
        NAMESPACE fs_tools::
        DESTINATION ${FS_TOOLS_CMAKE_DIR}
        FILE fs_toolsTargets.cmake
)

set(_FS_TOOLS_BUILD_EXPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/fs_toolsTargets.cmake")
export(EXPORT fs_toolsTargets
        NAMESPACE fs_tools::
        FILE "${_FS_TOOLS_BUILD_EXPORT_FILE}")

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/fs_toolsConfigVersion.cmake"
        VERSION "${SOFTWARE_VERSION}"
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fs_toolsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/fs_toolsConfig.cmake"
        INSTALL_DESTINATION "${FS_TOOLS_CMAKE_DIR}"
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/fs_toolsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/fs_toolsConfigVersion.cmake"
        DESTINATION "${FS_TOOLS_CMAKE_DIR}"
)