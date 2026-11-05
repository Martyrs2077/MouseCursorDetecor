# User-invoked build entry point. The assistant must not execute this script.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolBin = 'C:\GCC\ucrt64\bin'
$cmakeExe = Join-Path $toolBin 'cmake.exe'
$objdumpExe = Join-Path $toolBin 'objdump.exe'
$publishDir = $projectRoot
$publishedExe = Join-Path $publishDir 'MouseCursorDetector.exe'
$publishedInstaller = Join-Path $publishDir 'MouseCursorDetector-Setup.exe'
$installerScript = Join-Path $projectRoot 'installer\MouseCursorDetector.iss'

function Get-InstallerCompiler {
    # Run only when the user invokes build.cmd. Keep the tool local and portable.
    $toolsDir = Join-Path $projectRoot '.tools'
    $compilerDir = Join-Path $toolsDir 'innosetup-6.7.4'
    $compiler = Join-Path $compilerDir 'ISCC.exe'
    if (Test-Path -LiteralPath $compiler -PathType Leaf) { return $compiler }

    New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
    $download = Join-Path $toolsDir 'innosetup-6.7.4.exe'
    $downloadUrl = 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-6.7.4.exe'
    if (-not (Test-Path -LiteralPath $download -PathType Leaf)) {
        Write-Host 'Downloading the Inno Setup 6.7.4 packaging tool (first build only)...'
        $originalProtocol = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $originalProtocol -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $downloadUrl -OutFile "$download.partial" -UseBasicParsing
            Move-Item -LiteralPath "$download.partial" -Destination $download -Force
        } catch {
            throw "Cannot download Inno Setup. Check your network and run build.cmd again. You can also save the official download to $download. Details: $($_.Exception.Message)"
        } finally {
            [Net.ServicePointManager]::SecurityProtocol = $originalProtocol
        }
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $download
    if ($signature.Status -ne 'Valid' -or
        $null -eq $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Pyrsys B\.V\.(,|$)') {
        throw "Inno Setup signature validation failed. Remove or replace $download with the official signed 6.7.4 download, then retry."
    }
    Write-Host 'Preparing the local portable packaging tool...'
    $arguments = @('/CURRENTUSER', '/PORTABLE=1', '/VERYSILENT',
        '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/NOICONS',
        '/TASKS=', ('/DIR="{0}"' -f $compilerDir))
    $process = Start-Process -FilePath $download -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
        throw "Inno Setup preparation failed (exit $($process.ExitCode)). Run build.cmd again."
    }
    return $compiler
}

function Assert-ExecutableAvailable([string]$FilePath) {
    if (-not (Test-Path -LiteralPath $FilePath)) { return }
    $stream = $null
    try {
        $stream = [System.IO.File]::Open($FilePath, [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    } catch {
        throw "Cannot replace $FilePath. Exit MouseCursorDetector from its system tray menu, then run build.cmd again. If it is already closed, check file permissions or other programs holding this file. Details: $($_.Exception.Message)"
    } finally {
        if ($null -ne $stream) { $stream.Dispose() }
    }
}

Assert-ExecutableAvailable $publishedExe
Assert-ExecutableAvailable $publishedInstaller
foreach ($tool in @('g++.exe', 'windres.exe', 'ninja.exe', 'cmake.exe', 'objdump.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $toolBin $tool))) {
        throw "Missing $tool. Install the documented MSYS2 UCRT64 tools manually."
    }
}

$installerCompiler = Get-InstallerCompiler
$originalPath = $env:PATH
Push-Location -LiteralPath $projectRoot
try {
    $env:PATH = "$toolBin;$originalPath"
    & $cmakeExe --preset gcc-release
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmakeExe --build --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

    $executable = Join-Path $projectRoot 'build\gcc-release\MouseCursorDetector.exe'
    $headers = & $objdumpExe -f $executable
    if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'pei-x86-64') {
        throw 'The executable is not a validated x64 PE file.'
    }
    $imports = & $objdumpExe -p $executable
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect executable dependencies.' }
    $dependencies = @($imports | ForEach-Object {
        if ($_ -match 'DLL Name:\s*(\S+)') { $Matches[1] }
    })
    if ($dependencies.Count -eq 0) { throw 'No DLL import records found.' }
    foreach ($dependency in $dependencies) {
        if ($dependency -match '^(libgcc|libstdc\+\+|libwinpthread|msys-|cygwin)') {
            throw "Unexpected external runtime: $dependency"
        }
        if ($dependency -notmatch '^(api-ms-win-|ext-ms-win-)' -and
            -not (Test-Path -LiteralPath (Join-Path "$env:WINDIR\System32" $dependency))) {
            throw "Non-system dependency requires review: $dependency"
        }
    }

    $installerOutputDir = Join-Path $projectRoot 'build\installer'
    New-Item -ItemType Directory -Path $installerOutputDir -Force | Out-Null
    & $installerCompiler "/DAppExe=$executable" "/O$installerOutputDir" $installerScript
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed. Root outputs have not been updated.' }
    $installer = Join-Path $installerOutputDir 'MouseCursorDetector-Setup.exe'
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw 'Installer build did not produce the expected setup EXE.'
    }

    Assert-ExecutableAvailable $publishedExe
    Assert-ExecutableAvailable $publishedInstaller
    try {
        Copy-Item -LiteralPath $executable -Destination $publishedExe
        Copy-Item -LiteralPath $installer -Destination $publishedInstaller
    } catch {
        throw "Build completed, but publishing failed. Exit MouseCursorDetector from its system tray menu and run build.cmd again. Destination: $publishedExe. Details: $($_.Exception.Message)"
    }
    Write-Output "Published: $publishDir\MouseCursorDetector.exe"
    Write-Output "Published: $publishedInstaller"
    Write-Output 'Existing config/ and styles/ folders were preserved.'
    Write-Output ('System DLL imports: ' + ($dependencies -join ', '))
    Write-Output 'Clean Windows and interactive feature acceptance tests remain required.'
} finally {
    $env:PATH = $originalPath
    Pop-Location
}

