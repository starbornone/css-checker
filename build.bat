@echo off
echo Building CSS Checker C++...

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo CMake configuration failed. Trying with different generator...
    cmake .. -G "MinGW Makefiles"
    if errorlevel 1 (
        echo CMake configuration failed. Please check your CMake installation.
        pause
        exit /b 1
    )
)

:: Build the project
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed. Please check the error messages above.
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable location: build\bin\Release\css_checker.exe (or build\bin\css_checker.exe)
echo.
echo Usage examples:
echo   css_checker.exe --help
echo   css_checker.exe --src your-project-directory
echo   css_checker.exe --verbose --dry-run
echo.
pause