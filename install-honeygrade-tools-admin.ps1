# Run from Administrator PowerShell:
#   powershell -ExecutionPolicy Bypass -File "H:\engRel\ai_assisted\honey_quality_assessment\install-honeygrade-tools-admin.ps1"

$ErrorActionPreference = "Continue"

function Install-WinGetPackage {
    param(
        [Parameter(Mandatory=$true)][string]$Id,
        [Parameter(Mandatory=$true)][string]$Name,
        [switch]$Interactive
    )

    Write-Host ""
    Write-Host "==== $Name ===="
    if ($Interactive) {
        winget install --id $Id --exact --source winget --accept-package-agreements --accept-source-agreements
    } else {
        winget install --id $Id --exact --source winget --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "$Name may not have installed cleanly. Exit code: $LASTEXITCODE"
    }
}

function Add-MachinePath {
    param([Parameter(Mandatory=$true)][string]$PathToAdd)

    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if (($machinePath -split ";") -notcontains $PathToAdd) {
        [Environment]::SetEnvironmentVariable("Path", ($machinePath.TrimEnd(";") + ";" + $PathToAdd).TrimStart(";"), "Machine")
    }
    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
}

Write-Host "Installing HoneyGrade development tools..."
Write-Host "PostgreSQL may open an installer window. If it asks for a postgres password, use: postgres"

Install-WinGetPackage -Id "Git.Git" -Name "Git"
Install-WinGetPackage -Id "Python.Python.3.12" -Name "Python 3.12"
Install-WinGetPackage -Id "OpenJS.NodeJS.LTS" -Name "Node.js LTS"
Install-WinGetPackage -Id "PostgreSQL.PostgreSQL.17" -Name "PostgreSQL 17" -Interactive
Install-WinGetPackage -Id "EclipseAdoptium.Temurin.21.JDK" -Name "Java JDK 21"
Install-WinGetPackage -Id "Google.AndroidStudio" -Name "Android Studio"
Install-WinGetPackage -Id "Google.PlatformTools" -Name "Android SDK Platform Tools"
Install-WinGetPackage -Id "Kitware.CMake" -Name "CMake"
Install-WinGetPackage -Id "Ninja-build.Ninja" -Name "Ninja"
Install-WinGetPackage -Id "EclipseFoundation.Mosquitto" -Name "Mosquitto MQTT Broker"
Install-WinGetPackage -Id "Gyan.FFmpeg" -Name "FFmpeg"
Install-WinGetPackage -Id "Ngrok.Ngrok" -Name "ngrok"
Install-WinGetPackage -Id "Espressif.EIM-CLI" -Name "ESP-IDF Installation Manager CLI"

Write-Host ""
Write-Host "==== Flutter SDK ===="
$flutterRoot = "C:\dev\flutter"
if (!(Test-Path (Join-Path $flutterRoot "bin\flutter.bat"))) {
    New-Item -ItemType Directory -Force -Path "C:\dev" | Out-Null
    git clone --depth 1 -b stable https://github.com/flutter/flutter.git $flutterRoot
} else {
    Write-Host "Flutter already exists at $flutterRoot"
}
Add-MachinePath (Join-Path $flutterRoot "bin")

Write-Host ""
Write-Host "==== Python helper packages ===="
$python = Get-Command py -ErrorAction SilentlyContinue
if ($python) {
    py -3.12 -m ensurepip --upgrade
    py -3.12 -m pip install --upgrade pip virtualenv
}

Write-Host ""
Write-Host "==== Final quick checks ===="
$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
$tools = @(
    "git", "python", "py", "pip", "node", "npm",
    "psql", "pg_ctl", "java", "javac",
    "flutter", "dart", "adb",
    "cmake", "ninja", "mosquitto", "ffmpeg", "ngrok", "eim"
)

foreach ($tool in $tools) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue
    if ($cmd) {
        Write-Host "OK      $tool -> $($cmd.Source)"
    } else {
        Write-Host "MISSING $tool"
    }
}

Write-Host ""
Write-Host "Done. Restart PowerShell and VS Code after this finishes."
Write-Host "Then tell Codex: tools installed, continue waking HoneyGrade."
