# Shared native-process helper for the Windows Qt client build entry points.
# Child tools inherit this shell's standard handles. Do not redirect compiler
# output through managed pipes: CMake/MSBuild already provide normal progress
# output and the caller is responsible for displaying it.

function Invoke-ProcessChecked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$WorkingDirectory = ""
    )

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $FilePath
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $false
    $psi.RedirectStandardError = $false
    $psi.EnvironmentVariables["MSBUILDDISABLENODEREUSE"] = "1"
    $psi.EnvironmentVariables["TrackFileAccess"] = "false"
    foreach ($arg in $Arguments) {
        [void]$psi.ArgumentList.Add($arg)
    }
    if (-not [string]::IsNullOrWhiteSpace($WorkingDirectory)) {
        $psi.WorkingDirectory = $WorkingDirectory
    }

    Write-Host "[$Name]"
    $process = [System.Diagnostics.Process]::Start($psi)
    try {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "$Name failed with exit code $($process.ExitCode)."
        }
    } finally {
        $process.Dispose()
    }
}
