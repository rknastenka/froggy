# ==============================================================================
# Install.ps1
# One-click installer for WindToDo (Froggy).
# Imports the signing certificate into Trusted People, then installs the MSIX.
#
# Usage:  Right-click → "Run with PowerShell"
#    or:  powershell -ExecutionPolicy Bypass -File Install.ps1
# ==============================================================================

#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Locate files next to this script ---
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

$cer = Get-ChildItem -Path $scriptDir -Filter '*.cer' | Select-Object -First 1
$msix = Get-ChildItem -Path $scriptDir -Filter '*.msix' | Select-Object -First 1

if (-not $cer) {
    Write-Error "No .cer certificate file found in $scriptDir"
    exit 1
}
if (-not $msix) {
    Write-Error "No .msix package found in $scriptDir"
    exit 1
}

# --- Import the certificate (requires elevation) ---
$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Requesting administrator privileges to import the certificate..." -ForegroundColor Yellow
    $args = "-ExecutionPolicy Bypass -File `"$($MyInvocation.MyCommand.Definition)`""
    Start-Process powershell -Verb RunAs -ArgumentList $args
    exit
}

Write-Host ""
Write-Host "=== WindToDo (Froggy) Installer ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Importing certificate: $($cer.Name) ..." -ForegroundColor White
Import-Certificate -FilePath $cer.FullName -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
Write-Host "  Certificate imported to Trusted People store." -ForegroundColor Green

# --- Install the MSIX package ---
Write-Host "Installing package: $($msix.Name) ..." -ForegroundColor White
Add-AppxPackage -Path $msix.FullName
Write-Host "  Package installed successfully!" -ForegroundColor Green

Write-Host ""
Write-Host "Done! You can now launch Froggy from the Start menu." -ForegroundColor Cyan
Write-Host ""
Read-Host "Press Enter to close"
