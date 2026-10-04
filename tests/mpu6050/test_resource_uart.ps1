$ErrorActionPreference='Stop'
# Actual production script with serial factory replaced; no ports opened.
$source=Join-Path $PSScriptRoot '../../scripts/board/mpu6050/capture_resource_dual_uart.ps1'
$script=Get-Content -LiteralPath $source -Raw
$folder=Join-Path ([System.IO.Path]::GetTempPath()) ('resource-uart-fixture-'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $folder | Out-Null
$fake=@'
Add-Type -TypeDefinition @"
using System;
public class ProbeFakeSerial {
 public string PortName;
 public object Handshake; public bool DtrEnable; public bool RtsEnable;
 public int ReadTimeout; public int WriteTimeout;
 public int BytesToRead { get { return 0; } }
 public ProbeFakeSerial(string port,int baud,System.IO.Ports.Parity parity,int bits,System.IO.Ports.StopBits stop){PortName=port;}
 public void Open(){if(PortName==Environment.GetEnvironmentVariable("PROBE_FAKE_DENIED"))throw new UnauthorizedAccessException("fixture occupied "+PortName);}
 public int Read(byte[] buffer,int offset,int count){return 0;}
 public void Write(string data){throw new Exception("fixture unexpected write");}
 public void Dispose(){Console.WriteLine("FAKE_DISPOSE "+PortName);}
}
"@
'@
$script=$script.Replace("`$ErrorActionPreference='Stop'","`$ErrorActionPreference='Stop'`n$fake").Replace('[System.IO.Ports.SerialPort]::new','[ProbeFakeSerial]::new')
$script=$script.Replace('while($watch.Elapsed.TotalSeconds -lt 120)','while($watch.Elapsed.TotalSeconds -lt 0)')
$fixture=Join-Path $folder 'fixture.ps1'
Set-Content -LiteralPath $fixture -Value $script -Encoding UTF8
foreach($mode in @('COM5','COM6','NONE')){
 $env:PROBE_FAKE_DENIED=$mode
 $stdout=Join-Path $folder ($mode+'.stdout');$stderr=Join-Path $folder ($mode+'.stderr')
 $process=Start-Process -FilePath powershell.exe -ArgumentList @('-NoProfile','-NonInteractive','-File',$fixture,'-LinuxLog',(Join-Path $folder ($mode+'-linux.log')),'-M0Log',(Join-Path $folder ($mode+'-m0.log')),'-ControlFile',(Join-Path $folder 'control.txt')) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 $code=$process.ExitCode;$text=Get-Content -LiteralPath $stdout -Raw
 if($mode -eq 'NONE'){
  if($code -ne 0 -or $text -notmatch 'UART_CAPTURE_READY' -or $text -notmatch 'FAKE_DISPOSE COM6'){throw 'normal fixture failed'}
 }else{
  if($code -eq 0 -or $text -match 'UART_CAPTURE_READY' -or $text -notmatch ('FAKE_DISPOSE '+$mode)){throw ('fail-closed fixture failed '+$mode)}
 }
 Write-Output "UART_FACTORY_FIXTURE_PASS mode=$mode exit=$code"
}
Remove-Item Env:PROBE_FAKE_DENIED
Write-Output "UART_FIXTURE_ARTIFACTS $folder"
