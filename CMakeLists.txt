cmake_minimum_required(VERSION 3.28)

# Если из CI не прилетело — по умолчанию 0.0.0
if (NOT DEFINED PROJECT_SEMVER)
    set(PROJECT_SEMVER "0.0.0")
endif ()

# "Красивая" строка версии для встраивания в бинарь/ресурсы
if (NOT DEFINED SOFTWARE_VERSION)
    set(SOFTWARE_VERSION "${PROJECT_SEMVER}")
endif ()

message(STATUS "PROJECT_SEMVER='${PROJECT_SEMVER}'")
message(STATUS "SOFTWARE_VERSION='${SOFTWARE_VERSION}'")


project(fs_tools VERSION ${PROJECT_SEMVER} LANGUAGES C CXX)

include(CTest)  # создаёт опцию BUILD_TESTING и если ON — вызовет enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

#add_compile_options(-Werror -Wextra)
add_compile_definitions(SOFTWARE_VERSION="${SOFTWARE_VERSION}")

add_library(fs_tools STATIC SysFSHelper.cpp)

target_include_directories(fs_tools
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(fs_tools PUBLIC Threads::Threads)

add_library(fs_tools::fs_tools ALIAS fs_tools)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

include(install.cmake)