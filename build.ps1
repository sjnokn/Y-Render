param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio 2022 with C++ desktop tools."
}

$installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if ([string]::IsNullOrWhiteSpace($installPath)) {
    throw "Visual Studio MSBuild installation was not found."
}

$msbuild = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
if (!(Test-Path $msbuild)) {
    throw "MSBuild.exe was not found at $msbuild"
}

& $msbuild ".\YRender.vcxproj" /m /p:Configuration=$Configuration /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built $Configuration build\bin\YRender.exe"
