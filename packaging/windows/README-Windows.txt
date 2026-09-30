FlickImp for Windows
====================

Requirements: Windows 10 (1809 or later) or Windows 11, 64-bit.

Install
-------
1. Right-click the downloaded zip -> Extract All...
2. In the extracted folder, double-click install.cmd

   The installer asks for your TMDB API Read Access Token (free from
   https://www.themoviedb.org/settings/api) and a port (default 8647),
   starts FlickImp in the background, and sets it to start automatically
   when you log in. No administrator rights are needed.

3. Browse to http://localhost:8647  (or Start Menu -> FlickImp)

Upgrading: extract the new version and run install.cmd again.
Your config and database are kept.

Where things live
-----------------
  Program:   %LOCALAPPDATA%\Programs\FlickImp\
  Config:    %APPDATA%\flickimp\fi_config.json
  Database:  %APPDATA%\flickimp\flickimp.db
  Log:       %APPDATA%\flickimp\flickimp.log
  Autostart: Task Scheduler -> Task Scheduler Library -> FlickImp

Edit fi_config.json to change the port or TMDB credentials, then restart
FlickImp (in PowerShell):

    Stop-ScheduledTask FlickImp; Start-ScheduledTask FlickImp

Check for new episodes from the command line:

    & "$env:LOCALAPPDATA\Programs\FlickImp\flickimp.exe" --check

Uninstall
---------
Double-click %LOCALAPPDATA%\Programs\FlickImp\uninstall.cmd
(keeps your data; run "uninstall.cmd -Purge" from a terminal to delete it too)

Notes
-----
- FlickImp is not code-signed. If SmartScreen shows "Windows protected your
  PC", click "More info" -> "Run anyway".
- If Windows Firewall asks about flickimp.exe: "Cancel" keeps FlickImp
  reachable only from this PC; "Allow" lets phones and other computers on
  your home network use it.

https://github.com/Nutball-Labs/FlickImp
