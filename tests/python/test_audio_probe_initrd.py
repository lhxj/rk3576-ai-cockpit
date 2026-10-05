import gzip,importlib.util,pathlib,stat,tempfile,time,unittest
R=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('initrd',R/'scripts/dev/package_audio_probe_initrd.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def row(name,content=b'',mode=stat.S_IFREG|0o644):
 f=[1,mode,0,0,1,123,len(content),0,0,0,0,len(name.encode())+1,0];return f,name,content
class Initrd(unittest.TestCase):
 def blob(self,rows):return gzip.compress(b''.join(m.encode(*x) for x in rows)+m.encode([0]*13,'TRAILER!!!',b''),mtime=0)
 def test_bounds_format_duplicate_and_unsafe(self):
  for data in [b'070701',self.blob([row('../escape')]),self.blob([row('/absolute')]),self.blob([row('./alias')]),self.blob([row('foo//alias')]),self.blob([row('nul\0name')]),self.blob([row('x'),row('x')]),self.blob([row('dev',mode=stat.S_IFCHR)])]:
   with self.assertRaises((ValueError,EOFError)):m.entries(data,time.monotonic()+1)
  with self.assertRaisesRegex(ValueError,'deadline'):m.entries(self.blob([row('x')]),0)
 def test_extra_cpio_segment_rejected(self):
  raw=m.encode(*row('x'))+m.encode([0]*13,'TRAILER!!!',b'')+m.encode(*row('second'))
  with self.assertRaisesRegex(ValueError,'extra segment'):m.entries(gzip.compress(raw),time.monotonic()+1)
 def test_module_replacement_nonmodule_metadata_symlink_preserved(self):
  with tempfile.TemporaryDirectory() as td:
   stage=pathlib.Path(td);ko=stage/'kernel/drivers/net/usb/cdc_eem.ko';ko.parent.mkdir(parents=True);ko.write_bytes(b'new longer module')
   original=[row('init',b'unchanged'),row('etc/mtab',b'/proc/mounts',stat.S_IFLNK|0o777),row('usr/lib/modules/'+m.OLD+'/kernel/drivers/net/usb/cdc_eem.ko',b'old')]
   original=m.entries(self.blob(original),time.monotonic()+1)
   paired,changes=m.transform(original,stage,time.monotonic()+1,{'kernel/drivers/net/usb/cdc_eem.ko':(0o644,b'new longer module')});decoded=m.entries(self.blob(paired),time.monotonic()+1)
   m.verify_preservation(original,decoded);self.assertEqual(paired,decoded);self.assertEqual(len(changes),1);self.assertEqual(decoded[1][2],b'/proc/mounts');self.assertEqual(decoded[2][2],b'new longer module')
 def test_missing_stage_or_wrong_module_rejected(self):
  with tempfile.TemporaryDirectory() as td:
   with self.assertRaisesRegex(ValueError,'missing'):m.transform([row('usr/lib/modules/'+m.OLD+'/kernel/drivers/net/usb/cdc_eem.ko')],pathlib.Path(td),time.monotonic()+1,{})
 def test_preservation_gate_is_exact(self):
  original=[row('init',b'original')];modified=[row('init',b'tampered')]
  with self.assertRaisesRegex(ValueError,'non-module'):m.verify_preservation(original,modified)

 def test_same_vermagic_or_metadata_stage_tamper_rejected(self):
  with tempfile.TemporaryDirectory() as td:
   stage=pathlib.Path(td);relative='kernel/drivers/net/usb/cdc_eem.ko';ko=stage/relative;ko.parent.mkdir(parents=True);ko.write_bytes(b'same vermagic different bytes')
   original=[row('usr/lib/modules/'+m.OLD+'/'+relative)]
   with self.assertRaisesRegex(ValueError,'stage differs'):m.transform(original,stage,time.monotonic()+1,{relative:(0o644,b'original pinned bytes')})
   meta=stage/'modules.dep';meta.write_text('tampered dependency')
   with self.assertRaisesRegex(ValueError,'stage differs'):m.transform([row('usr/lib/modules/'+m.OLD+'/modules.dep')],stage,time.monotonic()+1,{'modules.dep':(0o644,b'pinned dependency')})
