[CmdletBinding()]
param(
    [string]$QtDir = "C:\Qt\6.11.0\msvc2022_64",
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$ClientDir = $PSScriptRoot
$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $ClientDir "..")).Path
$BuildDir = Join-Path $ClientDir "build_manual"
$DistDir = Join-Path $ProjectRoot "release\shatranj-client"
$AppVersion = (Get-Content -LiteralPath (Join-Path $ProjectRoot "VERSION") -TotalCount 1).Trim()
$Source = Join-Path $ProjectRoot "src\pc\client\main.cpp"
$PositionSource = Join-Path $ProjectRoot "src\common\chess\position.c"
$LegalSource = Join-Path $ProjectRoot "src\common\chess\legal.c"
$McuMaxSource = Join-Path $ProjectRoot "third_party\mcu-max\src\mcu-max.c"
$MqttSource = Join-Path $ProjectRoot "src\common\mqtt\mqtt.c"
$ProtocolSources = @(
    @{
        Path = Join-Path $ProjectRoot "src\common\protocol\game_protocol.c"
        Obj = Join-Path $BuildDir "game_protocol.obj"
        Rsp = Join-Path $BuildDir "game_protocol-cl.rsp"
    },
    @{
        Path = Join-Path $ProjectRoot "src\common\protocol\game_protocol_extra.c"
        Obj = Join-Path $BuildDir "game_protocol_extra.obj"
        Rsp = Join-Path $BuildDir "game_protocol_extra-cl.rsp"
    },
    @{
        Path = Join-Path $ProjectRoot "src\common\protocol\mqtt_session_protocol.c"
        Obj = Join-Path $BuildDir "mqtt_session_protocol.obj"
        Rsp = Join-Path $BuildDir "mqtt_session_protocol-cl.rsp"
    },
    @{
        Path = Join-Path $ProjectRoot "src\common\protocol\mqtt_session_protocol_format.c"
        Obj = Join-Path $BuildDir "mqtt_session_protocol_format.obj"
        Rsp = Join-Path $BuildDir "mqtt_session_protocol_format-cl.rsp"
    },
    @{
        Path = Join-Path $ProjectRoot "src\common\protocol\direct_session_protocol.c"
        Obj = Join-Path $BuildDir "direct_session_protocol.obj"
        Rsp = Join-Path $BuildDir "direct_session_protocol-cl.rsp"
    }
)
$Obj = Join-Path $BuildDir "main.obj"
$PositionObj = Join-Path $BuildDir "position.obj"
$LegalObj = Join-Path $BuildDir "legal.obj"
$McuMaxObj = Join-Path $BuildDir "mcu-max.obj"
$MqttObj = Join-Path $BuildDir "mqtt.obj"
$Rc = Join-Path $BuildDir "shatranj-client.rc"
$Res = Join-Path $BuildDir "shatranj-client.res"
$Icon = Join-Path $BuildDir "shatranj-client.ico"
$Exe = Join-Path $BuildDir "shatranj-client.exe"
$ClRsp = Join-Path $BuildDir "cl.rsp"
$PositionClRsp = Join-Path $BuildDir "position-cl.rsp"
$LegalClRsp = Join-Path $BuildDir "legal-cl.rsp"
$McuMaxClRsp = Join-Path $BuildDir "mcu-max-cl.rsp"
$MqttClRsp = Join-Path $BuildDir "mqtt-cl.rsp"
$LinkRsp = Join-Path $BuildDir "link.rsp"

function Remove-GeneratedTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    $rootPrefix = $resolvedRoot.TrimEnd('\') + '\'

    if (-not $resolvedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove outside client tree: $resolvedPath"
    }

    for ($attempt = 1; $attempt -le 10; $attempt++) {
        try {
            Remove-Item -LiteralPath $resolvedPath -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 10) {
                Write-Host "[ERR] Unable to clean generated client output: $resolvedPath"
                Write-Host "[ERR] Close any running Shatranj client window and retry."
                Write-Host "[ERR] Original error: $($_.Exception.Message)"
                exit 1
            }
            Start-Sleep -Milliseconds 300
        }
    }
}

function Add-UInt16LE {
    param(
        [System.Collections.Generic.List[byte]]$Bytes,
        [Parameter(Mandatory = $true)]
        [int]$Value
    )

    $Bytes.Add([byte]($Value -band 0xff))
    $Bytes.Add([byte](($Value -shr 8) -band 0xff))
}

function Add-UInt32LE {
    param(
        [System.Collections.Generic.List[byte]]$Bytes,
        [Parameter(Mandatory = $true)]
        [long]$Value
    )

    $Bytes.Add([byte]($Value -band 0xff))
    $Bytes.Add([byte](($Value -shr 8) -band 0xff))
    $Bytes.Add([byte](($Value -shr 16) -band 0xff))
    $Bytes.Add([byte](($Value -shr 24) -band 0xff))
}

function New-ShatranjIconImage {
    param(
        [int]$Size
    )

    $bitmap = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $format = New-Object System.Drawing.StringFormat
    $font = $null

    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

        $font = New-Object System.Drawing.Font "Segoe UI Symbol", ([single]($Size * 0.90)), ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center
        $format.FormatFlags = [System.Drawing.StringFormatFlags]::NoClip

        $rect = New-Object System.Drawing.RectangleF 0, ([single](-$Size * 0.04)), ([single]$Size), ([single]($Size * 1.08))
        $graphics.DrawString(([string][char]0x265E), $font, [System.Drawing.Brushes]::Black, $rect, $format)
    } finally {
        if ($font -ne $null) {
            $font.Dispose()
        }
        $format.Dispose()
        $graphics.Dispose()
    }

    $image = New-Object 'System.Collections.Generic.List[byte]'
    Add-UInt32LE $image 40
    Add-UInt32LE $image $Size
    Add-UInt32LE $image ($Size * 2)
    Add-UInt16LE $image 1
    Add-UInt16LE $image 32
    Add-UInt32LE $image 0
    Add-UInt32LE $image ($Size * $Size * 4)
    Add-UInt32LE $image 0
    Add-UInt32LE $image 0
    Add-UInt32LE $image 0
    Add-UInt32LE $image 0

    for ($y = $Size - 1; $y -ge 0; $y--) {
        for ($x = 0; $x -lt $Size; $x++) {
            $pixel = $bitmap.GetPixel($x, $y)
            $image.Add([byte]$pixel.B)
            $image.Add([byte]$pixel.G)
            $image.Add([byte]$pixel.R)
            $image.Add([byte]$pixel.A)
        }
    }

    $maskStride = [int]((($Size + 31) / 32)) * 4
    $mask = New-Object byte[] ($maskStride * $Size)
    for ($y = $Size - 1; $y -ge 0; $y--) {
        $maskRow = $Size - 1 - $y
        for ($x = 0; $x -lt $Size; $x++) {
            if ($bitmap.GetPixel($x, $y).A -lt 128) {
                $maskOffset = ($maskRow * $maskStride) + [int]($x / 8)
                $mask[$maskOffset] = [byte]($mask[$maskOffset] -bor (0x80 -shr ($x % 8)))
            }
        }
    }
    foreach ($b in $mask) {
        $image.Add($b)
    }

    $bitmap.Dispose()
    return $image.ToArray()
}

function New-ShatranjIcon {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $sizes = @(16, 32, 48, 64)
    $images = @()
    foreach ($size in $sizes) {
        $images += ,(New-ShatranjIconImage -Size $size)
    }

    $bytes = New-Object 'System.Collections.Generic.List[byte]'
    Add-UInt16LE $bytes 0
    Add-UInt16LE $bytes 1
    Add-UInt16LE $bytes $sizes.Count

    $offset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $size = $sizes[$i]
        $image = $images[$i]
        $bytes.Add([byte]$size)
        $bytes.Add([byte]$size)
        $bytes.Add([byte]0)
        $bytes.Add([byte]0)
        Add-UInt16LE $bytes 1
        Add-UInt16LE $bytes 32
        Add-UInt32LE $bytes $image.Length
        Add-UInt32LE $bytes $offset
        $offset += $image.Length
    }

    foreach ($image in $images) {
        foreach ($b in $image) {
            $bytes.Add($b)
        }
    }

    [System.IO.File]::WriteAllBytes($Path, $bytes.ToArray())
}

if (-not (Test-Path -LiteralPath $QtDir)) {
    throw "Qt directory not found: $QtDir"
}
if (-not (Test-Path -LiteralPath $VcVars)) {
    throw "Visual Studio vcvarsall.bat not found: $VcVars"
}
if (-not (Test-Path -LiteralPath $Source)) {
    throw "Source file not found: $Source"
}
if (-not (Test-Path -LiteralPath $PositionSource)) {
    throw "Source file not found: $PositionSource"
}
if (-not (Test-Path -LiteralPath $LegalSource)) {
    throw "Source file not found: $LegalSource"
}
if (-not (Test-Path -LiteralPath $McuMaxSource)) {
    throw "Source file not found: $McuMaxSource"
}
if (-not (Test-Path -LiteralPath $MqttSource)) {
    throw "Source file not found: $MqttSource"
}
foreach ($protocolSource in $ProtocolSources) {
    if (-not (Test-Path -LiteralPath $protocolSource.Path)) {
        throw "Source file not found: $($protocolSource.Path)"
    }
}

Remove-GeneratedTree -Path $BuildDir -Root $ClientDir
Remove-GeneratedTree -Path $DistDir -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ProjectRoot "release\netchesszx-client") -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ProjectRoot "release\shatranj") -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\shatranj") -Root $ClientDir
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\netchesszx-client") -Root $ClientDir
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\zxchess-client") -Root $ClientDir

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

New-ShatranjIcon -Path $Icon
Set-Content -LiteralPath $Rc -Value "1 ICON `"shatranj-client.ico`"" -Encoding ASCII

$includeDirs = @(
    (Join-Path $QtDir "include"),
    (Join-Path $QtDir "include\QtWidgets"),
    (Join-Path $QtDir "include\QtNetwork"),
    (Join-Path $QtDir "include\QtGui"),
    (Join-Path $QtDir "include\QtCore"),
    (Join-Path $ProjectRoot "src"),
    (Join-Path $ProjectRoot "third_party\mcu-max\src")
)

$args = @(
    "/nologo",
    "/c",
    "/EHsc",
    "/std:c++17",
    "/permissive-",
    "/Zc:__cplusplus",
    "/MD",
    "/O2",
    "/DNDEBUG",
    "/DWIN32",
    "/D_WINDOWS",
    "/DUNICODE",
    "/D_UNICODE",
    "/DQT_WIDGETS_LIB",
    "/DQT_NETWORK_LIB",
    "/DQT_GUI_LIB",
    "/DQT_CORE_LIB",
    "/DNETCHESSZX_APP_VERSION=$AppVersion"
)

foreach ($dir in $includeDirs) {
    $args += "/I`"$dir`""
}

$args += "`"$Source`""
$args += "/Fo:`"$Obj`""

Set-Content -LiteralPath $ClRsp -Value $args -Encoding ASCII

$cBaseArgs = @(
    "/nologo",
    "/c",
    "/TC",
    "/std:c11",
    "/MD",
    "/O2",
    "/DNDEBUG",
    "/DWIN32",
    "/D_WINDOWS"
)

$cIncludeDirs = @(
    (Join-Path $ProjectRoot "src"),
    (Join-Path $ProjectRoot "third_party\mcu-max\src")
)

foreach ($dir in $cIncludeDirs) {
    $cBaseArgs += "/I`"$dir`""
}

$positionArgs = @($cBaseArgs)
$positionArgs += "`"$PositionSource`""
$positionArgs += "/Fo:`"$PositionObj`""
Set-Content -LiteralPath $PositionClRsp -Value $positionArgs -Encoding ASCII

$legalArgs = @($cBaseArgs)
$legalArgs += "`"$LegalSource`""
$legalArgs += "/Fo:`"$LegalObj`""
Set-Content -LiteralPath $LegalClRsp -Value $legalArgs -Encoding ASCII

$mcuMaxArgs = @($cBaseArgs)
$mcuMaxArgs += "`"$McuMaxSource`""
$mcuMaxArgs += "/Fo:`"$McuMaxObj`""
Set-Content -LiteralPath $McuMaxClRsp -Value $mcuMaxArgs -Encoding ASCII

$mqttArgs = @($cBaseArgs)
$mqttArgs += "`"$MqttSource`""
$mqttArgs += "/Fo:`"$MqttObj`""
Set-Content -LiteralPath $MqttClRsp -Value $mqttArgs -Encoding ASCII

foreach ($protocolSource in $ProtocolSources) {
    $protocolArgs = @($cBaseArgs)
    $protocolArgs += "`"$($protocolSource.Path)`""
    $protocolArgs += "/Fo:`"$($protocolSource.Obj)`""
    Set-Content -LiteralPath $protocolSource.Rsp -Value $protocolArgs -Encoding ASCII
}

$linkArgs = @(
    "/NOLOGO",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:mainCRTStartup",
    "/OUT:`"$Exe`"",
    "`"$Obj`"",
    "`"$PositionObj`"",
    "`"$LegalObj`"",
    "`"$McuMaxObj`"",
    "`"$MqttObj`""
)
foreach ($protocolSource in $ProtocolSources) {
    $linkArgs += "`"$($protocolSource.Obj)`""
}
$linkArgs += @(
    "`"$Res`"",
    "/LIBPATH:`"$(Join-Path $QtDir "lib")`"",
    "Qt6Widgets.lib",
    "Qt6Network.lib",
    "Qt6Gui.lib",
    "Qt6Core.lib",
    "user32.lib",
    "gdi32.lib",
    "shell32.lib",
    "ole32.lib",
    "uuid.lib",
    "advapi32.lib",
    "ws2_32.lib"
)

Set-Content -LiteralPath $LinkRsp -Value $linkArgs -Encoding ASCII

$compileSteps = @(
    "cl @`"$ClRsp`"",
    "cl @`"$PositionClRsp`"",
    "cl @`"$LegalClRsp`"",
    "cl @`"$McuMaxClRsp`"",
    "cl @`"$MqttClRsp`""
)
foreach ($protocolSource in $ProtocolSources) {
    $compileSteps += "cl @`"$($protocolSource.Rsp)`""
}

$cmd = "cd /d `"$BuildDir`" && call `"$VcVars`" x64 && rc /nologo /fo `"$Res`" `"$Rc`" && " + ($compileSteps -join " && ") + " && link @`"$LinkRsp`""
& cmd.exe /d /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "cl failed with exit code $LASTEXITCODE"
}

Copy-Item -LiteralPath $Exe -Destination (Join-Path $DistDir "shatranj-client.exe") -Force

$pieceAssetSrc = Join-Path $ProjectRoot "assets\pc-client\pieces"
$pieceAssetDst = Join-Path $DistDir "assets\pc-client\pieces"
if (Test-Path -LiteralPath $pieceAssetSrc) {
    New-Item -ItemType Directory -Force -Path $pieceAssetDst | Out-Null
    Copy-Item -Path (Join-Path $pieceAssetSrc "*") -Destination $pieceAssetDst -Force
}

$dlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll")
foreach ($dll in $dlls) {
    $src = Join-Path (Join-Path $QtDir "bin") $dll
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Qt DLL not found: $src"
    }
    Copy-Item -LiteralPath $src -Destination $DistDir -Force
}

$platformDir = Join-Path $DistDir "platforms"
New-Item -ItemType Directory -Force -Path $platformDir | Out-Null
$qwindows = Join-Path $QtDir "plugins\platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $qwindows)) {
    throw "Qt platform plugin not found: $qwindows"
}
Copy-Item -LiteralPath $qwindows -Destination $platformDir -Force

Write-Host "Built: $Exe"
Write-Host "Packaged: $(Join-Path $DistDir "shatranj-client.exe")"
