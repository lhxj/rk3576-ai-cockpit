#!/usr/bin/env python3
"""Host-only pair exact inspected Debian gzip/newc initrd with diagnostic modules."""
import argparse,gzip,hashlib,io,json,pathlib,stat,subprocess,tarfile,time,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
AMP=pathlib.Path('/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project')
BACKUP=AMP/'artifacts/local/p025-post-recovery-20261002T173913Z-975579/boot-originals.tar'
NATIVE=ROOT/'artifacts/local/audio-probe-diagnostic-kernel-v2'
OLD='6.1.99-rk3576';NEW='6.1.99-rk3576-audioprobe-d1'
ORIGINAL_SHA='425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397'
COMPRESSED_CAP=32*1024*1024;RAW_CAP=64*1024*1024;MEMBER_CAP=4096

def need(ok,why):
 if not ok:raise ValueError(why)
def digest(raw):return hashlib.sha256(raw).hexdigest()
def check(deadline):need(time.monotonic()<deadline,'package deadline')
def file_sha(p,deadline):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  while True:
   check(deadline);raw=stream.read(1024*1024)
   if not raw:break
   h.update(raw)
 return h.hexdigest()
def safe_name(name):
 need('\0' not in name and (name=='.' or (name and not name.startswith('/') and '..' not in pathlib.PurePosixPath(name).parts and pathlib.PurePosixPath(name).as_posix()==name)),'unsafe cpio path')

def entries(blob,deadline):
 need(len(blob)<=COMPRESSED_CAP and blob[:2]==b'\x1f\x8b','only pinned single gzip/newc format')
 with gzip.GzipFile(fileobj=io.BytesIO(blob)) as stream:raw=stream.read(RAW_CAP+1)
 need(len(raw)<=RAW_CAP,'decompressed cap');offset=0;result=[];seen=set();trailer=False
 while offset<len(raw):
  check(deadline)
  if trailer:need(not any(raw[offset:]),'unexpected cpio/early/extra segment');break
  need(raw[offset:offset+6]==b'070701','newc magic')
  fields=[int(raw[offset+6+8*i:offset+14+8*i],16) for i in range(13)]
  size,ns=fields[6],fields[11];start=offset+110
  need(ns>0 and start+ns<=len(raw) and raw[start+ns-1]==0,'newc name bounds')
  name=raw[start:start+ns-1].decode();start=(start+ns+3)&~3;need(start+size<=len(raw),'newc content bounds')
  content=raw[start:start+size];offset=(start+size+3)&~3
  if name=='TRAILER!!!':need(size==0,'trailer content');trailer=True;continue
  safe_name(name);need(name not in seen,'duplicate cpio member');seen.add(name)
  need(stat.S_IFMT(fields[1]) in (stat.S_IFREG,stat.S_IFDIR,stat.S_IFLNK),'cpio special type')
  result.append((fields,name,content));need(len(result)<=MEMBER_CAP,'member cap')
 need(trailer,'missing trailer');return result

def encode(fields,name,content):
 f=list(fields);n=name.encode()+b'\0';f[6]=len(content);f[11]=len(n);f[12]=0
 raw=b'070701'+b''.join(f'{v:08x}'.encode() for v in f)+n;raw+=bytes(-len(raw)%4);raw+=content;raw+=bytes(-len(raw)%4);return raw

def module_archive_files(archive,original,deadline):
 required=set()
 for fields,name,content in original:
  for prefix in ('usr/lib/modules/','lib/modules/'):
   if name.startswith(prefix+OLD+'/') and stat.S_ISREG(fields[1]):required.add(name[len(prefix+OLD+'/'):])
 found={};seen=set();count=0
 with tarfile.open(archive) as tar:
  for member in tar:
   check(deadline);count+=1;need(count<=4096,'module archive member cap');safe_name(member.name);need(member.name not in seen,'duplicate module archive path');seen.add(member.name)
   prefix='lib/modules/'+NEW+'/'
   need(member.name=='lib/modules/'+NEW or member.name.startswith(prefix),'wrong module archive release')
   need(member.isdir() or member.isfile(),'module archive special/link')
   relative=member.name[len(prefix):] if member.name.startswith(prefix) else ''
   if relative in required:
    need(member.isfile() and member.size<=RAW_CAP,'module replacement type/cap')
    found[relative]=(member.mode,tar.extractfile(member).read(RAW_CAP+1))
 need(set(found)==required,'missing module archive replacement')
 return found

def transform(original,stage,deadline,archive_files):
 output=[];changes=[];replacements=[]
 for fields,name,content in original:
  check(deadline);previous=name;oldcontent=content
  for prefix in ('usr/lib/modules/','lib/modules/'):
   if name==prefix+OLD or name.startswith(prefix+OLD+'/'):
    name=prefix+NEW+name[len(prefix+OLD):]
    if stat.S_ISREG(fields[1]):
     relative=name[len(prefix+NEW+'/'):];p=stage/relative
     need(p.is_file() and not p.is_symlink() and p.resolve().is_relative_to(stage),'missing/newstage file '+relative)
     need(p.stat().st_size<=RAW_CAP,'replacement file cap');mode,exact=archive_files[relative]
     need(stat.S_IMODE(p.stat().st_mode)==mode and p.stat().st_size==len(exact) and file_sha(p,deadline)==digest(exact),'stage differs from pinned modules archive '+relative)
     content=exact # Cpio keeps original permissions/metadata; tar mode only verifies stage identity.
     if name.endswith('.ko'):replacements.append(relative)
    elif stat.S_ISLNK(fields[1]):
     # Only a link member inside module subtree may reference release paths.
     for pathprefix in ('/usr/lib/modules/','/lib/modules/','usr/lib/modules/','lib/modules/'):
      if content.startswith((pathprefix+OLD).encode()):content=content.replace((pathprefix+OLD).encode(),(pathprefix+NEW).encode(),1);break
    break
  if name!=previous or content!=oldcontent:changes.append({'old':previous,'new':name,'sha256':digest(content)})
  adjusted=list(fields);adjusted[6]=len(content);adjusted[11]=len(name.encode())+1;adjusted[12]=0
  output.append((adjusted,name,content))
 need(replacements==['kernel/drivers/net/usb/cdc_eem.ko'],'exact inspected one-module initrd')
 need(len({name for _,name,_ in output})==len(output),'transformed duplicates')
 return output,changes

def verify_preservation(original,paired):
 need(len(original)==len(paired),'entry count changed')
 for (a,n,c),(b,m,d) in zip(original,paired):
  fields=[i for i in range(13) if i not in (6,11,12)]
  need(all(a[i]==b[i] for i in fields),'metadata changed '+n)
  if not (n.endswith('/modules/'+OLD) or '/modules/'+OLD+'/' in n):need(n==m and c==d and a==b,'non-module change '+n)
  for release in [OLD,'6.1.99-rk3576-m0echo-p026']:
   need(not (m.endswith('/modules/'+release) or '/modules/'+release+'/' in m),'old module release path')

def package(out):
 deadline=time.monotonic()+60;out=out.resolve();need(out.is_relative_to(ROOT/'artifacts/local') and not out.exists(),'fresh task-local output required')
 reviewed=json.loads((ROOT/'docs/bringup/mpu6050/AUDIO_PROBE_DIAGNOSTIC_BUILD.json').read_text())
 need(reviewed['release']==NEW and reviewed['status']=='HOST_NATIVE_BUILT_NOT_DEPLOYED','reviewed native identity')
 for relative,info in reviewed['artifacts'].items():
  p=NATIVE/relative;need(p.stat().st_size==info['bytes'] and file_sha(p,deadline)==info['sha256'],'native asset '+relative)
 with tarfile.open(BACKUP) as archive:
  member=archive.getmember('initrd.img-'+OLD);need(member.isfile() and member.size<=COMPRESSED_CAP,'stock tar member cap');old=archive.extractfile(member).read(COMPRESSED_CAP+1)
 need(digest(old)==ORIGINAL_SHA,'exact stock initrd SHA');original=entries(old,deadline)
 need(len(original)==448,'exact inspected stock members')
 stage=NATIVE/'stage/lib/modules'/NEW;archive_files=module_archive_files(NATIVE/'modules.tar',original,deadline);paired,changes=transform(original,stage,deadline,archive_files)
 raw=b''.join(encode(*x) for x in paired)+encode([0,0,0,0,1,0,0,0,0,0,0,0,0],'TRAILER!!!',b'');raw+=bytes(-len(raw)%512)
 need(len(raw)<=RAW_CAP,'encoded cap');check(deadline);blob=gzip.compress(raw,compresslevel=6,mtime=0);need(len(blob)<=COMPRESSED_CAP,'output compressed cap')
 decoded=entries(blob,deadline);verify_preservation(original,decoded)
 need(decoded==paired,'roundtrip exact fields/content')
 ko=stage/'kernel/drivers/net/usb/cdc_eem.ko';info=subprocess.run(['modinfo',str(ko)],capture_output=True,text=True,timeout=5);need(info.returncode==0 and len(info.stdout)+len(info.stderr)<=16384 and NEW+' SMP mod_unload aarch64' in info.stdout,'initrd module vermagic');check(deadline)
 out.mkdir();output=out/('initrd.img-'+NEW);output.write_bytes(blob)
 catalog=[{'name':name,'fields':fields,'bytes':len(content),'sha256':digest(content),'symlink':content.decode() if stat.S_ISLNK(fields[1]) else None} for fields,name,content in decoded]
 (out/'contents.json').write_text(json.dumps(catalog,indent=2)+'\n');(out/'cdc-eem-modinfo.txt').write_text(info.stdout)
 result={'status':'HOST_PAIRED_INITRD_PASS_NOT_DEPLOYED','deployable':False,'board_operations':False,'release':NEW,'source_archive':str(BACKUP),'stock_initrd_sha256':ORIGINAL_SHA,'stock_bytes':len(old),'format':'pinned gzip-only newc; no early uncompressed cpio; no extra cpio segment','members':len(decoded),'module_replacements':1,'non_module_bytes_and_metadata_preserved':True,'changes':changes,'output':{'file':str(output.relative_to(ROOT)),'bytes':len(blob),'sha256':digest(blob)},'contents_sha256':file_sha(out/'contents.json',deadline),'tools':{'python':__import__('sys').version,'gzip':'Python gzip.compress level6 mtime0','zlib':zlib.ZLIB_RUNTIME_VERSION,'module_check_command':['modinfo',str(ko)],'module_check_exit':info.returncode},'root_modules_required':'complete independent /lib/modules/'+NEW+' from actual new modules.tar; not old fallback','deployment_status':'PLAN_ONLY_NOT_AUTHORIZED','elapsed_seconds':60-(deadline-time.monotonic())}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);args=ap.parse_args();print(json.dumps(package(args.output),indent=2))
