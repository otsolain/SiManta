@echo off
setlocal enabledelayedexpansion

REM =============================================================
REM   Simanta - Build + Package (no installer)
REM   Produces dist\teacher\ and dist\student\ with all DLLs
REM   required to run the .exe on any Windows PC.
REM =============================================================

pushd "%~dp0"

echo ============================================
echo   Simanta - Build and Package
echo ============================================
echo.

REM -- 1) Build --
echo [1/3] Building...
call "%~dp0build_manual.bat"
if errorlevel 1 (
    echo BUILD FAILED!
    popd
    exit /b 1
)

REM -- Detect which toolchain was used (inspect CMakeCache) --
set "TOOLCHAIN="
set "QT_BIN="
set "MINGW_BIN="
if exist "build\CMakeCache.txt" (
    findstr /C:"CMAKE_CXX_COMPILER:" "build\CMakeCache.txt" | findstr /I "mingw" >nul
    if not errorlevel 1 set "TOOLCHAIN=mingw"
)
if not defined TOOLCHAIN (
    if exist "build\CMakeCache.txt" (
        findstr /C:"CMAKE_CXX_COMPILER:" "build\CMakeCache.txt" | findstr /I "cl.exe" >nul
        if not errorlevel 1 set "TOOLCHAIN=msvc"
    )
)

REM Locate Qt bin (+ MinGW bin if relevant) for windeployqt
for %%Q in (6.11.0 6.10.0 6.9.3 6.9.2 6.9.1 6.9.0 6.8.3 6.8.2 6.8.1 6.8.0 6.7.3 6.7.2 6.7.1 6.7.0 6.6.3 6.6.2 6.6.1 6.6.0 6.5.3) do (
    if not defined QT_BIN if /I "!TOOLCHAIN!"=="msvc"  if exist "C:\Qt\%%Q\msvc2022_64\bin\windeployqt.exe" set "QT_BIN=C:\Qt\%%Q\msvc2022_64\bin"
    if not defined QT_BIN if /I "!TOOLCHAIN!"=="mingw" if exist "C:\Qt\%%Q\mingw_64\bin\windeployqt.exe"   set "QT_BIN=C:\Qt\%%Q\mingw_64\bin"
    REM Fallback: whichever exists first
    if not defined QT_BIN if exist "C:\Qt\%%Q\msvc2022_64\bin\windeployqt.exe" set "QT_BIN=C:\Qt\%%Q\msvc2022_64\bin"
    if not defined QT_BIN if exist "C:\Qt\%%Q\mingw_64\bin\windeployqt.exe"   set "QT_BIN=C:\Qt\%%Q\mingw_64\bin"
)
if not defined QT_BIN (
    echo [ERROR] Cannot find windeployqt.exe under C:\Qt\*\{msvc2022_64,mingw_64}\bin
    popd
    exit /b 1
)

if /I "!TOOLCHAIN!"=="mingw" (
    for %%M in (mingw1310_64 mingw1120_64 mingw1100_64 mingw900_64 mingw810_64) do (
        if not defined MINGW_BIN if exist "C:\Qt\Tools\%%M\bin\gcc.exe" set "MINGW_BIN=C:\Qt\Tools\%%M\bin"
    )
    if not defined MINGW_BIN (
        echo [WARN] MinGW bin not found; MinGW runtime DLLs may be missing from dist.
    ) else (
        echo [*] MinGW bin:  !MINGW_BIN!
    )
)

echo [*] Toolchain:  !TOOLCHAIN!
echo [*] Qt bin:     !QT_BIN!

REM -- 2) Clean dist folders --
echo.
echo [2/3] Preparing dist folders...
if exist dist\teacher rmdir /s /q dist\teacher
if exist dist\student rmdir /s /q dist\student
mkdir dist\teacher
mkdir dist\student

REM MinGW puts .exe directly in build\<sub>\; MSVC puts it in build\<sub>\Release\.
set "TEACHER_EXE="
if exist "build\teacher\Release\SimantaTeacher.exe" set "TEACHER_EXE=build\teacher\Release\SimantaTeacher.exe"
if not defined TEACHER_EXE if exist "build\teacher\SimantaTeacher.exe" set "TEACHER_EXE=build\teacher\SimantaTeacher.exe"
if not defined TEACHER_EXE (
    echo [ERROR] SimantaTeacher.exe not found in build tree.
    popd
    exit /b 1
)

set "STUDENT_EXE="
if exist "build\student\Release\SimantaStudent.exe" set "STUDENT_EXE=build\student\Release\SimantaStudent.exe"
if not defined STUDENT_EXE if exist "build\student\SimantaStudent.exe" set "STUDENT_EXE=build\student\SimantaStudent.exe"
if not defined STUDENT_EXE (
    echo [ERROR] SimantaStudent.exe not found in build tree.
    popd
    exit /b 1
)

copy /Y "!TEACHER_EXE!" dist\teacher\ >nul
copy /Y "!STUDENT_EXE!" dist\student\ >nul

REM -- 3) Deploy Qt DLLs (+ MinGW runtimes) --
echo.
echo [3/3] Deploying Qt DLLs...

REM Put Qt bin on PATH so windeployqt can find its own dependencies.
set "PATH=!QT_BIN!;!PATH!"
if defined MINGW_BIN set "PATH=!MINGW_BIN!;!PATH!"

"!QT_BIN!\windeployqt.exe" --release --no-translations --no-compiler-runtime "dist\teacher\SimantaTeacher.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed for teacher.
    popd
    exit /b 1
)
"!QT_BIN!\windeployqt.exe" --release --no-translations --no-compiler-runtime "dist\student\SimantaStudent.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed for student.
    popd
    exit /b 1
)

REM MinGW builds also need these three runtime DLLs next to the exe.
if /I "!TOOLCHAIN!"=="mingw" if defined MINGW_BIN (
    for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
        if exist "!MINGW_BIN!\%%D" (
            copy /Y "!MINGW_BIN!\%%D" dist\teacher\ >nul
            copy /Y "!MINGW_BIN!\%%D" dist\student\ >nul
        )
    )
)

REM Icons
if exist installer\logo.ico copy /Y installer\logo.ico dist\teacher\logo.ico >nul
if exist installer\logo.ico copy /Y installer\logo.ico dist\student\logo.ico >nul
if exist installer\logo_cropped.png copy /Y installer\logo_cropped.png dist\teacher\logo.png >nul
if exist installer\logo_cropped.png copy /Y installer\logo_cropped.png dist\student\logo.png >nul

echo.
echo ============================================
echo   Distribution ready in dist\
echo     dist\teacher\SimantaTeacher.exe
echo     dist\student\SimantaStudent.exe
echo   Run the exe from its dist folder, not from build\.
echo ============================================
popd
endlocal
exit /b 0
