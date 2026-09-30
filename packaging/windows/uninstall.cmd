@echo off
rem uninstall.cmd -- double-click to remove FlickImp (keeps your config and database).
rem For a full wipe: uninstall.cmd -Purge
rem Copies itself to %TEMP% first, because uninstall deletes the folder it lives in.
copy /y "%~dp0uninstall.ps1" "%TEMP%\flickimp-uninstall.ps1" >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TEMP%\flickimp-uninstall.ps1" %*
del "%TEMP%\flickimp-uninstall.ps1" >nul 2>&1
echo.
pause
rem SN: 00005
