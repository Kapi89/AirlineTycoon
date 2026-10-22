@echo off
rem HD_generate.bat - generates the HD graphics from your own copy of the game. A double-click is enough.
rem Lives next to AT.exe in the game folder. Steps: hd_tools\HD_generate.ps1, log: hd_generate.log.
rem Stop with Ctrl+C; double-clicking again continues where it stopped.
setlocal
title Generate HD graphics for Airline Tycoon Deluxe
set "SKRIPT=%~dp0hd_tools\HD_generate.ps1"
if not exist "%SKRIPT%" set "SKRIPT=%~dp0HD_generate.ps1"
if not exist "%SKRIPT%" (
    echo The folder hd_tools is missing next to HD_generate.bat.
    echo Please unpack the complete release archive into the game folder.
    pause
    exit /b 1
)
rem "%~dp0." instead of "%~dp0": a trailing backslash before the quote would escape it
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SKRIPT%" -Spiel "%~dp0." %*
set RC=%ERRORLEVEL%
if "%RC%"=="9009" (
    echo PowerShell was not found. It is part of Windows 10 and 11.
    pause
)
exit /b %RC%
