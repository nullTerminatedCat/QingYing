# 打包青影：生成 setup.exe、portable.zip 与 SHA256SUMS
param(
    [string]$BuildDirectory = 'build',
    [string]$RuntimeDirectory = '',
    [string]$Iscc = '',
    [string]$OutputDirectory = 'dist'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$version = ((Select-String -Path CMakeLists.txt -Pattern 'project\(QingYing VERSION ([0-9.]+)').Matches[0].Groups[1].Value)
$arch = 'x64'
$name = "QingYing-$version-windows-$arch"
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
$zip = Join-Path $out "$name-portable.zip"
$setup = Join-Path $out "$name-setup.exe"
foreach ($f in @($zip, $setup)) { if (Test-Path $f) { Remove-Item $f } }

# VC++ 运行库
if (!$RuntimeDirectory) {
    # 运行库版本必须不低于编译器版本：优先 VCToolsRedistDir，否则取本机最高版本的 VC14x CRT
    $roots = @()
    if ($env:VCToolsRedistDir) { $roots += $env:VCToolsRedistDir }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        foreach ($vs in (& $vswhere -all -products * -property installationPath)) {
            $msvc = Join-Path $vs 'VC\Redist\MSVC'
            if (Test-Path $msvc) { $roots += (Get-ChildItem $msvc -Directory | Where-Object Name -match '^\d+\.\d+\.\d+$').FullName }
        }
    }
    $RuntimeDirectory = $roots | ForEach-Object { Get-ChildItem (Join-Path $_ 'x64') -Directory -Filter 'Microsoft.VC14*.CRT' -ErrorAction SilentlyContinue } |
        Where-Object { Test-Path (Join-Path $_.FullName 'vcruntime140.dll') } |
        Sort-Object { [version](Get-Item (Join-Path $_.FullName 'vcruntime140.dll')).VersionInfo.FileVersionRaw } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if (!$RuntimeDirectory) { throw 'VC++ runtime directory not found; pass -RuntimeDirectory' }
}
Write-Host "VC++ runtime: $RuntimeDirectory"
foreach ($r in 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
    if (!(Test-Path (Join-Path $RuntimeDirectory $r))) { throw "Missing VC++ runtime: $r in $RuntimeDirectory" }
}

# 组装文件（白名单）
$stage = Join-Path $out ("staging-" + [Guid]::NewGuid().ToString('N'))
$payload = Join-Path $stage $name
New-Item -ItemType Directory -Force $payload | Out-Null
Copy-Item (Join-Path $BuildDirectory 'QingYing.exe') $payload
Get-ChildItem $BuildDirectory -Filter 'Qt6*.dll' | Copy-Item -Destination $payload
foreach ($f in 'd3dcompiler_47.dll', 'opengl32sw.dll') {
    $p = Join-Path $BuildDirectory $f; if (Test-Path $p) { Copy-Item $p $payload }
}
foreach ($d in 'platforms', 'styles', 'imageformats', 'iconengines', 'generic', 'tls', 'networkinformation') {
    $p = Join-Path $BuildDirectory $d; if (Test-Path $p) { Copy-Item $p $payload -Recurse }
}
if (!(Test-Path (Join-Path $payload 'platforms\qwindows.dll'))) { throw 'Missing Qt platform plugin' }
Copy-Item (Join-Path $RuntimeDirectory '*.dll') $payload
Copy-Item LICENSE, THIRD_PARTY_NOTICES.md, README.md $payload
$rev = (git rev-parse HEAD).Trim()
"Version: $version`r`nArchitecture: $arch`r`nSource revision: $rev" | Set-Content (Join-Path $payload 'BUILD.txt') -Encoding UTF8

# 检查所有二进制均为 x64
foreach ($bin in (Get-ChildItem $payload -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll' })) {
    $rd = [IO.BinaryReader]::new([IO.File]::OpenRead($bin.FullName))
    try { $rd.BaseStream.Position = 0x3c; $pe = $rd.ReadInt32(); $rd.BaseStream.Position = $pe + 4
          if ($rd.ReadUInt16() -ne 0x8664) { throw "Not x64: $($bin.FullName)" } } finally { $rd.Dispose() }
}

Compress-Archive -Path $payload -DestinationPath $zip -CompressionLevel Optimal

if (!$Iscc) {
    $cands = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe", 'F:\work\zip\tools\innosetup\ISCC.exe')
    $Iscc = $cands | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (!$Iscc) { throw 'Inno Setup 6 (ISCC.exe) not found' }
& $Iscc "/DAppSourceDir=$payload" "/DAppVersion=$version" "/DAppArchitecture=$arch" "/DOutputDirectory=$out" installer/QingYing.iss | Out-Host
if ($LASTEXITCODE -ne 0) { throw "ISCC failed: $LASTEXITCODE" }
Remove-Item -Recurse -Force $stage

$sums = foreach ($f in @($setup, $zip)) { "{0}  {1}" -f (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(), (Split-Path $f -Leaf) }
$sums | Set-Content (Join-Path $out "SHA256SUMS-$version-$arch.txt") -Encoding ASCII
$sums
