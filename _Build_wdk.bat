REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

:CHDIR
cd /d "%~dp0"

:DEFINITIONS
set WDK_CONFIG=fre

:COMPILER
set WDK6=C:\WDK6
set WDK7=C:\WDK7
if not exist "%WDK6%" ( echo ERROR: Can't find WDK6. && pause && goto :EOF )
if not exist "%WDK7%" ( echo ERROR: Can't find WDK7. && pause && goto :EOF )

:BUILD
%COMSPEC% /C "call %WDK6%\bin\setenv.bat %WDK6%\ %WDK_CONFIG% x86 W2K && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0500 && build -gcewZ && echo."
::%COMSPEC% /C "call %WDK7%\bin\setenv.bat %WDK7%\ %WDK_CONFIG% x86 WXP no_oacr && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0501 && build -gcewZ && echo."
if %ERRORLEVEL% neq 0 ( echo ERRORLEVEL = %ERRORLEVEL% && pause && goto :EOF )

%COMSPEC% /C "call %WDK7%\bin\setenv.bat %WDK7%\ %WDK_CONFIG% x64 WNET no_oacr && cd /d """%cd%""" && set C_DEFINES=%C_DEFINES% -D_WIN32_WINNT=0x0502 && build -gcewZ && echo."
if %ERRORLEVEL% neq 0 ( echo ERRORLEVEL = %ERRORLEVEL% && pause && goto :EOF )
