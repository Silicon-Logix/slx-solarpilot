@echo off
rem Silicon LogiX / SLX SolarPilot
rem Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
rem License: Silicon LogiX Evaluation License 1.0. See LICENSE.
cd /d "%~dp0"
echo SLX SolarPilot - EMS EEBUS di riferimento su PC
echo In Rete e dispositivi: ruolo Impianto per EMS esterno e Profilo SLX esteso.
echo Avviare EEBUS; dopo questo servizio caricare la sua identita e associarla nella GUI.
echo Ctrl+C termina il servizio.
if not exist "workspace\eebus\plant-peer.json" (
  echo Identita impianto assente. Aprire prima il simulatore e attivare il banco esterno.
  pause
  exit /b 1
)
set "SLX_RUNNER=build\ucrt\bin\slx-solarpilot-cli.exe"
if exist "bin\slx-solarpilot-cli.exe" set "SLX_RUNNER=bin\slx-solarpilot-cli.exe"
if exist "slx-solarpilot-cli.exe" set "SLX_RUNNER=slx-solarpilot-cli.exe"
if exist "workspace\bench-profile.json" (
  "%SLX_RUNNER%" --eebus-ems --peer workspace/eebus/plant-peer.json --profile workspace/bench-profile.json
) else (
  "%SLX_RUNNER%" --eebus-ems --peer workspace/eebus/plant-peer.json
)
