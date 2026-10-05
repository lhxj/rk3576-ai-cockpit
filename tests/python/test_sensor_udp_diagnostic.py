import importlib.util,io,json,os,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
P=ROOT/'scripts/board/mpu6050/run_sensor_coexistence_v5.py'
spec=importlib.util.spec_from_file_location('diag',P);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Diagnostic(unittest.TestCase):
 def test_timeline_offsets_first_frame_partial_and_error_preserved(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);err=p/'rtsp-decoder.log';progress=p/'rtsp-progress.log';err.write_text('CABAC error\n');progress.write_text('fra')
   output=io.StringIO();timeline=m.DecoderTimeline(p,output,lambda:3.0);timeline.poll('START');progress.write_text('frame=4\n');timeline.poll('FULL_STAGE_OBSERVED');timeline.poll('SHUTDOWN')
   rows=[json.loads(x) for x in output.getvalue().splitlines()]
   self.assertEqual(sum(x['kind']=='first_decoded_frame_observed' for x in rows),1)
   self.assertEqual(err.read_text(),'CABAC error\n');self.assertEqual(rows[0]['phase'],'START');self.assertEqual(rows[0]['offset'],0)
 def test_bounds(self):
  output=io.StringIO();timeline=m.DecoderTimeline(pathlib.Path('.'),output);timeline.bytes=m.MAX_LOG-10
  with self.assertRaises(RuntimeError):timeline.emit('error','START',text='abc')
  self.assertEqual(output.getvalue(),'')
 def test_udp_owned_ipv4_ipv6_baseline(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'net').mkdir();(p/'7/fd').mkdir(parents=True)
   (p/'net/snmp').write_text('Udp: InErrors RcvbufErrors\nUdp: 9 4\n')
   row='1: 0100007F:1234 00000000:0000 07 00000000:0000002A 00:00000000 00000000 1000 0 12345 2 00000000 6\n'
   for family in ['udp','udp6']:(p/'net'/family).write_text('header\n'+row+'malformed\n')
   os.symlink('socket:[12345]',p/'7/fd/3')
   before=m.udp_snapshot(None,p);self.assertEqual(before['global_udp']['RcvbufErrors'],4);self.assertNotIn('read_error',before)
   result=m.udp_snapshot(7,p);self.assertEqual(len(result['owned_udp']),2);self.assertEqual(result['owned_udp'][0]['drops'],6);self.assertEqual(result['owned_udp'][0]['rx_queue_bytes'],42)
   self.assertEqual(result['rtp_sequence_evidence'],'unavailable');self.assertIn('read_error',m.udp_snapshot(99,p))
 def test_udp_proc_cap(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'net').mkdir();(p/'net/snmp').write_bytes(b'x'*(512*1024+1))
   with self.assertRaises(RuntimeError):m.udp_snapshot(None,p)
 def test_original_gates_and_args_retained(self):
  old=(P.with_name('run_sensor_coexistence_v4.py')).read_text();new=P.read_text()
  for line in old.splitlines():
   if "need(" in line and 'new evidence output' not in line:self.assertIn(line,new)
  self.assertIn("'-rtsp_transport','udp'",new);self.assertIn("'-loglevel','error'",new)
  self.assertLess(new.index("timeline.emit('udp_baseline_before_decoder'"),new.index("client=subprocess.Popen(['ffmpeg'"))
if __name__=='__main__':unittest.main()
