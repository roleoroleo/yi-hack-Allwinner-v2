# Compatible with Windows PowerShell 5.1 and PowerShell 7.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $Destination,
    [string] $Archive,
    [switch] $KeepWifi
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$release = 'y21ga_0.3.7.tgz'
$releaseUrl = "https://github.com/roleoroleo/yi-hack-Allwinner-v2/releases/download/0.3.7/$release"
$releaseHash = '0029a4aec5d6c083d277e96172168a6869254400725faded72556f52ba772bf8'
$report = 'YI-LOCAL-STATUS.txt'
$ascii = [System.Text.Encoding]::ASCII

function Assert-EmptyCard([string] $Path) {
    $volumeDirectories = @('System Volume Information', '$RECYCLE.BIN')
    $nonEmpty = Get-ChildItem -LiteralPath $Path -Force |
        Where-Object { -not ($_.PSIsContainer -and $_.Name -in $volumeDirectories) } |
        Select-Object -First 1
    if ($nonEmpty) {
        throw 'Select an empty microSD card or output directory. Existing files will not be overwritten.'
    }
}

function Assert-WifiValue([string] $Value, [string] $Label, [int] $Minimum, [int] $Maximum) {
    if ($Value.Length -lt $Minimum -or $Value.Length -gt $Maximum) {
        throw "$Label must contain $Minimum to $Maximum characters."
    }
    if ($Value -match '[^ -~]|["\\]' -or $Value -ne $Value.Trim(' ') -or
        $Value.Contains('wifi_ssid=') -or $Value.Contains('wifi_psk=')) {
        throw "$Label contains a character or sequence this package does not support."
    }
}

$target = Get-Item -LiteralPath $Destination
if (-not $target.PSIsContainer -or $target.PSProvider.Name -ne 'FileSystem') {
    throw 'Select the existing drive or directory for your microSD card.'
}
Assert-EmptyCard $target.FullName
$tar = Get-Command -Name 'tar.exe', 'tar' -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $tar) {
    throw 'Could not find tar. Recent versions of Windows include tar.exe.'
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('yi-local-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    if ($Archive) {
        $archivePath = (Get-Item -LiteralPath $Archive).FullName
    } else {
        $archivePath = Join-Path $work $release
        Write-Host "Downloading official package: $release"
        Invoke-WebRequest -Uri $releaseUrl -OutFile $archivePath -UseBasicParsing -TimeoutSec 60
    }
    if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $releaseHash) {
        throw "The SHA-256 does not match the official $release package."
    }

    $card = Join-Path $work 'card'
    New-Item -ItemType Directory -Path $card | Out-Null
    & $tar.Source '-xzf' $archivePath '-C' $card
    if ($LASTEXITCODE -ne 0) { throw 'Could not extract the release archive.' }

    $hack = Join-Path $card 'yi-hack'
    if ([System.IO.File]::ReadAllText((Join-Path $hack 'model_suffix')).Trim() -ne 'y21ga') {
        throw 'The archive does not contain the y21ga model suffix.'
    }

    $config = Join-Path $hack 'etc/system.conf'
    $settings = [System.IO.File]::ReadAllText($config, $ascii)
    $oldOption = "DISABLE_CLOUD=no`n"
    if (-not $settings.Contains($oldOption)) { throw 'The expected cloud configuration entry is missing.' }
    [System.IO.File]::WriteAllText($config, $settings.Replace($oldOption, "DISABLE_CLOUD=yes`n"), $ascii)

    $bootScript = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot 'boot-report.sh')).Replace("`r`n", "`n")
    [System.IO.File]::WriteAllText((Join-Path $hack 'boot-report.sh'), $bootScript, $ascii)
    $hook = "#!/bin/sh`nexec sh /tmp/sd/yi-hack/boot-report.sh /tmp/sd >/dev/null 2>&1`n"
    [System.IO.File]::WriteAllText((Join-Path $card 'debug.sh'), $hook, $ascii)
    [System.IO.File]::WriteAllText((Join-Path $card $report), "status=waiting_for_first_boot`nboot_count=0`n", $ascii)

    if (-not $KeepWifi) {
        $ssid = Read-Host '2.4 GHz Wi-Fi network name'
        Assert-WifiValue $ssid 'Wi-Fi network name' 1 32
        $key = Read-Host 'WPA2 password' -AsSecureString
        $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($key)
        try {
            $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
            Assert-WifiValue $password 'Password' 8 63
            [System.IO.File]::WriteAllText(
                (Join-Path $card 'Factory/configure_wifi.cfg'),
                "wifi_ssid=$ssid`nwifi_psk=$password`n",
                $ascii
            )
        } finally {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
            Remove-Variable -Name password -ErrorAction SilentlyContinue
            $key.Dispose()
        }
    }

    Assert-EmptyCard $target.FullName
    Get-ChildItem -LiteralPath $card -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $target.FullName -Recurse
    }
    Write-Host "Card prepared: $($target.FullName)"
    Write-Host "After booting, read $report or open the camera web interface."
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force
}
