$ErrorActionPreference = "Stop"

powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release

if (!(Test-Path .\build\bin\YRender.exe)) {
    throw "YRender.exe was not produced."
}

Write-Host "Verification passed: Debug and Release builds completed."
