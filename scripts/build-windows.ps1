# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
param([string]$Toolchain = '', [switch]$Headless)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Toolchain) {
  & python "$PSScriptRoot/bootstrap_toolchain.py"
  if ($LASTEXITCODE) { throw 'Toolchain bootstrap failed' }
  $Toolchain = "$projectRoot/build/tools/llvm-mingw-20261020-ucrt-x86_64/bin"
}
if (-not (Test-Path -LiteralPath "$Toolchain/clang++.exe")) { throw 'Supply -Toolchain with an x64 LLVM-MinGW UCRT bin directory.' }
$env:Path = "$Toolchain;" + $env:Path
& python "$PSScriptRoot/bootstrap.py"
if ($LASTEXITCODE) { throw 'Dependency verification failed' }
$desktop = if ($Headless) { 'OFF' } else { 'ON' }
& python "$PSScriptRoot/bootstrap_eebus.py"
if ($LASTEXITCODE) { throw 'EEBUS toolchain bootstrap failed' }
& cmake -S $projectRoot -B "$projectRoot/build/ucrt" -G Ninja '-DCMAKE_BUILD_TYPE=RelWithDebInfo' "-DCMAKE_C_COMPILER=$Toolchain/clang.exe" "-DCMAKE_CXX_COMPILER=$Toolchain/clang++.exe" "-DSLX_BUILD_DESKTOP=$desktop"
if ($LASTEXITCODE) { throw 'Configure failed' }
& cmake --build "$projectRoot/build/ucrt" -j 4
if ($LASTEXITCODE) { throw 'Build failed' }
& ctest --test-dir "$projectRoot/build/ucrt" --output-on-failure
if ($LASTEXITCODE) { throw 'Verification failed' }
