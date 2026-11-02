param(
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Validate','BuildEditor','GenerateAssets','TestEditor','Package')][string]$Action='Validate',
    [ValidateSet('Clients','WindowsServer','LinuxServer')][string]$Target='Clients'
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$descriptor=@(Get-ChildItem -LiteralPath $repoRoot -Filter '*.uplugin')
if($descriptor.Count -ne 1) {throw 'Expected one mod descriptor at the repository root.'}
$modName=$descriptor[0].BaseName
$project=Join-Path $ProjectRoot 'FactoryGame.uproject'
$build=Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$uat=Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
foreach($required in @($project,$build,$editor,$uat,(Join-Path $ProjectRoot "Mods/$modName/$modName.uplugin"))) {
    if(-not (Test-Path -LiteralPath $required)) {throw "Missing prerequisite: $required"}
}
if($Action -eq 'Validate') {Write-Output "Prerequisite paths exist for $modName. No build was run."; return}
$reports=Join-Path $repoRoot 'Saved/PublicBuild'
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$savedModules=$env:PSModulePath
try {
    $env:PSModulePath=@((Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/Modules'),(Join-Path $env:ProgramFiles 'WindowsPowerShell/Modules')) -join ';'
    switch($Action) {
        'BuildEditor' {
            & $build FactoryEditor Win64 Development "-Project=$project" -WaitMutex
            if($LASTEXITCODE -ne 0) {throw "Editor build failed: $LASTEXITCODE"}
        }
        'GenerateAssets' {
            $generator=if($modName -eq 'TrapperPoweredFoundations') {'PFGenerateAssets'} else {'TFGenerateAssets'}
            & $editor $project "-run=$generator" -unattended -nop4 -nosplash -nullrhi "-abslog=$reports/assets.log"
            if($LASTEXITCODE -ne 0) {throw "Asset generation failed: $LASTEXITCODE"}
        }
        'TestEditor' {
            $reportDir=Join-Path $reports ('EditorTests-'+[Guid]::NewGuid().ToString('N'))
            & $editor $project -unattended -nop4 -nosplash -nullrhi "-ExecCmds=Automation RunTests $modName" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportDir" "-abslog=$reportDir.log"
            if($LASTEXITCODE -ne 0) {throw "Editor tests exited with $LASTEXITCODE"}
            $report=Get-Content -Raw -LiteralPath (Join-Path $reportDir 'index.json') | ConvertFrom-Json
            if(-not $report.tests -or @($report.tests | Where-Object {$_.state -ne 'Success'}).Count) {throw 'Editor tests failed or were not run.'}
        }
        'Package' {
            $argsList=@("-ScriptsForProject=$project",'PackagePlugin',"-Project=$project","-DLCName=$modName",'-clientconfig=Shipping','-serverconfig=Shipping','-build','-utf8output','-nocompileeditor','-installed','-WaitForUATMutex')
            if($Target -eq 'Clients') {$argsList+=@('-platform=Win64','-merge')}
            elseif($Target -eq 'WindowsServer') {$argsList+=@('-server','-serverplatform=Win64','-noclient','-Target=FactoryServer')}
            else {$argsList+=@('-server','-serverplatform=Linux','-noclient','-Target=FactoryServer')}
            & $uat @argsList
            if($LASTEXITCODE -ne 0) {throw "Package failed: $LASTEXITCODE"}
        }
    }
} finally {$env:PSModulePath=$savedModules}
