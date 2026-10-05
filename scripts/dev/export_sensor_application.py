#!/usr/bin/env python3
"""L0 source tar export with exact members/hash; no board access."""
import argparse,hashlib,io,json,pathlib,subprocess,tarfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
def main():
 a=argparse.ArgumentParser();a.add_argument('--output',type=pathlib.Path,required=True);out=a.parse_args().output.resolve()
 if not out.is_relative_to(ROOT/'artifacts/local') or out.exists():raise SystemExit('fresh local output required')
 out.mkdir();names=subprocess.check_output(['git','ls-files','apps','libs','tests','tools','cmake','config','rtos','CMakeLists.txt','CMakePresets.json'],cwd=ROOT,text=True).splitlines()
 names+=['apps/cockpit_ui/tests/sensor_stream_probe.cpp','apps/cockpit_ui/tests/sensor_ui_probe.cpp','apps/cockpit_ui/tests/sensor_coexistence_probe.cpp'];names=sorted(set(names));members={}
 with tarfile.open(out/'application-source.tar.gz','w:gz') as archive:
  for name in names:
   path=ROOT/name
   if not path.is_file() or path.is_symlink():raise SystemExit('non regular '+name)
   data=path.read_bytes();assert len(data)<2*1024*1024
   info=tarfile.TarInfo('src/'+name);info.size=len(data);info.mode=0o644;archive.addfile(info,io.BytesIO(data));members['src/'+name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 source=out/'application-source.tar.gz';m={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_export':True,'archive_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'archive_bytes':source.stat().st_size,'members':members}
 (out/'application-source.json').write_text(json.dumps(m,indent=2)+'\n');print(out/'application-source.json')
if __name__=='__main__':main()
