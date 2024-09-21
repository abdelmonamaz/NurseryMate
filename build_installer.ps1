<#
.SYNOPSIS
    Genere l'installateur Windows de Nursera (Release + windeployqt + Inno Setup).
.NOTES
    Prerequis : Qt 6.11 MSVC 2022 (C:\Qt\6.11.0\msvc2022_64), CMake, Inno Setup 6.
    Le generateur Visual Studio du preset desktop-msvc ne requiert pas vcvars.
#>

$ErrorActionPreference = "Stop"
$env:PATH = "C:\Qt\6.11.0\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;$env:PATH"

$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=== Verification des prerequis ===" -ForegroundColor Cyan
if (-Not (Get-Command "cmake" -ErrorAction SilentlyContinue)) { Write-Error "CMake introuvable." }
if (-Not (Get-Command "windeployqt" -ErrorAction SilentlyContinue)) { Write-Error "windeployqt introuvable." }
$InnoSetup = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-Not $InnoSetup) { Write-Error "Inno Setup 6 (ISCC.exe) introuvable." }

# Version : dernier tag git vX.Y.Z (repris aussi par CMake), repli 0.1.0.
# NB : pas de redirection 2>$null sur git (NativeCommandError en PS 5.1).
$Version = "0.1.0"
$tag = git tag --list "v*" --sort=-v:refname | Select-Object -First 1
if ($tag) { $Version = $tag.TrimStart("v") }
Write-Host "Version : $Version" -ForegroundColor Green

Write-Host "`n=== Configuration + compilation Release (preset desktop-msvc) ===" -ForegroundColor Cyan
cmake --preset desktop-msvc
if ($LASTEXITCODE -ne 0) { Write-Error "Configuration CMake echouee." }
cmake --build --preset desktop-release --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation Release echouee." }

$ExePath = "$ProjectRoot\build\desktop-msvc\apps\desktop\Release\nursera-desktop.exe"
if (-Not (Test-Path $ExePath)) { Write-Error "nursera-desktop.exe introuvable apres compilation." }

Write-Host "`n=== Preparation du dossier deploy ===" -ForegroundColor Cyan
$DeployDir = "$ProjectRoot\deploy"
if (Test-Path $DeployDir) { Remove-Item -Recurse -Force $DeployDir }
New-Item -ItemType Directory -Path $DeployDir | Out-Null
Copy-Item $ExePath -Destination "$DeployDir\nursera-desktop.exe"
Copy-Item "$ProjectRoot\resources\logo.ico" -Destination "$DeployDir\logo.ico"

Write-Host "`n=== Deploiement des dependances Qt (windeployqt) ===" -ForegroundColor Cyan
# Tout le QML applicatif est compile dans l'exe (qt_add_qml_module) ;
# --qmldir couvre les imports QtQuick/Controls a embarquer.
windeployqt --qmldir "$ProjectRoot\apps\desktop" --qmldir "$ProjectRoot\libs\ui-common" `
    --release --no-translations "$DeployDir\nursera-desktop.exe"
if ($LASTEXITCODE -ne 0) { Write-Error "windeployqt a echoue." }

Write-Host "`n=== Generation de l'installateur (Inno Setup) ===" -ForegroundColor Cyan
& $InnoSetup "/DAppVer=$Version" "$ProjectRoot\installer.iss"
if ($LASTEXITCODE -ne 0) { Write-Error "Creation de l'installateur echouee." }

$Installer = Get-Item "$ProjectRoot\Output\Nursera_Installer_$Version.exe"
Write-Host "`n=== SUCCES : $($Installer.FullName) ($([math]::Round($Installer.Length / 1MB, 1)) Mo) ===" -ForegroundColor Green
