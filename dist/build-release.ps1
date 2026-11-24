# 生成发布包：TaskJournal.exe / 便携版 zip / 安装包
# 用法： powershell -ExecutionPolicy Bypass -File dist\build-release.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$proj = Join-Path $root 'poject'
$dist = $PSScriptRoot
$build = Join-Path $root 'build'
$gpp = 'C:\Users\is\Desktop\p3\mingw64\mingw64\bin\g++.exe'
$windres = 'C:\Users\is\Desktop\p3\mingw64\mingw64\bin\windres.exe'

New-Item -ItemType Directory -Force $build, (Join-Path $proj 'assets') | Out-Null
Copy-Item (Join-Path $build 'app.ico') (Join-Path $proj 'assets\app.ico') -Force

# 1. 主程序（图标 + 版本信息 + 静态链接）
Push-Location $proj
& $windres app.rc -O coff -o build_app_res.o
& $gpp -std=c++17 -O2 -s -static server.cpp task.cpp build_app_res.o `
       -o (Join-Path $build 'TaskJournal.exe') -lws2_32 -lshell32
Pop-Location

# 2. 便携版
$pkg = Join-Path $dist 'package'
New-Item -ItemType Directory -Force $pkg | Out-Null
Copy-Item (Join-Path $build 'TaskJournal.exe') $pkg -Force
Copy-Item (Join-Path $dist '使用说明.txt') $pkg -Force
$zip = Join-Path $dist 'TaskJournal-1.0.0-portable.zip'
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $pkg '*') -DestinationPath $zip -Force

# 3. 安装包（把主程序作为资源嵌进去）
$ins = Join-Path $dist 'installer'
Copy-Item (Join-Path $build 'TaskJournal.exe') $ins -Force
Copy-Item (Join-Path $build 'app.ico') $ins -Force
Copy-Item (Join-Path $dist '使用说明.txt') (Join-Path $ins 'readme.txt') -Force
Push-Location $ins
& $windres installer.rc -O coff -o installer_res.o
& $gpp -std=c++17 -O2 -s -static installer.cpp installer_res.o `
       -o (Join-Path $dist 'TaskJournal-1.0.0-Setup.exe') `
       -lole32 -luuid -lshell32 -lshlwapi -ladvapi32
Pop-Location

Write-Host '完成：'
Get-ChildItem $dist -File | Select-Object Name, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 2) } }
