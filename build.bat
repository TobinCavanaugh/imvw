@echo off
setlocal

REM =====================================================
REM build.bat - Build imvw with w64devkit MinGW-w64
REM =====================================================

REM Put w64devkit (64-bit MinGW-w64 GCC 12.2.0) first in PATH
set PATH=C:\raylib\w64devkit\bin;%PATH%

echo.
echo [1/2] Configuring CMake with MinGW Makefiles...
echo.

cmake -B cmake-build-debug-mingw -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE="%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static

if %ERRORLEVEL% neq 0 (
  echo.
  echo Configuring CMake with preset instead...
  cmake --preset mingw-debug
)

if %ERRORLEVEL% neq 0 (
  echo.
  echo ERROR: CMake configuration failed!
  exit /b %ERRORLEVEL%
)

echo.
echo [2/2] Building...
echo.

cmake --build cmake-build-debug-mingw -- -j

if %ERRORLEVEL% neq 0 (
  echo.
  echo ERROR: Build failed!
  exit /b %ERRORLEVEL%
)

echo.
echo ===== BUILD SUCCESSFUL =====
echo.
echo Output: cmake-build-debug-mingw\bin\imvw.exe
echo.

endlocal
