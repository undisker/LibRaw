@echo off
REM LibRaw Windows Build Script
REM Builds LibRaw with all dependencies for Windows x64

setlocal enabledelayedexpansion

REM Configuration
set BUILD_TYPE=Release
set ARCH=x64
set BUILD_DIR=build_win64
set OUTPUT_DIR=dependencies\bin

REM Find Visual Studio
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo Error: Visual Studio not found
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo Error: Could not find Visual Studio installation
    exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo Error: Failed to initialize Visual Studio environment
    exit /b 1
)

echo ============================================
echo Building LibRaw Dependencies for Windows x64
echo ============================================

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Build zlib
echo.
echo [1/4] Building zlib...
cd dependencies\zlib
if exist build rd /s /q build
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DZLIB_BUILD_TESTING=OFF ^
    ..
cmake --build . --config %BUILD_TYPE% --parallel
copy /Y %BUILD_TYPE%\zlib.dll ..\..\..\%OUTPUT_DIR%\
copy /Y %BUILD_TYPE%\zlib.lib ..\..\..\%OUTPUT_DIR%\
cd ..\..\..

REM Build libjpeg-turbo
echo.
echo [2/4] Building libjpeg-turbo...
cd dependencies\libjpeg-turbo
if exist build rd /s /q build
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DENABLE_SHARED=ON ^
    -DENABLE_STATIC=OFF ^
    -DWITH_TURBOJPEG=ON ^
    ..
cmake --build . --config %BUILD_TYPE% --parallel
copy /Y %BUILD_TYPE%\jpeg62.dll ..\..\..\%OUTPUT_DIR%\
copy /Y %BUILD_TYPE%\jpeg.lib ..\..\..\%OUTPUT_DIR%\
copy /Y %BUILD_TYPE%\turbojpeg.dll ..\..\..\%OUTPUT_DIR%\
copy /Y %BUILD_TYPE%\turbojpeg.lib ..\..\..\%OUTPUT_DIR%\
REM Copy generated headers
copy /Y jconfig.h ..\
copy /Y jconfigint.h ..\
cd ..\..\..

REM Build LCMS2
echo.
echo [3/4] Building LCMS2...
cd dependencies\lcms2
if exist build rd /s /q build
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DBUILD_SHARED_LIBS=ON ^
    ..
cmake --build . --config %BUILD_TYPE% --parallel
copy /Y %BUILD_TYPE%\lcms2.dll ..\..\..\%OUTPUT_DIR%\
copy /Y %BUILD_TYPE%\lcms2.lib ..\..\..\%OUTPUT_DIR%\
cd ..\..\..

REM Build JasPer
echo.
echo [4/4] Building JasPer...
cd dependencies
if exist jasper_build rd /s /q jasper_build
mkdir jasper_build && cd jasper_build
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DJAS_ENABLE_SHARED=ON ^
    -DJAS_ENABLE_PROGRAMS=OFF ^
    -DJAS_ENABLE_DOC=OFF ^
    ..\jasper
cmake --build . --config %BUILD_TYPE% --parallel
copy /Y src\libjasper\%BUILD_TYPE%\jasper.dll ..\..\%OUTPUT_DIR%\
copy /Y src\libjasper\%BUILD_TYPE%\jasper.lib ..\..\%OUTPUT_DIR%\
cd ..\..

REM Build LibRaw
echo.
echo ============================================
echo Building LibRaw...
echo ============================================
if exist %BUILD_DIR% rd /s /q %BUILD_DIR%
mkdir %BUILD_DIR% && cd %BUILD_DIR%

REM Set paths to dependencies
set ZLIB_ROOT=%CD%\..\dependencies\zlib\build
set JPEG_ROOT=%CD%\..\dependencies\libjpeg-turbo\build
set LCMS2_ROOT=%CD%\..\dependencies\lcms2\build

cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DBUILD_SHARED_LIBS=ON ^
    -DLIBRAW_ENABLE_ZLIB=ON ^
    -DLIBRAW_ENABLE_JPEG=ON ^
    -DLIBRAW_ENABLE_LCMS2=ON ^
    -DLIBRAW_ENABLE_X3FTOOLS=ON ^
    -DZLIB_INCLUDE_DIR="%ZLIB_ROOT%;%CD%\..\dependencies\zlib" ^
    -DZLIB_LIBRARY="%ZLIB_ROOT%\%BUILD_TYPE%\zlib.lib" ^
    -DJPEG_INCLUDE_DIR="%JPEG_ROOT%;%CD%\..\dependencies\libjpeg-turbo" ^
    -DJPEG_LIBRARY="%JPEG_ROOT%\%BUILD_TYPE%\jpeg.lib" ^
    ..

cmake --build . --config %BUILD_TYPE% --parallel

copy /Y bin\%BUILD_TYPE%\raw.dll ..\%OUTPUT_DIR%\libraw.dll
copy /Y lib\%BUILD_TYPE%\raw.lib ..\%OUTPUT_DIR%\libraw.lib
cd ..

echo.
echo ============================================
echo Build Complete!
echo ============================================
echo Output files in: %OUTPUT_DIR%
dir %OUTPUT_DIR%\*.dll

endlocal
