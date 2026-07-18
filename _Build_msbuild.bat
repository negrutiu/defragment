REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
setlocal EnableDelayedExpansion
echo.

cd /d "%~dp0"

if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=Release
if "%platforms%" equ "" set platforms=%~2
if "%platforms%" equ "" set platforms=Win32,x64,arm64

for /f "delims=*" %%i in ('dir /b /od *.sln 2^> nul') do set solution=%%~fi

rem | ------------------------------------------------------------

if not exist "%vswhere%" set vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vswhere%" set vswhere=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vswhere%" echo ERROR: Missing "vswhere.exe"&& pause && exit /b 1

if not exist "%vcvarsall%" for /f "delims=*" %%i in ('"%vswhere%" -version [18^,19^) -prerelease -requires Microsoft.Component.MSBuild -property installationPath 2^> nul') do set vcvarsall=%%i\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v145
if not exist "%vcvarsall%" for /f "delims=*" %%i in ('"%vswhere%" -version [17^,18^) -prerelease -requires Microsoft.Component.MSBuild -property installationPath 2^> nul') do set vcvarsall=%%i\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v143
if not exist "%vcvarsall%" for /f "delims=*" %%i in ('"%vswhere%" -version [16^,17^) -prerelease -requires Microsoft.Component.MSBuild -property installationPath 2^> nul') do set vcvarsall=%%i\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v142
if not exist "%vcvarsall%" for /f "delims=*" %%i in ('"%vswhere%" -version [15^,16^) -prerelease -requires Microsoft.Component.MSBuild -property installationPath 2^> nul') do set vcvarsall=%%i\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v141
if not exist "%vcvarsall%" echo ERROR: Missing "Visual Studio 2017-2026"&& pause && exit /b 2

rem | ------------------------------------------------------------

pushd "%cd%"
call "%vcvarsall%" x86
popd

echo %platforms% | findstr /i Win32 > nul && title %config%-msbuild-x86&&   (msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=Win32 /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!)
echo %platforms% | findstr /i x64   > nul && title %config%-msbuild-amd64&& (msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=x64   /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!)
echo %platforms% | findstr /i ARM64 > nul && title %config%-msbuild-arm64&& (msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=ARM64 /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!)

rem note: this script is called by the ci/cd pipeline, it must not pause
rem pause