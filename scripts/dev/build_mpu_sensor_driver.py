#!/usr/bin/env python3
"""Host-only MPU_SENSOR_V1 driver milestone; no resource control bridge or deployment."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
AMP=pathlib.Path('/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project')
SYSTEM=pathlib.Path('/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system')
SI=SYSTEM/'artifacts/local/system-board-validation-20261004'
BASE=SI/'persistent-host-v2/source'
TOOLCHAIN=pathlib.Path('/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin')
TOOLS=AMP/'artifacts/local/p028-build-final-v4/tools'
CONTROL=AMP/'artifacts/local/p030-final-fdt-package-v1/fdt.bin'
KEYDIR=AMP/'artifacts/local/p029-signed-v8/private'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,cwd,log,env=None):
 with log.open('wb') as f:subprocess.run([str(x) for x in args],cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180,check=True)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);a=ap.parse_args();out=a.output.resolve()
 if not out.is_relative_to(ROOT/'artifacts/local') or out.exists():raise SystemExit('new local output required')
 inputs=json.loads((ROOT/'patches/mpu6050/resource-probe-inputs.json').read_text())
 for name,want in inputs['files'].items():
  if sha(BASE/name)!=want:raise SystemExit('SI input mismatch '+name)
 source_manifest=json.loads((ROOT/'patches/mpu6050/source-inputs.json').read_text())
 for name,want in source_manifest['hal_files'].items():
  if sha(BASE/'bsp/rockchip/common/hal'/name)!=want:raise SystemExit('HAL input mismatch '+name)
 frozen={str(BASE/n):sha(BASE/n) for n in inputs['files']}
 out.mkdir();tree=out/'rtos'
 shutil.copytree(BASE,tree,symlinks=True,ignore=shutil.ignore_patterns('.git','build','*.o','*.elf','*.bin','*.map','.sconsign*','*.pyc','__pycache__'))
 for p in tree.rglob('*'):
  if p.is_symlink() and not p.resolve().is_relative_to(tree):raise SystemExit('escaping SDK link')
 patches=['0001-i2c9-deferred-held-clock.patch','0002-i2c9-deferred-reset-deassert.patch','0003-i2c9-intmux-gate-owner.patch']
 for name in patches:run(['patch','-p1','--batch','--forward','--fuzz=0','-i',ROOT/'patches/mpu6050'/name],tree,out/(name+'.log'))
 bsp=tree/'bsp/rockchip/rk3576-mcu'
 for name in ['mpu6050.h','mpu6050.c','sensor_task.c']:shutil.copyfile(ROOT/'rtos/sensor'/name,bsp/'applications'/name)
 p=bsp/'applications/SConscript';text=p.read_text().replace("src     = ['main.c', 'amp_echo.c']","src     = ['main.c', 'amp_echo.c', 'mpu6050.c', 'sensor_task.c']");assert 'sensor_task.c' in text;p.write_text(text)
 p=bsp/'.config';text=p.read_text().replace('# CONFIG_RT_USING_I2C is not set','CONFIG_RT_USING_I2C=y');p.write_text(text+'\nCONFIG_RT_USING_I2C9=y\nCONFIG_MPU_SENSOR_V1_I2C9_OWNERSHIP=y\nCONFIG_MPU_SENSOR_V1=y\n')
 with (bsp/'Kconfig').open('a') as f:f.write('\nconfig MPU_SENSOR_V1\n    bool "MPU6050 bounded sampling driver milestone"\n    depends on MPU_SENSOR_V1_I2C9_OWNERSHIP\n    default n\n')
 with (bsp/'rtconfig.py').open('a') as f:f.write("\nLFLAGS += ' -Wl,-u,mpu_sensor_resource_ready '\n")
 env=dict(os.environ,RTT_ROOT=str(tree),RTT_CC='gcc',RTT_EXEC_PATH=str(TOOLCHAIN));env.pop('BSP_ROOT',None);env.pop('PKGS_ROOT',None)
 run(['scons','--useconfig=.config'],bsp,out/'config.log',env);run(['scons','-j4'],bsp,out/'build.log',env)
 h=(bsp/'rtconfig.h').read_text()
 for sym in ['RT_USING_I2C','RT_USING_I2C9','MPU_SENSOR_V1_I2C9_OWNERSHIP','MPU_SENSOR_V1']:assert '#define '+sym+'\n' in h
 for i in range(9):assert '#define RT_USING_I2C'+str(i)+'\n' not in h
 for tool,args in [('size',[]),('nm',[]),('readelf',['-l','-S'])]:run([TOOLCHAIN/('arm-none-eabi-'+tool),*args,bsp/'rtthread.elf'],bsp,out/('elf-'+tool+'.txt'))
 signed=out/'signed';signed.mkdir();shutil.copyfile(bsp/'rtthread.bin',signed/'rttmcu.bin')
 import re
 its=(AMP/'artifacts/local/p030-m0-initdiag-v1/vendor-fit-v2/amp-signed.its').read_text().replace('P029 development signed cold BUS M0','MPU_SENSOR_V1 driver milestone NOT DEPLOYABLE')
 binary=(bsp/'rtthread.bin').read_bytes();total=0x1000+((len(binary)+511)&~511)
 its=re.sub(r'totalsize = <0x[0-9a-f]+>;',f'totalsize = <0x{total:x}>;',its,count=1);(signed/'amp-signed.its').write_text(its)
 for name,want in {'mkimage':'988a2b46efb8dc21710b82a0b856496dbc13fa2b0fc194f3f947f1ba35a61f83','fit_check_sign':'3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8'}.items():assert sha(TOOLS/name)==want
 assert sha(CONTROL)=='43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce'
 run([TOOLS/'mkimage','-f','amp-signed.its','-E','-p','0x1000','-k',KEYDIR,'-r','amp-signed.itb'],signed,out/'sign.log')
 run([TOOLS/'fit_check_sign','-f',signed/'amp-signed.itb','-k',CONTROL,'-s'],signed,out/'verify.log');assert 'Signature check OK' in (out/'verify.log').read_text()
 sys.path.insert(0,str(ROOT/'scripts/amp'));from validate_host_proposal import fdt
 fit=(signed/'amp-signed.itb').read_bytes();nodes=fdt(fit)
 baseline=fdt((SI/'persistent-package-v1/packet/amp-signed.itb').read_bytes())
 props=nodes['/images/mcu'];assert {k:v for k,v in props.items() if k!='data-size'}=={k:v for k,v in baseline['/images/mcu'].items() if k!='data-size'}
 assert len(fit)<=0x90000 and len(binary)<=0x80000
 assert int.from_bytes(props['data-size'],'big')==len(binary) and int.from_bytes(props['data-position'],'big')==0x1000
 assert fit[0x1000:0x1000+len(binary)]==binary and int.from_bytes(nodes['/']['totalsize'],'big')==len(fit)
 assert nodes['/images/mcu/hash']['value'].hex()==hashlib.sha256(binary).hexdigest()
 assert set(nodes['/configurations/conf/signature']['hashed-nodes'].rstrip(b'\0').decode().split('\0'))=={'/','/configurations','/configurations/conf','/images/mcu','/images/mcu/hash'}
 assert sha(bsp/'gcc_link.ld')==sha(BASE/'bsp/rockchip/rk3576-mcu/gcc_link.ld')
 symbols={line.split()[-1]:int(line.split()[0],16) for line in (out/'elf-nm.txt').read_text().splitlines() if len(line.split())==3 and re.fullmatch('[0-9a-fA-F]+',line.split()[0])}
 assert symbols['_sstack']==0x7fc00 and symbols['_estack']==0x80000
 assert symbols['__bss_end__']==symbols['__sram_heap_start__'] and symbols['__sram_heap_start__']<symbols['__sram_heap_end__']==0x7fc00
 elf=(bsp/'rtthread.elf').read_bytes()
 import struct
 assert elf[:4]==b'\x7fELF' and elf[4:6]==b'\x01\x01'
 entry,phoff=struct.unpack_from('<II',elf,24);phsize,phnum=struct.unpack_from('<HH',elf,42)
 load_segments=[]
 for i in range(phnum):
  kind,offset,vaddr,paddr,filesz,memsz,flags,align=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if kind==1:
   assert (vaddr+memsz<=0x80000) or (filesz==0 and (vaddr,memsz) in [(0x20000000,0x2000),(0x21000000,0x10000),(0x21010000,0x10000)])
   load_segments.append({'virtual':hex(vaddr),'physical':hex(paddr),'filesz':filesz,'memsz':memsz})
 assert entry<0x80000
 budget={'elf_entry':hex(entry),'load_segments':load_segments,'bss_end':hex(symbols['__bss_end__']),'heap_bytes':symbols['__sram_heap_end__']-symbols['__sram_heap_start__'],'main_stack_bytes':1024,'sensor_thread_stack_bytes':2048,'fit_bytes':len(fit),'image_bytes':len(binary),'linker_unchanged':True,'runtime_high_water':'NOT_MEASURED'}
 manifest={'id':'MPU_SENSOR_V1','memory_budget':budget,'milestone':'DRIVER_SAMPLING_NATIVE_BUILD_ONLY','deployable':False,'board_run':False,'blocking':'typed resource-ready/epoch control bridge absent; entry retained but never auto-called','source_inputs':inputs,'health_source_unchanged':sha(bsp/'applications/amp_echo.c')==sha(BASE/'bsp/rockchip/rk3576-mcu/applications/amp_echo.c'),'patches':{n:sha(ROOT/'patches/mpu6050'/n) for n in patches},'sensor_sources':{n:sha(ROOT/'rtos/sensor'/n) for n in ['mpu6050.h','mpu6050.c','sensor_task.c']},'build_environment':{k:env[k] for k in ['RTT_ROOT','RTT_CC','RTT_EXEC_PATH']},'files':{str(p.relative_to(out)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in [bsp/'.config',bsp/'rtconfig.h',bsp/'gcc_link.ld',bsp/'rtthread.elf',bsp/'rtthread.map',bsp/'rtthread.bin',signed/'amp-signed.itb']}}
 assert all(sha(pathlib.Path(n))==v for n,v in frozen.items())
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(out/'manifest.json')
if __name__=='__main__':main()
