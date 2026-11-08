@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
title Parry Hotter - polskie menu
where py >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo BLAD: Nie znaleziono Python Launcher py.
  pause
  exit /b 1
)
py -3 -m tools.polish_system --game "C:\Programy\Steam\steamapps\common\Gothic"
if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Spolszczenie menu nie powiodlo sie. Zrob zrzut ekranu.
  pause
  exit /b 1
)
echo.
echo Teraz w GothicStarter_mod wybierz ParryHotter.ini i zaznacz Reparse Scripts.
pause
endlocal
