@echo off

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo.
echo =============================================
echo   SiManta ??? Windows Build
echo   Build type: %BUILD_TYPE%
echo =============================================
echo.

if "%Qt6_DIR%"=="" (
    echo [!] Qt6_DIR not set. Trying default locations...
    if exist "C:\Qt\6.7.0\msvc2022_64\lib\cmake\Qt6" (
        set Qt6_DIR=C:\Qt\6.7.0\msvc2022_64\lib\cmake\Qt6
    ) else if exist "C:\Qt\6.6.0\msvc2022_64\lib\cmake\Qt6" (
        set Qt6_DIR=C:\Qt\6.6.0\msvc2022_64\lib\cmake\Qt6
    ) else (
        echo [ERROR] Cannot find Qt6. Set Qt6_DIR environment variable.
        echo         Example: set Qt6_DIR=C:\Qt\6.7.0\msvc2022_64\lib\cmake\Qt6
        exit /b 1
    )
)

echo [*] Using Qt6 at: %Qt6_DIR%

if not exist build mkdir build
cd build

echo [*] Running CMake...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%Qt6_DIR%\..\..\..
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    exit /b 1
)

echo [*] Building...
cmake --build . --config %BUILD_TYPE% -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo [OK] Build successful!
echo.

echo [*] Creating distribution...
cd ..

if not exist dist\teacher mkdir dist\teacher
if not exist dist\student mkdir dist\student

copy build\teacher\%BUILD_TYPE%\lab-teacher.exe dist\teacher\ 2>nul
copy build\teacher\lab-teacher.exe dist\teacher\ 2>nul
copy build\student\%BUILD_TYPE%\lab-student.exe dist\student\ 2>nul
copy build\student\lab-student.exe dist\student\ 2>nul

echo [*] Running windeployqt for teacher...
windeployqt --release --no-translations dist\teacher\lab-teacher.exe 2>nul
echo [*] Running windeployqt for student...
windeployqt --release --no-translations dist\student\lab-student.exe 2>nul

echo.
echo =============================================
echo   Distribution created in dist/
echo   
echo   dist\teacher\  ??? Teacher Console
echo   dist\student\  ??? Student Agent
echo.
echo   To build installer:
echo     iscc installer\installer.iss
echo =============================================
echo.

