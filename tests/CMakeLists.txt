include(FetchContent)

FetchContent_Declare(
        GTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(GTest)

include(GoogleTest)

add_executable(vex_tests
	"SynchronizationTest.cpp"
	"UploadReadbackTest.cpp"
    "MipGenerationTest.cpp"
)

target_link_libraries(vex_tests 
    GTest::gtest_main
    Vex
)

vex_setup_runtime(vex_tests)

gtest_discover_tests(
    vex_tests
    DISCOVERY_MODE PRE_TEST  # Discover tests when running CTest, not at build time
)
