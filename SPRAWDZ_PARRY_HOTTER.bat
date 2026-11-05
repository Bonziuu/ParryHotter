@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
title Parry Hotter - sprawdzenie instalacji
echo.
echo ===== SPRAWDZENIE PARRY HOTTER =====
echo.
py -3 "%~dp0tools\doctor.py" --game "C:\Programy\Steam\steamapps\common\Gothic" --out "%~dp0ParryHotter_diagnostics.txt"
if %ERRORLEVEL% NEQ 0 (
  echo Nie mozna przygotowac raportu. Sprawdz czy Python jest zainstalowany.
)
echo.
echo Raport jest w pliku ParryHotter_diagnostics.txt obok instalatora.
pause
endlocal
