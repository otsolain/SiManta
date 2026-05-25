@echo off
REM =============================================================
REM   Build Simanta stress tester (stress_test.exe)
REM   Run from monitor/tools/ folder.
REM =============================================================
setlocal enabledelayedexpansion

pushd "%~dp0\.."

REM ---- Locate Qt (prefer mingw because it's the toolchain user has) ----
set "QT_PREFIX="
for %%Q in (6.11.0 6.10.0 6.9.3 6.9.2 6.9.1 6.9.0 6.8.3 6.8.2 6.8.1 6.8.0 6.7.3 6.7.2 6.7.1 6.7.0 6.6.3 6.6.2 6.6.1 6.6.0 6.5.3) do (
    if not defined QT_PREFIX if exist "C:\Qt\%%Q\mingw_64"   set "QT_PREFIX=C:\Qt\%%Q\mingw_64"
    if not defined QT_PREFIX if exist "C:\Qt\%%Q\msvc2022_64" set "QT_PREFIX=C:\Qt\%%Q\msvc2022_64"
)
if not defined QT_PREFIX (
    echo [ERROR] Cannot find Qt under C:\Qt\*
    popd
    exit /b 1
)
echo [*] Qt:    %QT_PREFIX%

REM ---- Locate CMake (try Qt's bundled CMake first, then PATH) ----
set "CMAKE_BIN="
where cmake >nul 2>nul
if not errorlevel 1 set "CMAKE_BIN=cmake"
if not defined CMAKE_BIN if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_BIN=C:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined CMAKE_BIN if exist "C:\Qt\Tools\CMake\bin\cmake.exe"    set "CMAKE_BIN=C:\Qt\Tools\CMake\bin\cmake.exe"
if not defined CMAKE_BIN (
    echo [ERROR] cmake.exe not found.
    popd
    exit /b 1
)
echo [*] CMake: %CMAKE_BIN%

REM ---- Locate Ninja or MinGW Make ----
set "NINJA_BIN="
if exist "C:\Qt\Tools\Ninja\ninja.exe" set "NINJA_BIN=C:\Qt\Tools\Ninja\ninja.exe"

set "MINGW_BIN="
for %%M in (mingw1310_64 mingw1120_64 mingw1100_64 mingw900_64) do (
    if not defined MINGW_BIN if exist "C:\Qt\Tools\%%M\bin\gcc.exe" set "MINGW_BIN=C:\Qt\Tools\%%M\bin"
)
if defined MINGW_BIN (
    echo [*] MinGW: %MINGW_BIN%
    set "PATH=%MINGW_BIN%;%PATH%"
)

REM ---- Configure ----
if not exist build_tools (
    if defined NINJA_BIN (
        "%CMAKE_BIN%" -G "Ninja" -B build_tools tools ^
            -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
            -DCMAKE_MAKE_PROGRAM="%NINJA_BIN%" ^
            -DCMAKE_BUILD_TYPE=Release
    ) else (
        "%CMAKE_BIN%" -G "MinGW Makefiles" -B build_tools tools ^
            -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
            -DCMAKE_BUILD_TYPE=Release
    )
    if errorlevel 1 (
        echo [ERROR] cmake configure failed.
        popd
        exit /b 1
    )
)

REM ---- Build ----
"%CMAKE_BIN%" --build build_tools --target stress_test
if errorlevel 1 (
    echo [ERROR] build failed.
    popd
    exit /b 1
)

REM ---- Find output exe ----
set "EXE="
if exist build_tools\Release\stress_test.exe set "EXE=build_tools\Release\stress_test.exe"
if not defined EXE if exist build_tools\stress_test.exe set "EXE=build_tools\stress_test.exe"

if defined EXE (
    REM Deploy Qt DLLs next to it
    "%QT_PREFIX%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime --no-system-d3d-compiler "!EXE!" >nul 2>nul
    REM Copy MinGW runtime DLLs (windeployqt doesn't do these)
    if defined MINGW_BIN (
        for %%R in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
            if exist "%MINGW_BIN%\%%R" copy /Y "%MINGW_BIN%\%%R" "build_tools\" >nul 2>nul
        )
    )
    echo.
    echo ============================================
    echo   [OK] Built: !EXE!
    echo ============================================
    echo.
    echo Usage:
    echo   !EXE! --count 20 --host 127.0.0.1
    echo   !EXE! --count 13 --host 192.168.1.10
) else (
    echo [WARN] Build succeeded but stress_test.exe not found.
)

popd
endlocal
