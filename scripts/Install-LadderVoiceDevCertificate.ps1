$ErrorActionPreference = 'Stop'

# LocalMachine store writes require elevation.
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Elevating to Administrator (required for LocalMachine certificate stores)..."
    Start-Process powershell.exe -Verb RunAs `
        -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit 0
}

$subjectName = 'Ladder Voice Local Dev'
$subject = "CN=$subjectName"
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $subject -and $_.EnhancedKeyUsageList.FriendlyName -contains 'Code Signing' } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $cert) {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $subject `
        -CertStoreLocation Cert:\CurrentUser\My `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable `
        -NotAfter (Get-Date).AddYears(3)
}

$exportDir = Join-Path $PSScriptRoot '..\trusted_bin'
New-Item -ItemType Directory -Path $exportDir -Force | Out-Null
$cerPath = Join-Path $exportDir 'LadderVoiceLocalDev.cer'
$pfxPath = Join-Path $exportDir 'LadderVoiceLocalDev.pfx'
$pfxPassword = ConvertTo-SecureString -String 'LadderVoiceLocalDev' -Force -AsPlainText
Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pfxPassword -Force | Out-Null

# CurrentUser stores — for this user's process-level certificate validation.
Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\TrustedPublisher | Out-Null
Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\Root | Out-Null

# LocalMachine stores — required for Smart App Control and SmartScreen, which
# run at system level and do not see CurrentUser stores.
Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null
Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null

Write-Host "Installed Ladder Voice local development code-signing certificate:"
Write-Host "  Subject: $($cert.Subject)"
Write-Host "  Thumbprint: $($cert.Thumbprint)"
Write-Host "  CER: $cerPath"
Write-Host "  PFX: $pfxPath"
Write-Host "  Stores: CurrentUser\TrustedPublisher, CurrentUser\Root,"
Write-Host "          LocalMachine\TrustedPublisher, LocalMachine\Root"
