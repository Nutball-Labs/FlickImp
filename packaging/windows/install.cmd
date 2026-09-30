@echo off
rem install.cmd -- double-click to install FlickImp for the current user.
rem Runs install.ps1 with a one-off execution-policy bypass (scripts are
rem unsigned; Windows blocks .ps1 files by default).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
echo.
pause
rem SN: 00005
