REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
setlocal EnableDelayedExpansion
echo.

cd /d "%~dp0"

if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=Release

for /f "delims=*" %%i in ('dir /b /od *.sln 2^> nul') do set solution=%%~fi

rem | ------------------------------------------------------------

if not exist "%PF%" set PF=%PROGRAMFILES(X86)%
if not exist "%PF%" set PF=%PROGRAMFILES%
set VSWHERE=%PF%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 17 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v143
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 16 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v142
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 15 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v141
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 14.0\VC\VcVarsAll.bat&& set toolset=v140
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat&& set toolset=v120
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat&& set toolset=v110
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat&& set toolset=v100
if not exist "%vcvarsall%" echo ERROR: Can't find Visual Studio 2010-2022 && pause && exit /b 2

rem | ------------------------------------------------------------

pushd "%cd%"
call "%vcvarsall%" x86
popd

title %config%-msbuild-x86-unicode&&   msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=Win32 /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!
title %config%-msbuild-amd64-unicode&& msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=x64   /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!
title %config%-msbuild-arm64-unicode&& msbuild /m /t:build "%solution%" /p:Configuration=%config% /p:Platform=ARM64 /p:PlatformToolset=%toolset% || pause && exit /b !errorlevel!

rem pause