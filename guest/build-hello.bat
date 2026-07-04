@echo off
set TCC=C:\tcc\tcc.exe

if not exist %TCC% goto missing_tcc

%TCC% hello.c -o hello.exe
if errorlevel 1 goto failed

echo Built hello.exe
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

