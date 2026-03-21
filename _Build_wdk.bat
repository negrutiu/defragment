REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.
setlocal EnableDelayedExpansion

cd /d "%~dp0"

:DEFINITIONS
if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=fre

:COMPILER
set WDK6=C:\WDK6
set WDK7=C:\WDK7
if not exist "%WDK6%" echo ERROR: Missing %WDK6% && pause && exit /b 2
if not exist "%WDK7%" echo ERROR: Missing %WDK7% && pause && exit /b 2

:BUILD
%comspec% /c "call %WDK6%\bin\setenv.bat %WDK6%\ %config% x86 W2K && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0500 && build -gcewZ && echo." || pause && exit /b !errorlevel!
::%comspec% /c "call %WDK7%\bin\setenv.bat %WDK7%\ %config% x86 WXP no_oacr && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0501 && build -gcewZ && echo." || pause && exit /b !errorlevel!

%comspec% /c "call %WDK7%\bin\setenv.bat %WDK7%\ %config% x64 WNET no_oacr && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0502 && build -gcewZ && echo." || pause && exit /b !errorlevel!

rem pause