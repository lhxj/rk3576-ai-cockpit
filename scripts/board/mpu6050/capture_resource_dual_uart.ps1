param(
 [Parameter(Mandatory=$true)][string]$LinuxLog,
 [Parameter(Mandatory=$true)][string]$M0Log,
 [Parameter(Mandatory=$true)][string]$ControlFile,
 [switch]$InterruptColdBoot
)
$ErrorActionPreference='Stop'
$captureFailed=$false
# One process owns each port once. root writes tokens only after reviewing logs.
# No I2C commands, no DTR/RTS, no automatic source/reset/retry/shutdown.
$ports=@();$streams=@();$totals=@(0,0);$recent=@('','');$seenCommands=0
$interrupted=$false;$loaded=$false;$inspected=$false;$sourced=$false;$cold=$false
$buffer=New-Object byte[] 4096
$watch=[System.Diagnostics.Stopwatch]::StartNew()
try {
 foreach($entry in @(@('COM5',1500000,$LinuxLog),@('COM6',115200,$M0Log))){
  $port=[System.IO.Ports.SerialPort]::new($entry[0],$entry[1],[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
  $port.Handshake=[System.IO.Ports.Handshake]::None;$port.DtrEnable=$false;$port.RtsEnable=$false;$port.ReadTimeout=50;$port.WriteTimeout=1000
  $ports+=,$port
  $streams+=,[System.IO.File]::Open($entry[2],[System.IO.FileMode]::CreateNew,[System.IO.FileAccess]::Write)
  $port.Open() # AccessDenied => stop; never kill another serial owner.
 }
 Write-Output "UART_CAPTURE_READY COM5=1500000_8N1 COM6=115200_8N1 noflow DTR=false RTS=false startup_limit120s total_limit240s per_uart_cap262144B"
 while($watch.Elapsed.TotalSeconds -lt 120){
  for($i=0;$i -lt 2;$i++){
   if($ports[$i].BytesToRead -gt 0){
    $n=$ports[$i].Read($buffer,0,[Math]::Min(4096,$ports[$i].BytesToRead))
    if($totals[$i]+$n -gt 262144){throw 'STOP per-UART 256KiB cap; root must stop diagnostic and cold recover'}
    $streams[$i].Write($buffer,0,$n);$streams[$i].Flush();$totals[$i]+=$n
    $recent[$i]+=[System.Text.Encoding]::ASCII.GetString($buffer,0,$n)
    if($recent[$i].Length -gt 32768){$recent[$i]=$recent[$i].Substring($recent[$i].Length-32768)}
   }
  }
  if($recent[0] -match 'soc cold boot'){$cold=$true}
  if($InterruptColdBoot -and !$interrupted -and !$sourced -and $recent[0] -match 'Hit any key to stop autoboot'){
   $ports[0].Write([string][char]27);$interrupted=$true
   Write-Output 'UART_CONTROL interrupt sent once on observed autoboot prompt'
  }
  if(Test-Path -LiteralPath $ControlFile){
   $item=Get-Item -LiteralPath $ControlFile
   if($item.Length -gt 1024){throw 'STOP control-file size limit'}
   $lines=@(Get-Content -LiteralPath $ControlFile)
   while($seenCommands -lt $lines.Count){
    $command=$lines[$seenCommands].Trim();$seenCommands++
    if(!$command){continue}
    switch -Exact ($command){
     'LOAD' {
      if(!$cold -or !$interrupted -or $loaded -or $sourced){throw 'STOP LOAD requires observed cold boot and interrupted prompt; one attempt'}
      $ports[0].Write("load mmc 0:2 0x4c000000 /amp-p029/i2c-resource-probe-v1/stage-resource-probe.scr`r")
      $loaded=$true;$recent[0]='';Write-Output 'UART_CONTROL LOAD sent once'
     }
     'INSPECT' {
      if(!$loaded -or $inspected -or $sourced){throw 'STOP INSPECT ordering'}
      $ports[0].Write("printenv fileaddr filesize`r")
      $inspected=$true;Write-Output 'UART_CONTROL INSPECT sent once; root must verify identity and length'
     }
     'SOURCE' {
      if(!$loaded -or !$inspected -or $sourced -or $recent[0] -notmatch '(?m)^fileaddr=(?:0x)?4c000000\s*$' -or $recent[0] -notmatch '(?m)^filesize=(?:0x)?c98\s*$'){
       throw 'STOP SOURCE requires latest actual fileaddr=4c000000 and filesize=c98; no retry'
      }
      $ports[0].Write("source 0x4c000000`r")
      $sourced=$true;$watch.Restart();Write-Output 'UART_CONTROL SOURCE sent once; diagnostic capture deadline120s begins'
     }
     default {throw 'STOP unknown control token'}
    }
   }
  }
  Start-Sleep -Milliseconds 20
 }
 Write-Output "DUAL_UART_CAPTURE_COMPLETE bytes_COM5=$($totals[0]) bytes_COM6=$($totals[1]) source_once=$sourced; no board recovery performed"
}catch{
 $captureFailed=$true
 [Console]::Error.WriteLine("UART_CAPTURE_STOP: " + $_.Exception.Message)
}finally{
 foreach($port in $ports){$port.Dispose()}
 foreach($stream in $streams){$stream.Dispose()}
}

if($captureFailed){exit 1}
