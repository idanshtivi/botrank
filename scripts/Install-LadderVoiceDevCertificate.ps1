$ErrorActionPreference = 'Stop'

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

Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\TrustedPublisher | Out-Null
Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\Root | Out-Null

Write-Host "Installed Ladder Voice local development code-signing certificate:"
Write-Host "  Subject: $($cert.Subject)"
Write-Host "  Thumbprint: $($cert.Thumbprint)"
Write-Host "  CER: $cerPath"
Write-Host "  PFX: $pfxPath"
