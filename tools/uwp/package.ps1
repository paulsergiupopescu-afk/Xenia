# Lays out, packs and signs the UWP app for Xbox Series X|S Developer Mode.
#
# Produces in -OutDir:
#   XeniaEdge_<version>_x64.msix   the app, signed with a self-signed certificate
#   XeniaEdge.cer                  that certificate (only needed on a PC)
#   Dependencies\x64\Microsoft.VCLibs.x64.14.00.appx
#                                  the UWP C++ runtime framework package
param(
  # Folder with xenia_edge_uwp.exe.
  [Parameter(Mandatory = $true)] [string] $BinDir,
  # Folder with dxil.dll and dxcompiler.dll from a DirectX Shader Compiler
  # release (bin\x64).
  [Parameter(Mandatory = $true)] [string] $DxcDir,
  [Parameter(Mandatory = $true)] [string] $OutDir,
  [string] $Version = "1.0.0.0",
  [string] $Publisher = "CN=XeniaEdgeUWP"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = Resolve-Path $OutDir
$Layout = Join-Path $OutDir "layout"
if (Test-Path $Layout) { Remove-Item -Recurse -Force $Layout }
New-Item -ItemType Directory -Force -Path $Layout | Out-Null

# Executable.
Copy-Item (Join-Path $BinDir "xenia_edge_uwp.exe") $Layout

# DXIL signing (dxil.dll) and the DXC runtime, loaded from D3D12\ next to the
# executable like on the desktop.
$D3D12Dir = Join-Path $Layout "D3D12"
New-Item -ItemType Directory -Force -Path $D3D12Dir | Out-Null
foreach ($dll in @("dxil.dll", "dxcompiler.dll")) {
  Copy-Item (Join-Path $DxcDir $dll) $D3D12Dir
}

# Tile and splash images, scaled from the app icon.
$AssetsDir = Join-Path $Layout "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
$Icon = [System.Drawing.Image]::FromFile(
  (Join-Path $RepoRoot "assets\icon\512.png"))
function Save-Image([string] $Name, [int] $Width, [int] $Height,
                    [double] $IconScale) {
  $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.InterpolationMode =
    [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $graphics.Clear([System.Drawing.Color]::Transparent)
  $size = [int]([Math]::Min($Width, $Height) * $IconScale)
  $graphics.DrawImage($Icon, [int](($Width - $size) / 2),
                      [int](($Height - $size) / 2), $size, $size)
  $graphics.Dispose()
  $bitmap.Save((Join-Path $AssetsDir $Name),
               [System.Drawing.Imaging.ImageFormat]::Png)
  $bitmap.Dispose()
}
Save-Image "StoreLogo.png" 50 50 1.0
Save-Image "Square44x44Logo.png" 44 44 1.0
Save-Image "Square150x150Logo.png" 150 150 0.8
Save-Image "Wide310x150Logo.png" 310 150 0.8
Save-Image "SplashScreen.png" 620 300 0.8
$Icon.Dispose()

# Manifest.
$Manifest = Get-Content -Raw (Join-Path $PSScriptRoot "AppxManifest.xml")
$Manifest = $Manifest.Replace("@VERSION@", $Version).Replace(
  "@PUBLISHER@", $Publisher)
Set-Content -Path (Join-Path $Layout "AppxManifest.xml") -Value $Manifest `
  -Encoding UTF8

# Windows SDK tools.
$SdkBin = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.*\x64\makeappx.exe" |
  Sort-Object FullName | Select-Object -Last 1
if (-not $SdkBin) { throw "makeappx.exe not found" }
$MakeAppx = $SdkBin.FullName
$SignTool = Join-Path $SdkBin.DirectoryName "signtool.exe"

$Msix = Join-Path $OutDir "XeniaEdge_${Version}_x64.msix"
& $MakeAppx pack /o /d $Layout /p $Msix
if ($LASTEXITCODE -ne 0) { throw "makeappx failed" }

# Self-signed certificate. The Xbox Device Portal installs packages without
# trusting it; a PC needs XeniaEdge.cer in Trusted People.
$Cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher `
  -KeyUsage DigitalSignature -FriendlyName "Xenia Edge UWP" `
  -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
$PfxPassword = ConvertTo-SecureString -String ([guid]::NewGuid().ToString()) `
  -Force -AsPlainText
$Pfx = Join-Path $env:RUNNER_TEMP "XeniaEdge.pfx"
if (-not $env:RUNNER_TEMP) { $Pfx = Join-Path $env:TEMP "XeniaEdge.pfx" }
Export-PfxCertificate -Cert $Cert -FilePath $Pfx -Password $PfxPassword | Out-Null
Export-Certificate -Cert $Cert -FilePath (Join-Path $OutDir "XeniaEdge.cer") | Out-Null
$PlainPassword = [System.Net.NetworkCredential]::new("", $PfxPassword).Password
& $SignTool sign /fd SHA256 /f $Pfx /p $PlainPassword $Msix
if ($LASTEXITCODE -ne 0) { throw "signtool failed" }
Remove-Item $Pfx
Remove-Item "Cert:\CurrentUser\My\$($Cert.Thumbprint)"

# The C++ runtime framework package the app depends on.
$VCLibs = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx" -ErrorAction SilentlyContinue |
  Select-Object -First 1
if (-not $VCLibs) { throw "Microsoft.VCLibs.x64.14.00.appx not found" }
$DependenciesDir = Join-Path $OutDir "Dependencies\x64"
New-Item -ItemType Directory -Force -Path $DependenciesDir | Out-Null
Copy-Item $VCLibs.FullName $DependenciesDir

Remove-Item -Recurse -Force $Layout
Write-Host "Packaged $Msix"
