include(GoogleTest)

add_executable(vex_tests
	"RenderDoc.h"
	"RenderDoc.cpp"
	"SynchronizationTest.cpp"
	"UploadReadbackTest.cpp"
    "MipGenerationTest.cpp"
    "EmbeddedShaderTest.cpp"
    "BufferViewTest.cpp"
    "GraphicsBackendTest.cpp"
	"ReflectionTest.cpp"
    "ClearTests.cpp"
    "AccelerationStructureTest.cpp"
 	"RayTracingTest.cpp"
)

target_link_libraries(vex_tests PRIVATE
    GTest::gtest_main
    Vex::Vex
    Vex::ShaderCompiler
    VexDev::renderdoc
)

vex_setup_runtime(vex_tests)

gtest_discover_tests(
    vex_tests
    DISCOVERY_MODE PRE_TEST  # Discover tests when running CTest, not at build time
)
