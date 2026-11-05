@echo off
chcp 65001 >nul
cd /d "%~dp0"
title Parry Hotter - budowanie pliku MOD
echo Budowanie ParryHotter.mod z lokalnie skompilowanych skryptow...
echo To polecenie wykonaj po uruchomieniu GothicStarter_mod z Reparse Scripts.
py -3 "%~dp0tools\package_mod.py" --game "C:\Programy\Steam\steamapps\common\Gothic"
if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Nie zbudowano moda - sprawdz powyzszy komunikat.
  pause
  exit /b 1
)
echo.
echo Gotowe! Wybierz Parry Hotter (PLAY) w GothicStarter.
pause
