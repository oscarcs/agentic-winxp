@echo off
set TCC=C:\tcc\tcc.exe
set DEF=C:\tcc\lib\ws2_32.def
set DLL=C:\WINDOWS\system32\ws2_32.dll

if not exist %TCC% goto missing_tcc

if exist %DEF% goto build
echo Creating %DEF%
%TCC% -impdef %DLL% -o %DEF%
if errorlevel 1 goto failed

:build
%TCC% netcheck.c -o netcheck.exe -lws2_32
if errorlevel 1 goto failed

echo Built netcheck.exe
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

