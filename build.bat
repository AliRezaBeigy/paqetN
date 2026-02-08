@echo off
REM Build script for PaqetN
REM Uses Qt's MinGW compiler to avoid conflicts with Strawberry Perl

echo Cleaning build directory...
if exist build rmdir /s /q build
mkdir build

echo Configuring CMake...
cmake -B build -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DAPP_VERSION=0.1.0 ^
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64"

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo Building project...
cmake --build build -j4

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
echo Executable: build\apppaqetN.exe
echo Tests: build\tst_paqetn.exe
