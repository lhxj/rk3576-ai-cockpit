import importlib.util,json,os,pathlib,tempfile,unittest
from unittest.mock import patch
R=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('audio_build',R/'scripts/dev/build_audio_probe_diagnostic.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Builder(unittest.TestCase):
 def test_actual_inputs_and_patch_identity(self):
  pins=m.verify_inputs();self.assertEqual(pins['source_sha256'],'217dccf7e8a56952ce30f7234a7e798ae7ab5b5e178846229640e118acd3492c')
  self.assertEqual(pins['inputs']['config']['sha256'],'58c9891ae953922ced41508ad4e1f91a480d1100d42c012b6be8038d1ddd9436')
 def test_config_only_release_allowed(self):
  before='CONFIG_LOCALVERSION="-rk3576-m0echo-p026"\n# CONFIG_LOCALVERSION_AUTO is not set\nCONFIG_MODULES=y\n'
  after=before.replace('-rk3576-m0echo-p026','-rk3576-audioprobe-d1')
  self.assertEqual(set(m.config_changes(before,after)),{'CONFIG_LOCALVERSION'})
  with self.assertRaisesRegex(ValueError,'Kconfig'):m.config_changes(before,after.replace('CONFIG_MODULES=y','CONFIG_MODULES=n'))
 def test_fresh_output_escape_existing_disk(self):
  with tempfile.TemporaryDirectory() as td:
   root=pathlib.Path(td);(root/'artifacts/local').mkdir(parents=True)
   with patch.object(m,'ROOT',root):
    with self.assertRaises(ValueError):m.fresh_output(root/'outside')
    with self.assertRaises(ValueError):m.fresh_output(root/'artifacts/local')
    with patch.object(m.shutil,'disk_usage',return_value=type('Disk',(),{'free':0})()):
     with self.assertRaisesRegex(ValueError,'16GiB'):m.fresh_output(root/'artifacts/local/new')
 def test_source_escape_and_deadline(self):
  with tempfile.TemporaryDirectory() as td:
   root=pathlib.Path(td);src=root/'src';src.mkdir();(root/'external').write_text('secret');(src/'link').symlink_to(root/'external')
   with self.assertRaisesRegex(ValueError,'symlink escape'):m.copy_source(src,root/'new',m.time.monotonic()+1)
   (src/'link').unlink();(src/'file').write_text('exact')
   with self.assertRaisesRegex(ValueError,'deadline'):m.copy_source(src,root/'new',0)
   result=m.copy_source(src,root/'new',m.time.monotonic()+1);self.assertEqual(result['bytes'],5);self.assertEqual((root/'new/file').read_text(),'exact')
 def test_owned_command_cap_and_timeout(self):
  with tempfile.TemporaryDirectory() as td:
   runner=m.Runner(pathlib.Path(td),dict(os.environ))
   with patch.object(m,'LOG_CAP',64):
    with self.assertRaisesRegex(ValueError,'log cap'):runner.run(['/usr/bin/python3','-c','print("x"*10000)'],'cap.log',2)
   with self.assertRaisesRegex(ValueError,'deadline'):runner.run(['/usr/bin/python3','-c','import time;time.sleep(10)'],'timeout.log',.1)
   runner.run(['/usr/bin/python3','-c','print("HOST_ONLY")'],'normal.log',2);self.assertIn('HOST_ONLY',(pathlib.Path(td)/'normal.log').read_text())
 def test_make_commands_isolated(self):
  runner=m.Runner(pathlib.Path('/task/out'),{});seen=[]
  with patch.object(runner,'run',side_effect=lambda argv,name,seconds:seen.append(argv)):
   runner.make(pathlib.Path('/task/out/src'),pathlib.Path('/task/out/build'),'INSTALL_MOD_PATH=/task/out/stage','DEPMOD=true','modules_install',name='stage.log')
  self.assertIn('O=/task/out/build',seen[0]);self.assertIn('INSTALL_MOD_PATH=/task/out/stage',seen[0]);self.assertNotIn('/lib/modules',seen[0])

 def test_postprocessing_hash_and_copy_deadline(self):
  with tempfile.TemporaryDirectory() as td:
   root=pathlib.Path(td);src=root/'src';src.mkdir();(src/'large').write_bytes(b'x'*(2*1024*1024))
   with self.assertRaisesRegex(ValueError,'postprocessing deadline'):m.sha(src/'large',0)
   with self.assertRaisesRegex(ValueError,'postprocessing deadline'):m.copy_checked_tree(src,root/'headers',0,root)
   with patch.object(pathlib.Path,'read_bytes',side_effect=AssertionError('whole-file read forbidden')):
    self.assertEqual(len(m.sha(src/'large',m.time.monotonic()+1)),64)
   copied=m.copy_source(src,root/'copied',m.time.monotonic()+2)
   audit=json.loads((root/copied['catalog']).read_text());self.assertEqual(audit['large']['sha256'],m.sha(src/'large'))
   self.assertEqual(copied['catalog_sha256'],m.sha(root/copied['catalog']))
