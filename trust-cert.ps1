param(
    [Parameter(Mandatory)]
    [string]$Thumbprint
)

if ($Thumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
    Write-Error "Invalid thumbprint format. Expected 40 hex characters."
    return
}

$certPath = Join-Path ([System.IO.Path]::GetTempPath()) 'KrispDev.cer'
Export-Certificate -Cert "Cert:\CurrentUser\My\$Thumbprint" -FilePath $certPath | Out-Null
Import-Certificate -FilePath $certPath -CertStoreLocation 'Cert:\CurrentUser\TrustedPeople' | Format-List Subject, Thumbprint
Remove-Item $certPath -ErrorAction SilentlyContinue
Write-Host "Certificate trusted successfully."
