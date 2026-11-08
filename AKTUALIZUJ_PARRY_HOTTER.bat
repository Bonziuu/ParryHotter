@echo off
chcp 65001 >nul
cd /d "%~dp0"
title Parry Hotter - automatyczna aktualizacja i start
echo Pobieranie i instalowanie aktualnej DLL z GitHub...
py -3 "%~dp0tools\update_hud.py" --game "C:\Programy\Steam\steamapps\common\Gothic" --run %*
if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Aktualizacja nie powiodla sie. Przeczytaj komunikat powyzej.
  pause
  exit /b 1
)
