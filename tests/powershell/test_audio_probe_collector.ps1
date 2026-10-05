param([string]$RepoRoot,[string]$Original='C:/Users/27432/Documents/MPU6050-Test-20261005-v1/capture-sensor-dual-uart-v2.ps1')
$ErrorActionPreference='Stop'
$root=if($RepoRoot){[IO.Path]::GetFullPath($RepoRoot)}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))}
$asset=Get-Content -Raw -LiteralPath (Join-Path $root 'docs/bringup/mpu6050/AUDIO_PROBE_DIAGNOSTIC_COLLECTOR.json') | ConvertFrom-Json
$file=Join-Path $root $asset.source_file
function Need([bool]$ok,[string]$message){if(!$ok){throw $message}}
Need ((Get-FileHash -LiteralPath $Original).Hash.ToLowerInvariant() -eq '95f55bef69ceeec5e53ab2afa83db9866658b383315a8ad005081a6e869e9e35') 'original pin'
$text=[IO.File]::ReadAllText($Original)
foreach($pair in $asset.replacements){$text=$text.Replace($pair[0],$pair[1])}
Need ($text -ceq [IO.File]::ReadAllText($file)) 'only reviewed literal substitutions'
Need ((Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant() -eq $asset.derived_sha256) 'derived pin'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$errors)
Need ($errors.Count -eq 0) 'PS5 AST syntax'
$switch=$ast.Find({param($n) $n -is [Management.Automation.Language.SwitchStatementAst]},$true)
$blocks=@{}
foreach($c in $switch.Clauses){$blocks[$c.Item1.Value]=[scriptblock]::Create($c.Item2.Extent.Text.Substring(1,$c.Item2.Extent.Text.Length-2))}
class HostPort {
 [Collections.Generic.List[string]]$Writes=[Collections.Generic.List[string]]::new()
 [void]Write([string]$value){$this.Writes.Add($value)}
}
# Execute only original production switch bodies with an in-memory port. Never whole collector.
$ports=@([HostPort]::new());$recent=@('','');$watch=[Diagnostics.Stopwatch]::StartNew()
$cold=$true;$interrupted=$true;$loaded=$false;$inspected=$false;$sourced=$false
. $blocks.LOAD
Need ($ports[0].Writes[0] -eq ($asset.replacements[0][1]+"`r")) 'exact p3 LOAD'
. $blocks.INSPECT
Need ($ports[0].Writes[1] -eq "printenv fileaddr filesize`r") 'INSPECT actual production command'
$recent[0]="fileaddr=4c000000`nfilesize=$($asset.script_size_hex)`n"
. $blocks.SOURCE
Need ($sourced -and $phaseLimit -eq 180 -and $ports[0].Writes[2] -eq "source 0x4c000000`r") 'SOURCE once/180s'
$rejected=$false;try{. $blocks.SOURCE}catch{$rejected=$true};Need $rejected 'second SOURCE rejected'
foreach($bad in @("fileaddr=4c000000`nfilesize=c99`n","fileaddr=4c000000`nfilesize=ca4`n","fileaddr=4c000000`nfilesize=d94`n","fileaddr=4c000001`nfilesize=$($asset.script_size_hex)`n",'')){
 $sourced=$false;$loaded=$true;$inspected=$true;$recent[0]=$bad
 $rejected=$false;try{. $blocks.SOURCE}catch{$rejected=$true};Need $rejected 'old/wrong/missing identity rejected'
}
$sourced=$false;$loaded=$true;$inspected=$false;$recent[0]="fileaddr=4c000000`nfilesize=$($asset.script_size_hex)`n"
$rejected=$false;try{. $blocks.SOURCE}catch{$rejected=$true};Need $rejected 'ordering rejected'
Need ($ports[0].Writes.Count -eq 3) 'failures send no board commands'
Write-Output 'AUDIO_PROBE_COLLECTOR_HOST_PASS AST/exact derivation/production LOAD INSPECT SOURCE/repeat/oldsize/wrongaddr/ordering; no serial open'
