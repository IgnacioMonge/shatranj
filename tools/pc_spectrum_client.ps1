param(
    [Parameter(Mandatory=$true)]
    [string]$HostIP,
    [int]$Port = 5000,
    [string]$Message = "HELLO 1 PC",
    [switch]$SelfTest
)

if ($SelfTest) {
    "pc_spectrum_client.ps1 ok"
    exit 0
}

$client = [System.Net.Sockets.TcpClient]::new()
Write-Host "Connecting to Spectrum $HostIP`:$Port ..."
$client.Connect($HostIP, $Port)
Write-Host "CONNECTED"

try {
    $stream = $client.GetStream()
    $payload = $Message + "`n"
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($payload)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    Write-Host "TX: $Message"

    $buffer = New-Object byte[] 512
    $stream.ReadTimeout = 10000
    $n = $stream.Read($buffer, 0, $buffer.Length)
    if ($n -gt 0) {
        $text = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $n)
    Write-Host ("RX: " + $text.TrimEnd())
    } else {
        Write-Host "RX: <closed>"
    }
} finally {
    $client.Close()
}
