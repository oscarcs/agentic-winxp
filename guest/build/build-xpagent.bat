@echo off
set TCC=C:\tcc\tcc.exe
set DEF=C:\tcc\lib\ws2_32.def
set DLL=C:\WINDOWS\system32\ws2_32.dll
set OUT=%1
if "%OUT%"=="" set OUT=xpagent.exe

if not exist %TCC% goto missing_tcc

if exist %DEF% goto build
echo Creating %DEF%
%TCC% -impdef %DLL% -o %DEF%
if errorlevel 1 goto failed

:build
%TCC% src\xpagent.c ..\portable\agent_core.c -I.. -o %OUT% -lws2_32
if errorlevel 1 goto failed

echo Built %OUT%
echo Run it with: %OUT%
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
