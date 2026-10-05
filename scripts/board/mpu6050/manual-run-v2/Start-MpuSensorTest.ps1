param([switch]$HostCheckOnly)
$ErrorActionPreference='Stop'
$Collector=Join-Path (Split-Path $PSScriptRoot -Parent) 'capture-sensor-dual-uart-v2.ps1'
$ExpectedCollector='95f55bef69ceeec5e53ab2afa83db9866658b383315a8ad005081a6e869e9e35'

function Get-AssetSha256([string]$Path){
 $stream=[IO.File]::OpenRead($Path);$digest=[Security.Cryptography.SHA256]::Create()
 try {return ([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}
 finally {$digest.Dispose();$stream.Dispose()}
}
function Read-SharedText([string]$Path,[int]$Cap=262144){
 if(!(Test-Path -LiteralPath $Path)){return ''}
 $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 try {
  if($stream.Length -gt $Cap){throw "STOP log cap $Path"}
  $reader=[IO.StreamReader]::new($stream);return $reader.ReadToEnd()
 } finally {$stream.Dispose()}
}
function Test-ColdIdentity([string]$Text){
 foreach($needle in @('soc cold boot','U-Boot SPL 2017.09-g8f53f800da-241224','sha256(7d8fe670d9...) + OK','sha256(43164981ef...) + OK','U-Boot 2017.09-g149b1c5','PROJECT: factory boot/CLI allowed by runtime policy=0',"Hit key to stop autoboot('CTRL+C'):")){
  if(!$Text.Contains($needle)){return $false}
 }
 return $Text -match '(?m)^=>\s*$'
}
function Quote-Argument([string]$Value){
 if($Value.Contains('"') -or $Value.Contains("`n")){throw 'STOP unsupported argument'}
 if([string]::IsNullOrEmpty($Value)){throw 'STOP empty argument'}
 if($Value -match '^[A-Za-z0-9_./:=+-]+$'){return $Value}
 return '"'+$Value+'"'
}
function Start-Hidden([string]$Exe,[string[]]$Arguments,[string]$Out,[string]$Err){
 $quoted=@($Arguments | ForEach-Object {Quote-Argument $_}) -join ' '
 $childProcess=Start-Process -FilePath $Exe -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput $Out -RedirectStandardError $Err
 $null=$childProcess.Handle # Preserve ExitCode access on Windows PowerShell 5.
 return $childProcess
}
function Convert-WslPath([string]$Path){
 $normalized=[IO.Path]::GetFullPath($Path).Replace('\','/')
 $result=@(& wsl.exe -d Ubuntu-22.04 -- wslpath -u $normalized)
 $code=$LASTEXITCODE
 if($code -ne 0 -or $result.Count -ne 1 -or [string]::IsNullOrWhiteSpace([string]$result[0])){throw "STOP WSL path conversion exit=$code path=$normalized"}
 $converted=([string]$result[0]).Trim()
 if(!$converted.StartsWith('/mnt/') -or $converted.Contains("`n")){throw 'STOP unexpected WSL path'}
 return $converted
}
if($HostCheckOnly){
 if((Get-AssetSha256 $Collector) -ne $ExpectedCollector){throw 'STOP collector hash'}
 $checkLock=Convert-WslPath (Join-Path $PSScriptRoot 'hold-lock.sh')
 $checkRuntime=Convert-WslPath (Join-Path $PSScriptRoot 'run-runtime.sh')
 $checkRelease=Convert-WslPath (Join-Path $PSScriptRoot 'host-check-release-path-only')
 foreach($helper in @($checkLock,$checkRuntime)){
  & wsl.exe -d Ubuntu-22.04 -- bash -n $helper
  if($LASTEXITCODE -ne 0){throw 'STOP helper syntax/read failed'}
 }
 Write-Output "HOST_CHECK_ONLY paths3/bash-n2 PASS no ports/SSH/lock/board command release=$checkRelease";return
}
$capture=$null;$lock=$null;$runtime=$null;$control=$null;$release=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$logDir=Join-Path $PSScriptRoot ('logs-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $logDir | Out-Null
function Guard {
 if($watch.Elapsed.TotalSeconds -ge 1200){throw 'STOP whole1200s deadline'}
 if($lock -and $lock.HasExited){throw 'STOP shared board lock ended'}
}
function Wait-Evidence([scriptblock]$Predicate,[double]$Seconds,[string]$Reason){
 $end=$watch.Elapsed.TotalSeconds+$Seconds
 do {
  Guard
  if($capture -and $capture.HasExited){throw "STOP collector ended before $Reason"}
  if(& $Predicate){return}
  Start-Sleep -Milliseconds 100
 } while($watch.Elapsed.TotalSeconds -lt $end)
 throw "STOP $Reason deadline"
}
function Token([string]$Value){
 Guard
 if($capture.HasExited){throw 'STOP collector ended before control token'}
 Add-Content -LiteralPath $control -Value $Value -Encoding ASCII
}
try {
 if((Get-AssetSha256 $Collector) -ne $ExpectedCollector){throw 'STOP collector hash'}
 $release=Join-Path $logDir 'release.lock'
 $control=Join-Path $logDir 'control.txt'
 $lockOut=Join-Path $logDir 'lock.stdout';$lockErr=Join-Path $logDir 'lock.stderr'
 $linux=Join-Path $logDir 'linux-com5.log';$m0=Join-Path $logDir 'm0-com6.log'
 $capOut=Join-Path $logDir 'capture.stdout';$capErr=Join-Path $logDir 'capture.stderr'
 $lockScript=Convert-WslPath (Join-Path $PSScriptRoot 'hold-lock.sh')
 $releaseWSL=Convert-WslPath $release
 $runtimeScript=Convert-WslPath (Join-Path $PSScriptRoot 'run-runtime.sh')
 $lock=Start-Hidden 'wsl.exe' @('-d','Ubuntu-22.04','--','bash',$lockScript,$releaseWSL) $lockOut $lockErr
 Wait-Evidence { (Read-SharedText $lockOut) -match 'MANUAL_BOARD_LOCK_READY' } 10 'shared-lock READY'
 $capture=Start-Hidden 'powershell.exe' @('-NoProfile','-NonInteractive','-File',$Collector,'-LinuxLog',$linux,'-M0Log',$m0,'-ControlFile',$control,'-InterruptColdBoot') $capOut $capErr
 Wait-Evidence { (Read-SharedText $capOut) -match 'UART_CAPTURE_READY' } 10 'UART READY'
 Write-Host '板当前停在U-Boot；保持接线。现在拔主电，等10秒，再上电。不要移动MIPI或风扇。'
 Write-Host "本次日志：$logDir"
 Wait-Evidence { Test-ColdIdentity (Read-SharedText $linux) } 300 'cold identity and interrupted U-Boot prompt'
 Token 'LOAD'
 Wait-Evidence { (Read-SharedText $linux) -match '3225 bytes read' } 15 'actual LOAD3225'
 Token 'INSPECT'
 Wait-Evidence { $text=Read-SharedText $linux; $text -match '(?m)^fileaddr=(?:0x)?4c000000\s*$' -and $text -match '(?m)^filesize=(?:0x)?c99\s*$' } 10 'actual INSPECT address/length'
 Token 'SOURCE'
 $sourceUTC=[DateTime]::UtcNow.ToString('o');$sourceWatch=[Diagnostics.Stopwatch]::StartNew()
 Set-Content -LiteralPath (Join-Path $logDir 'source-time.txt') -Value $sourceUTC -Encoding ASCII
 Wait-Evidence { (Read-SharedText $capOut) -match 'UART_CONTROL SOURCE sent once' } 5 'SOURCE once acknowledgement'
 $runOut=Join-Path $logDir 'runtime.stdout';$runErr=Join-Path $logDir 'runtime.stderr'
 $age=$sourceWatch.Elapsed.TotalSeconds.ToString('F6',[Globalization.CultureInfo]::InvariantCulture)
 $runtime=Start-Hidden 'wsl.exe' @('-d','Ubuntu-22.04','--','bash',$runtimeScript,$age) $runOut $runErr
 $runStart=$watch.Elapsed.TotalSeconds;$runtimeEnd=$runStart+600;$shown=0;$direction=$false
 do {
  Guard
  if($watch.Elapsed.TotalSeconds-$runStart -ge 600){throw 'STOP SSH45/runtime550 parent600s deadline; root cleanup required'}
  $output=Read-SharedText $runOut 2097152
  $errors=Read-SharedText $runErr 2097152
  if([Text.Encoding]::UTF8.GetByteCount($output)+[Text.Encoding]::UTF8.GetByteCount($errors) -gt 2097152){throw 'STOP combined runtime2MiB log cap'}
  if($output.Length -gt $shown){Write-Host ($output.Substring($shown)) -NoNewline;$shown=$output.Length}
  if(!$direction -and $output.Contains('T5_DIRECTION_CHANGE_ALLOWED')){
   $direction=$true
   Write-Host "`n现在仅轻轻改变固定模块方向，眼见Qt六轴/温度/状态/年龄是否真实变化；结束后如实回贴观察，不移动整板/MIPI/风扇。"
   Set-Content -LiteralPath (Join-Path $logDir 'human-confirmation.txt') -Value 'USER_CONFIRMATION_PENDING; report actual visual observation separately' -Encoding ASCII
  }
  Start-Sleep -Milliseconds 200
 } while(!$runtime.HasExited)
 $runtime.WaitForExit()
 $output=Read-SharedText $runOut 2097152
 if($output.Length -gt $shown){Write-Host ($output.Substring($shown)) -NoNewline}
 if($runtime.ExitCode -ne 0){Write-Host (Read-SharedText $runErr 2097152);throw "STOP runtime exit $($runtime.ExitCode); root cleanup required"}
 if(!$output.Contains('SENSOR_T5T6_RUNTIME_EXIT')){throw 'STOP normal runtime release marker absent'}
 Write-Host "`n运行正常结束。请回贴runtime.stdout末尾、runtime.stderr及实际Qt观察；默认冷恢复仍由主控安排。日志：$logDir"
} catch {
 Write-Host "`n$($_.Exception.Message)"
 Write-Host "停止扩大测试；不自动重启或冷恢复。保持接线，回贴此错误和日志目录给主控清理：$logDir"
 throw
} finally {
 # Never release the shared lock while this invocation's bounded remote runtime may remain.
 if($runtime -and !$runtime.HasExited){
  Write-Host '等待本次SSH/远端550秒截止退出，协作锁保持；资源未确认清理，需主控核实。'
  if(!$runtime.WaitForExit([Math]::Max(1,[int](1000*($runtimeEnd-$watch.Elapsed.TotalSeconds))))){
   Stop-Process -Id $runtime.Id -ErrorAction SilentlyContinue
   Write-Host '已停止本次拥有的WSL runtime进程；不能声称板端资源已清理。'
  }
 }
 if($control -and $capture -and !$capture.HasExited){Add-Content -LiteralPath $control -Value 'CANCEL' -Encoding ASCII;$capture.WaitForExit(5000) | Out-Null}
 if($capture -and !$capture.HasExited){Stop-Process -Id $capture.Id -ErrorAction SilentlyContinue}
 if($release){Set-Content -LiteralPath $release -Value 'RELEASE' -Encoding ASCII}
 if($lock){$lock.WaitForExit(5000) | Out-Null}
}
