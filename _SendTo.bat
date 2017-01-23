@echo off

cd /d "%~dp0"

REM :: Defrag.exe
if %PROCESSOR_ARCHITECTURE% equ AMD64 set DEFRAG=x64\Defrag.exe
if %PROCESSOR_ARCHITECTURE% neq AMD64 set DEFRAG=x86\Defrag.exe
if not exist "%DEFRAG%" echo ERROR: %DEFRAG% not found && pause && goto :EOF

REM :: Rerun ourselves elevated (using PowerShell)
REM :: Triple all quotes to be parsed correctly ("=""")
set _Arg=%*
if "%~1" neq "" set _Arg=%_Arg:"="""%

net file 1>nul 2>nul && goto :ELEVATED || powershell -ex unrestricted -Command "Start-Process -Verb RunAs -FilePath '%comspec%' -ArgumentList '/c """"""%~fnx0""" %_Arg%"""'"
goto :EOF


:ELEVATED
cd /D "%~dp0"
"%DEFRAG%" /defrag /prompt %*

REM pause