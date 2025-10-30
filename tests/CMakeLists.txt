
include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0 # Or a specific commit hash
)
FetchContent_MakeAvailable(googletest)

add_executable(vidpid_helper_tests
        TS_vidpid.cpp
)

target_link_libraries(vidpid_helper_tests
        PRIVATE
        GTest::gtest_main
        fs_tools
)

include(GoogleTest)
gtest_discover_tests(vidpid_helper_tests)
