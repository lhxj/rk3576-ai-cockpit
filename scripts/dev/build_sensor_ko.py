#!/usr/bin/env python3
"""Host-only paired Kbuild; no installation or board actions."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
AMP=pathlib.Path('/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project')
SI=pathlib.Path('/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system/artifacts/local/system-board-validation-20261004')
KERNEL=AMP/'artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208'
PREPARED=SI/'health-ko-v2/prepared-kernel'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);out=ap.parse_args().output.resolve()
 if not out.is_relative_to(ROOT/'artifacts/local') or out.exists():raise SystemExit('fresh local output required')
 pins=json.loads((ROOT/'patches/mpu6050/kernel-resource-inputs.json').read_text())
 for n,h in pins['files'].items():assert sha(KERNEL/n)==h,n
 original=json.loads((ROOT/'docs/bringup/mpu6050/RESOURCE_PROBE_HOST_PACKAGE.json').read_text())
 assert sha(PREPARED/'.config')==original['prepared_config_sha256']
 assert sha(PREPARED/'Module.symvers')==original['prepared_symvers_sha256']
 out.mkdir();prepared=out/'prepared-kernel';prepared.mkdir()
 for n in ('.config','Makefile','Module.symvers'):shutil.copyfile(PREPARED/n,prepared/n)
 for n in ('include','scripts','arch/arm64/include/generated'):shutil.copytree(PREPARED/n,prepared/n)
 module=out/'module';shutil.copytree(ROOT/'apps/rpmsg_srv/kernel',module)
 deps=AMP/'artifacts/local/p023-standard-tools/root'
 env=dict(os.environ,PATH=str(deps/'usr/bin')+':'+os.environ['PATH'],BISON_PKGDATADIR=str(deps/'usr/share/bison'),M4=str(deps/'usr/bin/m4'),CPATH=str(deps/'usr/include'),LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'),LD_LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'))
 with (out/'build.log').open('wb') as f:subprocess.run(['make','-C',str(KERNEL),'O='+str(prepared),'M='+str(module),'ARCH=arm64','CROSS_COMPILE=aarch64-linux-gnu-','modules'],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180,check=True)
 ko=module/'rk3576_sensor.ko'
 info=subprocess.check_output(['modinfo',str(ko)],text=True);(out/'modinfo.txt').write_text(info)
 assert '6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64' in info
 imported=subprocess.check_output(['aarch64-linux-gnu-nm','-u',str(ko)],text=True);(out/'imported-symbols.txt').write_text(imported)
 symbols={line.split()[1]:line.split()[0].lower() for line in (PREPARED/'Module.symvers').read_text().splitlines()}
 for line in imported.splitlines():assert line.split()[-1] in symbols,line
 assert '# CONFIG_MODVERSIONS is not set' in (PREPARED/'.config').read_text()
 result={'kind':'SENSOR_LINUX_KO_HOST_BUILD','board_run':False,'deployable':False,'kernel_commit':'521833e2d28decbd6473d5717f1f96cc4108e208','vermagic':'6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64','prepared_config_sha256':sha(PREPARED/'.config'),'prepared_symvers_sha256':sha(PREPARED/'Module.symvers'),'source':{n.name:sha(n) for n in (ROOT/'apps/rpmsg_srv/kernel').iterdir() if n.is_file()},'ko':{'bytes':ko.stat().st_size,'sha256':sha(ko)},'imported_symbols_exported':True,'crc_verification':'NOT_APPLICABLE_CONFIG_MODVERSIONS_N'}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(out/'manifest.json')
if __name__=='__main__':main()
