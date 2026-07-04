@echo off
title xpilot
cd /d C:\agent
echo Starting xpilot at %DATE% %TIME%
xpilot.exe 10.0.2.2 7778
echo xpilot exited with %ERRORLEVEL% at %DATE% %TIME%
