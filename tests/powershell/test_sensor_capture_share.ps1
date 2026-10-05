param([Parameter(Mandatory=$true)][string]$Collector)
$ErrorActionPreference='Stop'
$content=[System.IO.File]::ReadAllText($Collector)
$lines=@($content -split "`n" | Where-Object {$_ -match '\$streams\+=,\[System.IO.File\]::Open'})
if($lines.Count -ne 1 -or $lines[0] -notmatch '\[System.IO.FileShare\]::Read\)'){throw 'actual writer expression not shared-read'}
$testPath=Join-Path ([System.IO.Path]::GetTempPath()) ('sensor-share-'+[guid]::NewGuid().ToString()+'.log')
$writer=$null;$reader=$null
try {
 $expression=$lines[0].Substring($lines[0].IndexOf('+=,')+3).Replace('$entry[2]','$testPath')
 $writer=& ([scriptblock]::Create($expression))
 $raw=[Text.Encoding]::ASCII.GetBytes('SENSOR_CAPTURE_READY')
 $writer.Write($raw,0,$raw.Length);$writer.Flush()
 # Reader explicitly shares existing writer; a second writer is still rejected.
 $reader=[IO.File]::Open($testPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 $buffer=New-Object byte[] $raw.Length
 if($reader.Read($buffer,0,$buffer.Length) -ne $raw.Length -or [Text.Encoding]::ASCII.GetString($buffer) -ne 'SENSOR_CAPTURE_READY'){throw 'live read lost flushed bytes'}
 $secondWriter=$null;$refused=$false
 try {$secondWriter=[IO.File]::Open($testPath,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)} catch [System.IO.IOException] {$refused=$true}
 finally {if($secondWriter){$secondWriter.Dispose()}}
 if(!$refused){throw 'second writer unexpectedly permitted'}
 'SENSOR_COLLECTOR_ACTUAL_FILESTREAM_LIVE_READ_PASS second_writer_refused=True'
} finally {
 if($reader){$reader.Dispose()};if($writer){$writer.Dispose()}
 if([IO.File]::Exists($testPath)){[IO.File]::Delete($testPath)}
}
