"""Actual additive diagnostic installer packet checks; no board writes."""
import hashlib,importlib.util,json,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('resource_installer',ROOT/'scripts/board/mpu6050/install_resource_probe.py')
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
def sha(raw):return hashlib.sha256(raw).hexdigest()
class PacketTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.folder=pathlib.Path(self.tmp.name)/'packet';self.folder.mkdir()
  members=installer.MEMBERS-{'RESOURCE_PROBE.json','SHA256SUMS'}
  for name in members:(self.folder/name).write_bytes(('bounded '+name).encode())
  manifest={'id':'I2C_RESOURCE_PROBE_V1','destination':str(installer.DEST),'files':{n:{'bytes':(self.folder/n).stat().st_size,'sha256':sha((self.folder/n).read_bytes())} for n in members}}
  raw=json.dumps(manifest).encode();(self.folder/'RESOURCE_PROBE.json').write_bytes(raw)
  installer.PIN_MANIFEST=sha(raw)
  (self.folder/'SHA256SUMS').write_text(''.join(sha((self.folder/n).read_bytes())+'  '+n+'\n' for n in sorted(installer.MEMBERS-{'SHA256SUMS'})))
 def test_valid_exact_eight(self):
  data,manifest=installer.validate_packet(self.folder);self.assertEqual(len(data),8);self.assertEqual(manifest['id'],'I2C_RESOURCE_PROBE_V1')
 def test_extra_and_missing(self):
  (self.folder/'extra').write_text('x')
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
  (self.folder/'extra').unlink();(self.folder/'resource-probe.dtb').unlink()
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
 def test_manifest_hash(self):
  (self.folder/'RESOURCE_PROBE.json').write_text('{}')
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
 def test_payload_hash(self):
  (self.folder/'amp-signed.itb').write_text('changed')
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
 def test_file_symlink(self):
  path=self.folder/'amp-signed.itb';path.unlink();path.symlink_to(self.folder/'stage-resource-probe.scr')
  with self.assertRaises(OSError):installer.validate_packet(self.folder)
 def test_directory_symlink(self):
  path=self.folder.parent/'alias';path.symlink_to(self.folder)
  with self.assertRaises(RuntimeError):installer.validate_packet(path)
 def test_checksum_mismatch(self):
  (self.folder/'SHA256SUMS').write_text('wrong')
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
 def test_file_size_limit(self):
  (self.folder/'resource-probe.dtb').write_bytes(b'x'*(installer.MAX_FILE+1))
  with self.assertRaises(RuntimeError):installer.validate_packet(self.folder)
if __name__=='__main__':unittest.main()
