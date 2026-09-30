FlickImp for macOS
==================

Requirements: macOS 11 (Big Sur) or later, Apple Silicon or Intel.

Install
-------
1. Unpack the archive (double-click the .zip, or: tar xzf flickimp-*-macOS.tar.gz)
2. Open Terminal in the unpacked folder and run:

       ./install.sh

   The installer asks for your TMDB API Read Access Token (free from
   https://www.themoviedb.org/settings/api) and a port (default 8647),
   starts FlickImp, and registers it to start automatically at login.
   No administrator password is needed.

3. Browse to http://localhost:8647

Upgrading: unpack the new version and run ./install.sh again.
Your config and database are kept.

Where things live
-----------------
  Program:   ~/Library/Application Support/flickimp/app/
  Config:    ~/Library/Application Support/flickimp/fi_config.json
  Database:  ~/Library/Application Support/flickimp/flickimp.db
  Log:       ~/Library/Logs/flickimp.log
  Login item ~/Library/LaunchAgents/com.nutball-labs.flickimp.plist

Edit fi_config.json to change the port or TMDB credentials, then restart:

    launchctl kickstart -k gui/$(id -u)/com.nutball-labs.flickimp

Check for new episodes from the command line:

    ~/Library/Application\ Support/flickimp/app/flickimp --check

Uninstall
---------
    ~/Library/Application\ Support/flickimp/app/uninstall.sh          (keeps your data)
    ~/Library/Application\ Support/flickimp/app/uninstall.sh --purge  (deletes it too)

Notes
-----
FlickImp is not notarized by Apple. install.sh clears the download
quarantine flag so macOS will run it. If macOS still blocks it, open
System Settings > Privacy & Security and choose "Allow Anyway".

https://github.com/Nutball-Labs/FlickImp
