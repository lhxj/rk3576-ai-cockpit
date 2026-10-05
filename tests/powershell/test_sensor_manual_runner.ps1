param([Parameter(Mandatory=$true)][string]$Runner,[Parameter(Mandatory=$true)][string]$ActualColdLog)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($Runner,[ref]$tokens,[ref]$errors)
if($errors){throw 'runner AST error'}
foreach($name in @('Read-SharedText','Test-ColdIdentity','Quote-Argument','Start-Hidden')){
 $function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 if(!$function){throw 'production function absent'}
 . ([scriptblock]::Create($function.Extent.Text))
}
$cold=Get-Content -LiteralPath $ActualColdLog -Raw
if(!(Test-ColdIdentity $cold)){throw 'actual cold identity rejected'}
foreach($needle in @('soc cold boot','g8f53f800da-241224','7d8fe670d9','43164981ef','g149b1c5','runtime policy=0',"Hit key to stop autoboot('CTRL+C'):")){
 if(Test-ColdIdentity ($cold.Replace($needle,'BAD'))){throw "wrong identity accepted $needle"}
}
if((Quote-Argument 'C:\space path\test.ps1') -ne '"C:\space path\test.ps1"'){throw 'path quoting failed'}
$rejected=$false
try {Quote-Argument 'bad"value' | Out-Null} catch {$rejected=$true}
if(!$rejected){throw 'embedded quote accepted'}
$tmp=Join-Path ([IO.Path]::GetTempPath()) ('manual-read-'+[guid]::NewGuid()+'.txt')
try {
 [IO.File]::WriteAllText($tmp,'actual flushed log')
 if((Read-SharedText $tmp) -ne 'actual flushed log'){throw 'actual bounded read failed'}
 $rejected=$false
 try {Read-SharedText $tmp 2 | Out-Null} catch {$rejected=$true}
 if(!$rejected){throw 'logcap accepted'}
} finally {if(Test-Path -LiteralPath $tmp){Remove-Item -LiteralPath $tmp}}
$source=[IO.File]::ReadAllText($Runner)
if($source -match 'ExecutionPolicy|Bypass|Remove-Item|shutdown|reboot|insmod|rmmod'){throw 'unrequested operation in Windows controller'}
if(([regex]::Matches($source,"Token 'SOURCE'")).Count -ne 1){throw 'SOURCE not single explicit call'}
if($source -notmatch 'runtimeEnd-\$watch.Elapsed.TotalSeconds' -or $source -notmatch 'whole1200s' -or $source -notmatch '2097152'){throw 'bounded cleanup/runtime/logcap absent'}
$childDir=Join-Path ([IO.Path]::GetTempPath()) ('manual child '+[guid]::NewGuid())
New-Item -ItemType Directory -Path $childDir | Out-Null
try {
 $child=Join-Path $childDir 'argv child.ps1';$out=Join-Path $childDir 'child.stdout';$err=Join-Path $childDir 'child.stderr'
 [IO.File]::WriteAllText($child,'param([string]$Value) Write-Output $Value',[Text.UTF8Encoding]::new($true))
 $proc=Start-Hidden 'powershell.exe' @('-NoProfile','-NonInteractive','-File',$child,'-Value','two words intact') $out $err
 if(!$proc.WaitForExit(10000)){Stop-Process -Id $proc.Id;throw 'owned harmless child deadline'}
 if($proc.ExitCode -ne 0 -or (Get-Content -LiteralPath $out -Raw).Trim() -ne 'two words intact'){throw 'actual Start-Hidden argv lost spaces'}
} finally {
 $resolved=(Resolve-Path -LiteralPath $childDir).Path
 $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
 if([IO.Path]::GetDirectoryName($resolved) -ne $tempRoot -or !([IO.Path]::GetFileName($resolved).StartsWith('manual child '))){throw 'unexpected fixture cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse
}
'MANUAL_RUNNER_HOST_FIXTURES_PASS actual_identity_and_7_mutations quoting readcap source_once bounded_cleanup'
