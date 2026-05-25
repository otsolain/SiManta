@echo off
setlocal enabledelayedexpansion

REM =============================================================
REM   Simanta - Simple Windows Build
REM   Thin wrapper around build_manual.bat that lets you pick
REM   Debug or Release from the command line.
REM
REM   Usage:
REM     build_windows.bat [Release|Debug]
REM =============================================================

pushd "%~dp0"

set "BUILD_TYPE=%1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

if /I not "%BUILD_TYPE%"=="Release" if /I not "%BUILD_TYPE%"=="Debug" (
    echo [ERROR] Build type must be Release or Debug.
    popd
    exit /b 1
)

echo =============================================
echo   Simanta - Windows Build
echo   Build type: %BUILD_TYPE%
echo =============================================
echo.

REM Detect VS + Qt + cmake (same logic as build_manual.bat)
set "VCVARS="
for %%E in (Insiders Community Professional Enterprise BuildTools) do (
    for %%V in (18 17) do (
        if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
            set "VS_ROOT=C:\Program Files\Microsoft Visual Studio\%%V\%%E"
        )
    )
)
if not defined VCVARS (
    echo [ERROR] Visual Studio 2022 not found.
    popd
    exit /b 1
)
call "%VCVARS%" >nul

if not defined Qt6_DIR (
    for %%Q in (6.11.0 6.10.0 6.9.3 6.9.2 6.9.1 6.9.0 6.8.3 6.8.2 6.8.1 6.8.0 6.7.3 6.7.2 6.7.1 6.7.0 6.6.3 6.6.2 6.6.1 6.6.0 6.5.3) do (
        if not defined Qt6_DIR if exist "C:\Qt\%%Q\msvc2022_64\lib\cmake\Qt6" (
            set "Qt6_DIR=C:\Qt\%%Q\msvc2022_64\lib\cmake\Qt6"
        )
    )
)
if not defined Qt6_DIR (
    echo [ERROR] Qt 6.x for MSVC 2022 64-bit not found under C:\Qt.
    popd
    exit /b 1
)
echo [*] Using Qt6 at: %Qt6_DIR%

set "CMAKE_EXE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    set "CMAKE_EXE=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

if not exist build mkdir build

echo.
echo [*] Configuring...
"%CMAKE_EXE%" -S . -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH="%Qt6_DIR%\..\..\.."
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    popd
    exit /b 1
)

echo.
echo [*] Building...
"%CMAKE_EXE%" --build build --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Build failed.
    popd
    exit /b 1
)

echo.
echo [OK] Build successful.
echo     Teacher: build\teacher\%BUILD_TYPE%\SimantaTeacher.exe
echo     Student: build\student\%BUILD_TYPE%\SimantaStudent.exe
echo.
echo To package with Qt DLLs run:  build_and_package.bat
echo To build installer:           build_installer.bat
popd
endlocal
exit /b 0
