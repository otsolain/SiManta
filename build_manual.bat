@echo off
setlocal enabledelayedexpansion

REM =============================================================
REM   Simanta - Manual Build (Release)
REM   Auto-detects MSVC or MinGW toolchain + Qt installation.
REM =============================================================

pushd "%~dp0"

echo =============================================
echo   Simanta - Manual Build
echo =============================================
echo.

set "TOOLCHAIN="
set "VCVARS="
set "VS_ROOT="
set "Qt6_DIR="
set "QT_BIN="
set "MINGW_BIN="
set "CMAKE_EXE="

REM -- 1) Try MSVC 2022 --
for %%E in (Insiders Community Professional Enterprise BuildTools) do (
    for %%V in (18 17) do (
        if not defined VCVARS (
            if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
                set "VS_ROOT=C:\Program Files\Microsoft Visual Studio\%%V\%%E"
            )
        )
    )
)

if defined VCVARS (
    REM Find matching MSVC Qt
    for %%Q in (6.11.0 6.10.0 6.9.3 6.9.2 6.9.1 6.9.0 6.8.3 6.8.2 6.8.1 6.8.0 6.7.3 6.7.2 6.7.1 6.7.0 6.6.3 6.6.2 6.6.1 6.6.0 6.5.3) do (
        if not defined Qt6_DIR (
            if exist "C:\Qt\%%Q\msvc2022_64\lib\cmake\Qt6" (
                set "Qt6_DIR=C:\Qt\%%Q\msvc2022_64\lib\cmake\Qt6"
                set "QT_BIN=C:\Qt\%%Q\msvc2022_64\bin"
            )
        )
    )
    if defined Qt6_DIR (
        set "TOOLCHAIN=msvc"
        echo [*] Toolchain:     MSVC 2022 64-bit
        echo [*] Visual Studio: "!VS_ROOT!"
        call "!VCVARS!" >nul
        if errorlevel 1 (
            echo [ERROR] Failed to initialise Visual Studio environment.
            popd
            exit /b 1
        )
    )
)

REM -- 2) Fall back to MinGW bundled with Qt --
if not defined TOOLCHAIN (
    for %%Q in (6.11.0 6.10.0 6.9.3 6.9.2 6.9.1 6.9.0 6.8.3 6.8.2 6.8.1 6.8.0 6.7.3 6.7.2 6.7.1 6.7.0 6.6.3 6.6.2 6.6.1 6.6.0 6.5.3) do (
        if not defined Qt6_DIR (
            if exist "C:\Qt\%%Q\mingw_64\lib\cmake\Qt6" (
                set "Qt6_DIR=C:\Qt\%%Q\mingw_64\lib\cmake\Qt6"
                set "QT_BIN=C:\Qt\%%Q\mingw_64\bin"
            )
        )
    )
    if defined Qt6_DIR (
        REM Find a compatible MinGW in C:\Qt\Tools
        for %%M in (mingw1310_64 mingw1120_64 mingw1100_64 mingw900_64 mingw810_64) do (
            if not defined MINGW_BIN (
                if exist "C:\Qt\Tools\%%M\bin\gcc.exe" (
                    set "MINGW_BIN=C:\Qt\Tools\%%M\bin"
                )
            )
        )
        if defined MINGW_BIN (
            set "TOOLCHAIN=mingw"
            set "PATH=!MINGW_BIN!;!PATH!"
            echo [*] Toolchain:     MinGW 64-bit  ("!MINGW_BIN!")
        ) else (
            echo [ERROR] Found MinGW Qt but no MinGW compiler under C:\Qt\Tools.
            popd
            exit /b 1
        )
    )
)

if not defined TOOLCHAIN (
    echo [ERROR] No compatible toolchain found.
    echo         Install either:
    echo           * Visual Studio 2022 ^(with C++ workload^) + Qt msvc2022_64, or
    echo           * Qt with the MinGW 64-bit component ^(default online installer^)
    popd
    exit /b 1
)

echo [*] Qt6_DIR:       !Qt6_DIR!
echo [*] Qt bin:        !QT_BIN!

REM -- 3) Locate CMake --
where cmake >nul 2>nul
if not errorlevel 1 (
    set "CMAKE_EXE=cmake"
) else (
    if defined VS_ROOT (
        if exist "!VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=!VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        )
    )
    if not defined CMAKE_EXE (
        if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" (
            set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
        )
    )
)
if not defined CMAKE_EXE (
    echo [ERROR] CMake not found on PATH, under VS, or under C:\Qt\Tools\CMake_64.
    popd
    exit /b 1
)
echo [*] CMake:         !CMAKE_EXE!

REM -- Ninja (for MinGW builds) --
if /I "!TOOLCHAIN!"=="mingw" (
    where ninja >nul 2>nul
    if errorlevel 1 (
        if exist "C:\Qt\Tools\Ninja\ninja.exe" (
            set "PATH=C:\Qt\Tools\Ninja;!PATH!"
        ) else (
            echo [ERROR] Ninja not found. Expected C:\Qt\Tools\Ninja\ninja.exe
            popd
            exit /b 1
        )
    )
)

echo.

REM -- 4) Configure --
echo [1/2] Configuring (Release)...
if /I "!TOOLCHAIN!"=="mingw" (
    "!CMAKE_EXE!" -S . -B build -G "Ninja" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="!Qt6_DIR!\..\..\.." ^
        -DCMAKE_C_COMPILER="!MINGW_BIN!\gcc.exe" ^
        -DCMAKE_CXX_COMPILER="!MINGW_BIN!\g++.exe"
) else (
    "!CMAKE_EXE!" -S . -B build ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="!Qt6_DIR!\..\..\.."
)
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    popd
    exit /b 1
)

REM -- 5) Build --
echo.
echo [2/2] Building (this may take a few minutes)...
"!CMAKE_EXE!" --build build --config Release -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Build failed.
    popd
    exit /b 1
)

echo.
echo =============================================
echo   Build successful.
if /I "!TOOLCHAIN!"=="mingw" (
    echo     Teacher: build\teacher\SimantaTeacher.exe
    echo     Student: build\student\SimantaStudent.exe
) else (
    echo     Teacher: build\teacher\Release\SimantaTeacher.exe
    echo     Student: build\student\Release\SimantaStudent.exe
)
echo =============================================
popd
endlocal
exit /b 0
