$ErrorActionPreference='Stop'
# Actual production script with serial factory replaced; no ports opened.
$source=Join-Path $PSScriptRoot '../../scripts/board/mpu6050/capture_resource_dual_uart.ps1'
$script=Get-Content -LiteralPath $source -Raw
$folder=Join-Path ([System.IO.Path]::GetTempPath()) ('resource-uart-fixture-'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $folder | Out-Null
$fake=@'
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
public class ProbeFakeClock {
 static int made; int id; int queries;
 public static ProbeFakeClock StartNew(){return new ProbeFakeClock{id=made++};}
 public TimeSpan Elapsed { get {
  string mode=Environment.GetEnvironmentVariable("PROBE_FAKE_DENIED");
  if(id==1)return TimeSpan.FromSeconds(mode=="ABSOLUTE" ? 541 : 0);
  if(mode=="COLD" || mode=="SOURCE" || mode=="WRONG_PROMPT")return TimeSpan.FromSeconds(ProbeFakeSerial.Reads >= (mode=="SOURCE" ? 3 : 2) ? 121 : 0);
  return TimeSpan.FromSeconds(queries++==0 ? 0 : 301);
 }}
 public void Restart(){Console.WriteLine("FAKE_CLOCK_RESTART");}
}
public class ProbeFakeSerial {
 public static int Reads; public string PortName; string pending;
 public object Handshake; public bool DtrEnable; public bool RtsEnable;
 public int ReadTimeout; public int WriteTimeout;
 string Mode { get {return Environment.GetEnvironmentVariable("PROBE_FAKE_DENIED");} }
 string Next { get {
  if(PortName!="COM5")return "";
  if(Mode=="COLD" && Reads<2)return "soc cold boot\r\n";
  if(Mode=="WRONG_PROMPT" && Reads<2)return "soc cold boot\r\nHit any key to stop autoboot\r\n";
  if(Mode=="SOURCE" && Reads==0)return "soc cold boot\r\nHit key to stop autoboot('CTRL+C'): 3\r\n";
  return pending ?? "";
 }}
 public int BytesToRead { get {return Next.Length;} }
 public ProbeFakeSerial(string port,int baud,System.IO.Ports.Parity parity,int bits,System.IO.Ports.StopBits stop){PortName=port;}
 public void Open(){if(PortName==Mode)throw new UnauthorizedAccessException("fixture occupied "+PortName);if(PortName=="COM5" && Mode=="CANCEL")File.WriteAllText(Environment.GetEnvironmentVariable("PROBE_FAKE_CONTROL"),"CANCEL\n");}
 public int Read(byte[] buffer,int offset,int count){
  byte[] bytes=Encoding.ASCII.GetBytes(Next);Array.Copy(bytes,0,buffer,offset,bytes.Length);Reads++;pending=null;
  if(Mode=="SOURCE")File.AppendAllText(Environment.GetEnvironmentVariable("PROBE_FAKE_CONTROL"),(Reads==1 ? "LOAD" : Reads==2 ? "INSPECT" : "SOURCE")+"\n");
  return bytes.Length;
 }
 public void Write(string data){
  if(Mode!="SOURCE")throw new Exception("fixture unexpected write");
  if(data==((char)3).ToString()){Console.WriteLine("FAKE_CTRLC_ONCE");return;}
  if(data.StartsWith("load mmc")){pending="3224 bytes read in 1 ms\r\nHit key to stop autoboot('CTRL+C'): 2\r\n";return;}
  if(data.StartsWith("printenv")){pending="fileaddr=4c000000\r\nfilesize=c98\r\n";return;}
  if(data.StartsWith("source")){Console.WriteLine("FAKE_SOURCE_ONCE");return;}
  throw new Exception("unexpected command");
 }
 public void Dispose(){Console.WriteLine("FAKE_DISPOSE "+PortName);}
}
"@
'@
$script=$script.Replace("`$ErrorActionPreference='Stop'","`$ErrorActionPreference='Stop'`n$fake").Replace('[System.IO.Ports.SerialPort]::new','[ProbeFakeSerial]::new').Replace('[System.Diagnostics.Stopwatch]::StartNew()','[ProbeFakeClock]::StartNew()')
if(([regex]::Matches($script,[regex]::Escape('while($watch.Elapsed.TotalSeconds -lt $phaseLimit -and $totalWatch.Elapsed.TotalSeconds -lt 540)'))).Count -ne 1){throw 'actual phase loop not found exactly once'}
$script=$script.Replace('Start-Sleep -Milliseconds 20','# Fixture fake-clock step; no wall-clock sleep')
$fixture=Join-Path $folder 'fixture.ps1'
Set-Content -LiteralPath $fixture -Value $script -Encoding UTF8
foreach($mode in @('COM5','COM6','NONE','COLD','SOURCE','ABSOLUTE','WRONG_PROMPT','CANCEL')){
 $env:PROBE_FAKE_DENIED=$mode
 $env:PROBE_FAKE_CONTROL=Join-Path $folder ($mode+'-control.txt')
 $stdout=Join-Path $folder ($mode+'.stdout');$stderr=Join-Path $folder ($mode+'.stderr')
 $process=Start-Process -FilePath powershell.exe -ArgumentList @('-NoProfile','-NonInteractive','-File',$fixture,'-LinuxLog',(Join-Path $folder ($mode+'-linux.log')),'-M0Log',(Join-Path $folder ($mode+'-m0.log')),'-ControlFile',$env:PROBE_FAKE_CONTROL,'-InterruptColdBoot') -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 $code=$process.ExitCode;$text=Get-Content -LiteralPath $stdout -Raw
 if($mode -notin @('COM5','COM6')){
  if($code -ne 0 -or $text -notmatch 'UART_CAPTURE_READY' -or $text -notmatch 'FAKE_DISPOSE COM6'){throw 'normal fixture failed'}
 }else{
  if($code -eq 0 -or $text -match 'UART_CAPTURE_READY' -or $text -notmatch ('FAKE_DISPOSE '+$mode)){throw ('fail-closed fixture failed '+$mode)}
 }
 if($mode -eq 'COLD'){
  if(([regex]::Matches($text,'FAKE_CLOCK_RESTART')).Count -ne 1 -or $text -notmatch 'phase=COLD_STARTUP'){throw 'first-cold-only transition failed'}
 }
 if($mode -eq 'SOURCE'){
  if(([regex]::Matches($text,'FAKE_CLOCK_RESTART')).Count -ne 2 -or $text -notmatch 'phase=DIAGNOSTIC' -or ([regex]::Matches($text,'FAKE_SOURCE_ONCE')).Count -ne 1 -or ([regex]::Matches($text,'FAKE_CTRLC_ONCE')).Count -ne 1){throw 'SOURCE phase guard failed'}
 }
 if($mode -eq 'NONE' -or $mode -eq 'ABSOLUTE'){
  if($text -match 'FAKE_CLOCK_RESTART' -or $text -notmatch 'phase=WAIT_USER_POWER_ACTION'){throw 'manual/absolute expiry guard failed'}
 }
 if($mode -eq 'WRONG_PROMPT'){
  if($text -match 'FAKE_CTRLC_ONCE' -or $text -match 'FAKE_SOURCE_ONCE'){throw 'wrong prompt caused console write'}
 }
 if($mode -eq 'CANCEL'){
  if($text -notmatch 'UART_CAPTURE_CANCELLED' -or $text -match 'FAKE_CTRLC_ONCE' -or $text -match 'FAKE_SOURCE_ONCE'){throw 'CANCEL guard failed'}
 }
 Write-Output "UART_FACTORY_FIXTURE_PASS mode=$mode exit=$code"
}
Remove-Item Env:PROBE_FAKE_DENIED
Remove-Item Env:PROBE_FAKE_CONTROL
Write-Output "UART_FIXTURE_ARTIFACTS $folder"
