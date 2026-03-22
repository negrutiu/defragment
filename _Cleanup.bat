REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

cd /d "%~dp0"

call :cleanup
call :cleanup
call :cleanup
exit /b 0


:cleanup
rd /s /q .vs
rd /s /q ipch

rd /s /q bin

del *.aps
del *.bak
del *.user
del *.ncb
del /AH *.suo
del *.sdf
del *.VC.db

:cleanup_wdk
for /d %%a in (src\objchk*) do rd /s /q "%%a"
for /d %%a in (src\objfre*) do rd /s /q "%%a"

del *.err
del *.wrn
del *.log
del buildfre*.*
del buildchk*.*
del prefast*.*
