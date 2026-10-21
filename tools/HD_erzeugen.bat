@echo off
rem HD_erzeugen.bat - erzeugt die HD-Grafiken aus der eigenen Spielkopie. Doppelklick genuegt.
rem Liegt neben AT.exe im Spielordner. Ablauf: hd_tools\HD_erzeugen.ps1, Protokoll: hd_erzeugen.log.
rem Abbrechen mit Strg+C; ein neuer Doppelklick setzt fort.
setlocal
title HD-Grafiken fuer Airline Tycoon Deluxe erzeugen
set "SKRIPT=%~dp0hd_tools\HD_erzeugen.ps1"
if not exist "%SKRIPT%" set "SKRIPT=%~dp0HD_erzeugen.ps1"
if not exist "%SKRIPT%" (
    echo Der Ordner hd_tools fehlt neben HD_erzeugen.bat.
    echo Bitte das Release-Archiv vollstaendig in den Spielordner entpacken.
    pause
    exit /b 1
)
rem "%~dp0." statt "%~dp0": ein abschliessender Backslash vor dem Anfuehrungszeichen wuerde es maskieren
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SKRIPT%" -Spiel "%~dp0." %*
set RC=%ERRORLEVEL%
if "%RC%"=="9009" (
    echo PowerShell wurde nicht gefunden. Es gehoert zu Windows 10 und 11.
    pause
)
exit /b %RC%
