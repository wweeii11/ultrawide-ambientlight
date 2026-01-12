param (
    [switch]$Uninstall
)

$Subject = "CN=ambientlightDev"
$AppName = "ambientlight_a.exe"
$ConfigName = "config.ini"
$InstallDir = "$env:ProgramFiles\ultrawide-ambientlight"
$TargetExe = Join-Path $InstallDir $AppName
$TargetConfig = Join-Path $InstallDir $ConfigName
$ShortcutPath = "$env:AppData\Microsoft\Windows\Start Menu\Programs\AmbientLight.lnk"

# --- CLEANUP FUNCTION ---
function Remove-OldCerts {
    Write-Host "Scrubbing old certificates for $Subject..." -ForegroundColor Gray
    $Stores = "Cert:\LocalMachine\Root", "Cert:\LocalMachine\TrustedPublisher", "Cert:\CurrentUser\My"
    foreach ($StorePath in $Stores) {
        $Certs = Get-ChildItem $StorePath -ErrorAction SilentlyContinue | Where-Object { $_.Subject -eq $Subject }
        foreach ($Cert in $Certs) {
            Remove-Item $Cert.PSPath -Force
            Write-Host "  - Removed from $StorePath" -ForegroundColor Gray
        }
    }
}

# --- UNINSTALL LOGIC ---
if ($Uninstall) {
    Write-Host "Starting uninstallation..." -ForegroundColor Yellow
    Remove-OldCerts
    if (Test-Path $InstallDir) {
        Remove-Item -Recurse -Force $InstallDir
        Write-Host "Removed $InstallDir" -ForegroundColor Gray
    }
    Write-Host "Uninstallation complete." -ForegroundColor Green
    exit
}

# --- INSTALL/SETUP LOGIC ---

if (-not (Test-Path $AppName)) {
    Write-Error "Could not find $AppName in the current directory."
    exit
}

# 1. Always start with a clean slate
Remove-OldCerts

# 2. Create New Certificate
Write-Host "Creating fresh self-signed certificate..." -ForegroundColor Cyan
$Cert = New-SelfSignedCertificate -Type Custom `
    -Subject $Subject `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
    -KeyUsage DigitalSignature `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(1)

# 3. Trust the New Certificate
Write-Host "Installing trust for new certificate..." -ForegroundColor Cyan
$CertPath = Join-Path $pwd "temp_cert.cer"
Export-Certificate -Cert $Cert -FilePath $CertPath | Out-Null

# Import to Local Machine stores
Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null

Remove-Item $CertPath

# 4. Sign the Application
Write-Host "Signing $AppName..." -ForegroundColor Cyan
Set-AuthenticodeSignature -FilePath $AppName -Certificate $Cert

# 5. Deploy to Secure Location
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir | Out-Null
}

Write-Host "Deploying to $InstallDir..." -ForegroundColor Cyan
Copy-Item $AppName -Destination $TargetExe -Force
Copy-Item $ConfigName -Destination $TargetConfig -Force

# 6. Create Start Menu Shortcut
Write-Host "Creating Start Menu shortcut..." -ForegroundColor Cyan
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $TargetExe
$Shortcut.WorkingDirectory = $InstallDir
$Shortcut.Description = "Ambient Light Application"
$Shortcut.Save()

Write-Host "`nSuccess! New certificate generated and $AppName signed." -ForegroundColor Green
Write-Host "Location: $TargetExe"

