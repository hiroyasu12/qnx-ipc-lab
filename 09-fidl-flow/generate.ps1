param(
    [string]$GeneratorPath = "$PSScriptRoot/../build/tools/core-3.2.15/commonapi-core-generator-windows-x86_64.exe",
    [string]$JavaPath = "$PSScriptRoot/../build/tools/java8/jdk8u512-b09-jre/bin/java.exe"
)
$ErrorActionPreference = 'Stop'
$generator = (Resolve-Path -LiteralPath $GeneratorPath).Path
$java = (Resolve-Path -LiteralPath $JavaPath).Path
$workspace = [System.IO.Path]::GetFullPath("$PSScriptRoot/../build/tools/generator-workspace")
Push-Location $PSScriptRoot
try {
    # -data keeps Eclipse metadata inside this repository's ignored build directory.
    # Windows PowerShell 5 treats native stderr warnings as ErrorRecords.
    $ErrorActionPreference = 'Continue'
    $generatorOutput = & $generator -vm $java -data $workspace -d generated -sk Default -pf Calculator.fidl 2>&1 |
        ForEach-Object { "$_" }
    $generatorExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $generatorOutput | Set-Content -Encoding UTF8 generator-run.txt
    $generatorOutput | Write-Output
    if ($generatorExitCode -ne 0) { throw "Core generator failed: $generatorExitCode" }
    Get-ChildItem -LiteralPath generated -Recurse -File |
        ForEach-Object { $_.FullName.Substring($PSScriptRoot.Length + 1) } |
        Sort-Object | Set-Content -Encoding UTF8 generated-files.txt
} finally {
    Pop-Location
}
