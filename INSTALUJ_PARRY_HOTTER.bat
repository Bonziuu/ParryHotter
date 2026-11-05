@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
title Parry Hotter - instalator
echo.
echo ===== PARRY HOTTER - GOTHIC 1 =====
echo.
echo Lokalizacja Gothic 1:
echo C:\Programy\Steam\steamapps\common\Gothic
echo.
where py >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo BLAD: Nie znaleziono Python Launcher 'py'.
  echo Zainstaluj Python 3 lub sprawdz instalacje uzyta w poprzednim kroku.
  pause
  exit /b 1
)
py -3 "%~dp0tools\setup.py" --game "C:\Programy\Steam\steamapps\common\Gothic" --run
if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Instalacja nie powiodla sie. Zrob zrzut ekranu z komunikatem bledu.
  pause
  exit /b 1
)
echo.
echo W GothicStarter_mod wybierz Parry Hotter (DEV) i reparse scripts.
echo Po kompilacji wybierz Nowa gra.
pause
endlocal
