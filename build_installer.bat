@echo off
setlocal enabledelayedexpansion

REM =============================================================
REM   Simanta - Build + Package + Installer
REM =============================================================

pushd "%~dp0"

echo ============================================
echo   Simanta - Build + Package + Installer
echo ============================================
echo.

REM -- 1) Build + package --
call "%~dp0build_and_package.bat"
if errorlevel 1 (
    echo PACKAGING FAILED!
    popd
    exit /b 1
)

REM -- 2) Locate Inno Setup compiler --
set "ISCC="

if exist "C:\Program Files (x86)\Inno Setup 6\iscc.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 6\iscc.exe"
if not defined ISCC if exist "C:\Program Files\Inno Setup 6\iscc.exe" set "ISCC=C:\Program Files\Inno Setup 6\iscc.exe"
if not defined ISCC if exist "C:\Program Files (x86)\Inno Setup 7\iscc.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 7\iscc.exe"
if not defined ISCC if exist "C:\Program Files\Inno Setup 7\iscc.exe" set "ISCC=C:\Program Files\Inno Setup 7\iscc.exe"
if not defined ISCC if exist "C:\Program Files (x86)\Inno Setup 5\iscc.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 5\iscc.exe"
if not defined ISCC if exist "C:\Program Files\Inno Setup 5\iscc.exe" set "ISCC=C:\Program Files\Inno Setup 5\iscc.exe"

REM Also check per-user install location (default when installed without admin)
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\iscc.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\iscc.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 7\iscc.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 7\iscc.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 5\iscc.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 5\iscc.exe"

if not defined ISCC (
    where iscc >nul 2>nul
    if not errorlevel 1 set "ISCC=iscc"
)

if not defined ISCC (
    echo [ERROR] Inno Setup tidak ditemukan.
    echo.
    echo         Jalankan manual setelah install Inno Setup:
    echo           "C:\Program Files (x86)\Inno Setup 6\iscc.exe" setup_dist.iss
    echo.
    popd
    exit /b 1
)

echo.
echo [*] Inno Setup: "%ISCC%"
echo [*] Compiling installer...
"%ISCC%" setup_dist.iss
if errorlevel 1 (
    echo [ERROR] Installer compilation failed.
    popd
    exit /b 1
)

echo.
echo ============================================
echo   INSTALLER READY
echo   installer_output\Simanta.exe
echo ============================================
popd
endlocal
exit /b 0
