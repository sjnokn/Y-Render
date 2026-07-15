$ErrorActionPreference = "Stop"

function Invoke-CheckedBuild {
    param([string]$Configuration)

    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "$Configuration build failed with exit code $LASTEXITCODE."
    }
}

Invoke-CheckedBuild -Configuration Debug
Invoke-CheckedBuild -Configuration Release

if (!(Test-Path .\build\bin\YRender.exe)) {
    throw "YRender.exe was not produced."
}

Write-Host "Verification passed: Debug and Release builds completed."
