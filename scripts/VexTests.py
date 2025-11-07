import shutil
import subprocess
import sys
import platform
from pathlib import Path

""" This script runs the cmake vex_tests target for each compiler/graphics-api permutation. """

TargetOptimization = "RelWithDebInfo"
TargetVexConfig = "development"

def check_cmake():
    """Check that CMake exists in the path."""
    cmake_path = shutil.which("cmake")
    if cmake_path:
        print(f"CMake found at: {cmake_path}")
        return True
    else:
        print("CMake not found in the PATH. Make sure to add it so that this script can run tests.")
        return False
    
def check_msvc_requirements():
    """Check if MSVC is available, warn if not"""
    if platform.system() != "Windows":
        return False

    try:
        subprocess.run(['cl.exe'], capture_output=True, check=False)
        return True
    except FileNotFoundError:
        print("⚠️  MSVC compiler (cl.exe) not found in PATH")
        print("   For MSVC builds, please run from:")
        print("   - Developer Command Prompt for VS 2022, or")
        print("   - Developer PowerShell for VS 2022")
        print("   Continuing anyway - Clang builds should still work...")
        return False

def run_cmake_preset(preset_name):
    """Configure using a CMake preset"""
    print(f"\n=== Configuring preset: {preset_name} ===")

    try:
        result = subprocess.run([
            "cmake", 
            "--preset", 
            preset_name,
            "-DVEX_BUILD_TESTS=ON"
            "-DVEX_BUILD_EXAMPLES=OFF"
        ], 
        capture_output=True,  # Capture output instead of showing it
        text=True, 
        check=True
        )
        
        # Clear the line and show success
        print(f"\r✓ Configured {preset_name}" + " " * 20)
        return True
        
    except subprocess.CalledProcessError as e:
        # Clear the line and show failure
        print(f"\r✗ Configuration failed for {preset_name}" + " " * 20)

        # Show error details
        print("  Configuration error:")
        if e.stderr:
            print(f"{e.stderr.strip()}")
        
        return False

def build_tests(preset_name):
    """Build the vex_tests target for a given preset"""
    build_dir = f"out/build/{preset_name}"
    
    print(f"=== Building vex_tests for {preset_name} ===")
    
    try:
        result = subprocess.run([
            "cmake",
            "--build", build_dir,
            "--preset", preset_name + "-" + TargetVexConfig,
            "--target", "vex_tests",
            "--parallel"
        ], check=True, text=True)
        print(f"✓ Build successful for {preset_name}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Build failed for {preset_name}")
        return False

def run_tests(preset_name):
    """Run the vex_tests executable directly"""
    build_dir = Path(f"out/build/{preset_name}")
    
    # Find the test executable
    if platform.system() == "Windows":
        test_exe = build_dir / "tests" / TargetOptimization / "vex_tests.exe"
        if not test_exe.exists():
            # Try alternative locations
            test_exe = build_dir / "vex_tests.exe"
    else:
        test_exe = build_dir / "tests" / TargetOptimization / "vex_tests"
        if not test_exe.exists():
            test_exe = build_dir / "vex_tests"
    
    if not test_exe.exists():
        print(f"✗ Test executable not found for {preset_name}")
        print(f"  Looked for: {test_exe}")
        return False
    
    print(f"=== Running vex_tests for {preset_name} ===")
    print(f"Executable: {test_exe}")
    
    try:
        result = subprocess.run([
            str(test_exe),
            "--gtest_output=xml:test_results.xml",
            "--gtest_color=yes"  # Colored output
        ], check=True, text=True, cwd=test_exe.parent)
        print(f"✓ Tests passed for {preset_name}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Tests failed for {preset_name} (exit code: {e.returncode})")
        return False

def get_available_configs():
    """Determine which configurations to test based on platform"""
    base_configs = []
    
    # Only add dx12 and msvc on Windows
    if platform.system() == "Windows":
        base_configs.extend([ "clang-cl-vulkan", "clang-cl-dx12" ])
        if check_msvc_requirements():
            base_configs.extend([ "msvc-vulkan", "msvc-dx12" ])
    # Linux uses Clang and GCC
    elif platform.system() == "Linux":
        base_configs.extend([ "gcc-vulkan", "clang-vulkan" ])

    return base_configs

def main():
    print("Vex Test Runner")
    print("===============")

    if not check_cmake():
        sys.exit(1)

    test_configs = get_available_configs()
    results = {}
    
    print(f"Will test {len(test_configs)} configurations:")
    for config in test_configs:
        print(f"  - {config}")
    
    for config in test_configs:
        print(f"\n{'='*70}")
        print(f"TESTING CONFIGURATION: {config}")
        print(f"{'='*70}")
        
        # Configure
        if not run_cmake_preset(config):
            results[config] = "CONFIGURE_FAILED"
            continue
            
        # Build tests
        if not build_tests(config):
            results[config] = "BUILD_FAILED"
            continue
            
        # Run tests
        if not run_tests(config):
            results[config] = "TESTS_FAILED"
            continue
            
        results[config] = "SUCCESS"
    
    # Print summary
    print(f"\n{'='*70}")
    print("FINAL SUMMARY")
    print(f"{'='*70}")
    
    success_count = 0
    for config, status in results.items():
        status_symbol = "✓" if status == "SUCCESS" else "✗"
        print(f"{status_symbol} {config:<25} : {status}")
        if status == "SUCCESS":
            success_count += 1
    
    print(f"\nResult: {success_count}/{len(test_configs)} configurations passed")
    
    # Exit with error code if any tests failed
    if success_count != len(test_configs):
        sys.exit(1)

if __name__ == "__main__":
    main()