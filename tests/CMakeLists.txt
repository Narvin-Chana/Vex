include(FetchContent)

FetchContent_Declare(
        GTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(GTest)

include(GoogleTest)

add_executable(vex_tests
	"SynchronisationTest.cpp"
	"UploadReadbackTest.cpp"
)

target_link_libraries(vex_tests 
    GTest::gtest_main
    Vex
)

gtest_discover_tests(vex_tests)
