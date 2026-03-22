rem :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.
setlocal EnableDelayedExpansion

if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=Release
set makemore=%~2

for %%d in (%SYSTEMDRIVE%\mingw32 %SYSTEMDRIVE%\msys64\mingw32 "") do (
  if not exist "!MINGW32!\bin\gcc.exe" set MINGW32=%%~d
)
for %%d in (%SYSTEMDRIVE%\mingw64 %SYSTEMDRIVE%\msys64\mingw64 "") do (
  if not exist "!MINGW64!\bin\gcc.exe" set MINGW64=%%~d
)
for %%d in (%SYSTEMDRIVE%\msys64\usr\bin %SYSTEMDRIVE%\cygwin64\bin "%PROGRAMFILES%\Git\usr\bin" "") do (
  if not exist "!posix_shell!\grep.exe" set posix_shell=%%~d
)

set ORIGINAL_PATH=%PATH%

cd /d "%~dp0"

:x86
set PATH=%MINGW32%\bin;%posix_shell%;%ORIGINAL_PATH%

echo -------------------------------------------------------------------
echo %config% (x86)
echo -------------------------------------------------------------------
mingw32-make.exe CONFIG=%config% "OUTDIR=%~dp0bin/%config%-mingw-Win32" -fMakefile.mingw all %makemore% || pause && exit /b !errorlevel!

:amd64
set PATH=%MINGW64%\bin;%posix_shell%;%ORIGINAL_PATH%

echo -------------------------------------------------------------------
echo %config% (x64)
echo -------------------------------------------------------------------
mingw32-make.exe CONFIG=%config% "OUTDIR=%~dp0bin/%config%-mingw-x64" -fMakefile.mingw all %makemore% || pause && exit /b !errorlevel!

rem pause
