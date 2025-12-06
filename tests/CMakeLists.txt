include(FetchContent)

FetchContent_Declare(
        GTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(GTest)

FetchContent_Declare(
		render_doc
		SOURCE_SUBDIR  "<invalid path>" # this forces FetchContent to pull but not configure the dependency
		GIT_REPOSITORY https://github.com/baldurk/renderdoc.git
		GIT_TAG        v1.41
)
FetchContent_MakeAvailable(render_doc)

include(GoogleTest)

add_executable(vex_tests
	"SynchronizationTest.cpp"
	"UploadReadbackTest.cpp"
    "MipGenerationTest.cpp"
    "BufferViewTest.cpp"
    "GraphicsBackendTest.cpp"
	"ReflectionTest.cpp"
    "ClearTests.cpp"
	"RenderDoc.h"
	"RenderDoc.cpp"
)

target_compile_definitions(Vex PUBLIC VEX_TESTS=1)

target_link_libraries(vex_tests PRIVATE
    GTest::gtest_main
    Vex
)

add_header_only_dependency(vex_tests render_doc "${render_doc_SOURCE_DIR}" "renderdoc/api/app" "")

vex_setup_runtime(vex_tests)

gtest_discover_tests(
    vex_tests
    DISCOVERY_MODE PRE_TEST  # Discover tests when running CTest, not at build time
)
