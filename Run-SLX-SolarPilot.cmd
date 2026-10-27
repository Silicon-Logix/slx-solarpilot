@echo off
rem Silicon LogiX / SLX SolarPilot
rem Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
rem License: Silicon LogiX Evaluation License 1.0. See LICENSE.
cd /d "%~dp0"
if exist "SLX-SolarPilot.exe" (
  start "" "SLX-SolarPilot.exe"
  exit /b 0
)
if exist "bin\SLX-SolarPilot.exe" (
  start "" "bin\SLX-SolarPilot.exe" --root "%~dp0."
  exit /b 0
)
if exist "build\ucrt\bin\SLX-SolarPilot.exe" (
  start "" "build\ucrt\bin\SLX-SolarPilot.exe" --root "%~dp0."
  exit /b 0
)
echo Eseguire prima: powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
pause
