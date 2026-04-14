@echo off
echo ============================================
echo   SiManta - Build and Package Script
echo ============================================
echo.

echo [1/4] Building...
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
set Qt6_DIR=C:\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6
"C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -DCMAKE_BUILD_TYPE=Release
"C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
if ERRORLEVEL 1 (
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo [2/4] Creating distribution folders...
if exist dist\teacher rmdir /s /q dist\teacher
if exist dist\student rmdir /s /q dist\student
mkdir dist\teacher
mkdir dist\student
mkdir dist\installer

echo.
echo [3/4] Deploying Qt DLLs...
copy build\teacher\Release\SiMantaTeacher.exe dist\teacher\
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe dist\teacher\SiMantaTeacher.exe --release --no-translations

copy build\student\Release\SiMantaStudent.exe dist\student\
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe dist\student\SiMantaStudent.exe --release --no-translations

copy installer\logo.ico dist\teacher\logo.ico
copy installer\logo.ico dist\student\logo.ico

echo.
echo [4/4] Done! Distribution ready in dist\
echo.
echo To create setup.exe, open installer\installer.iss in Inno Setup and compile.
echo.

