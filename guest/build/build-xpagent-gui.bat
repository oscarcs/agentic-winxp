@echo off
set TCC=C:\tcc\tcc.exe
set OUT=%1
if "%OUT%"=="" set OUT=xpagent-gui.exe

if not exist %TCC% goto missing_tcc

%TCC% -mwindows src\xpagent-gui.c -o %OUT% -luser32 -lgdi32 -lmsimg32
if errorlevel 1 goto failed

echo Built %OUT%
echo Launch it with: start "" %OUT%
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
