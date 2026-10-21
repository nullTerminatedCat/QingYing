# 青影 Release x64 构建脚本
param(
    [string]$QtDir = 'F:/Qt/6.9.1/msvc2022_64',
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmd = "`"$vcvars`" >nul && cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QtDir && cmake --build $BuildDir --config Release"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Write-Host "Built: $root\$BuildDir\QingYing.exe"
