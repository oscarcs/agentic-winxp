@echo off
set TCC=C:\tcc\tcc.exe
set OUT=%1
if "%OUT%"=="" set OUT=capture-screen.exe

if not exist %TCC% goto missing_tcc

%TCC% capture-screen.c -o %OUT% -luser32 -lgdi32
if errorlevel 1 goto failed

echo Built %OUT%
goto done

:missing_tcc
echo Missing C:\tcc\tcc.exe
echo Extract agent-kit.zip to C:\ first.
goto failed

:failed
echo Build failed.
exit /b 1

:done
exit /b 0
