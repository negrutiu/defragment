@echo off

cd /d "%~dp0"

set DIR32=objfre_w2k_x86\i386
set DIR64=objfre_wnet_amd64\amd64
REM set DIR32=Release-mingw-Win32
REM set DIR64=Release-mingw-x64

if not exist "Defragment" mkdir "Defragment"
if not exist "Defragment\x86" mkdir "Defragment\x86"
if not exist "Defragment\x64" mkdir "Defragment\x64"

echo on
copy /Y "%DIR32%\Defrag.exe" "Defragment\x86\"
copy /Y "%DIR64%\Defrag.exe" "Defragment\x64\"
REM copy /Y "_SendTo.bat" "Defragment\"
@echo off
echo.

call "..\CodeSigning\Marius Negrutiu\sha1\Sign.bat" "Defragment\x86\*.exe" "Defragment\x64\*.exe"
if %ERRORLEVEL% neq 0 pause

pause