"""Compile the pinned actual U-Boot factory function, not the packager's constants."""
import hashlib,json,pathlib,struct,subprocess,tempfile,unittest,zlib
R=pathlib.Path(__file__).resolve().parents[2]
SHIM=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32; typedef unsigned long ulong;
typedef struct { unsigned char bytes[64]; } image_header_t;
static u32 be(const unsigned char*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static u32 crc(const unsigned char*p,size_t n){u32 x=~0U;while(n--){x^=*p++;for(int j=0;j<8;j++)x=(x>>1)^((x&1)?0xedb88320U:0);}return ~x;}
static int image_check_magic(const image_header_t*h){return be(h->bytes)==0x27051956;}
static int image_check_type(const image_header_t*h,int t){return h->bytes[30]==t;}
static int image_check_arch(const image_header_t*h,int a){return h->bytes[29]==a;}
static int image_check_os(const image_header_t*h,int o){return h->bytes[28]==o;}
static int image_get_comp(const image_header_t*h){return h->bytes[31];}
static u32 image_get_size(const image_header_t*h){return be(h->bytes+12);}
static int image_check_hcrc(const image_header_t*h){unsigned char c[64];memcpy(c,h,64);memset(c+4,0,4);return crc(c,64)==be(h->bytes+4);}
static int image_check_dcrc(const image_header_t*h){return crc((const unsigned char*)(h+1),image_get_size(h))==be(h->bytes+24);}
static u32 uimage_to_cpu(u32 x){return be((const unsigned char*)&x);}
static int called;
static int run_command_list(const char*s,int n,int flags){called++;return n>0&&s[0]=='e'?0:2;}
"""
MAIN=r"""
int main(int argc,char**argv){unsigned char b[65536+64];FILE*f=fopen(argv[1],"rb");if(!f)return 9;size_t n=fread(b,1,sizeof(b),f);fclose(f);if(n<64)return 8;int ret=amp_project_source_legacy_script((image_header_t*)b,argc>2?strtoul(argv[2],0,10):n);printf("ret=%d called=%d\n",ret,called);return ret;}
"""
class FactoryPolicy(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.dir=pathlib.Path(cls.temp.name)
  policy=json.loads((R/'tests/fixtures/audio_probe_factory_policy.json').read_text());snippet=(R/'tests/fixtures/audio_probe_factory_legacy.c').read_text()
  assert hashlib.sha256(snippet.encode()).hexdigest()==policy['fixture_sha256']
  constants='\n'.join('#define '+k+' '+str(v) for k,v in policy['enum_constants_from_actual_image_h'].items())
  (cls.dir/'policy.c').write_text(SHIM+'\n'+constants+'\n'+snippet+MAIN)
  subprocess.run(['gcc','-std=c11','-O2',str(cls.dir/'policy.c'),'-o',str(cls.dir/'policy')],check=True,capture_output=True,timeout=20)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def blob(self,arch=7,os=5,type=6,comp=0,vector=None):
  body=b'echo TEST\n';data=struct.pack('>II',len(body),0) if vector is None else vector;data+=body
  fields=[0x27051956,0,1791244800,len(data),0,0,zlib.crc32(data)&0xffffffff,os,arch,type,comp,b'policy-test']
  header=struct.pack('>7I4B32s',*fields);fields[1]=zlib.crc32(header)&0xffffffff
  return struct.pack('>7I4B32s',*fields)+data
 def run_policy(self,blob,loaded=None):
  p=self.dir/'script';p.write_bytes(blob);args=[str(self.dir/'policy'),str(p)]
  if loaded is not None:args.append(str(loaded))
  return subprocess.run(args,capture_output=True,text=True,timeout=5)
 def test_actual_factory_ppc_accepts_arm_rejects_before_body(self):
  good=self.run_policy(self.blob());self.assertEqual(good.returncode,0);self.assertIn('called=1',good.stdout)
  arm=self.run_policy(self.blob(arch=2));self.assertEqual(arm.returncode,1);self.assertIn('called=0',arm.stdout)
 def test_actual_factory_os_type_comp_fields(self):
  for params in [dict(os=0),dict(type=2),dict(comp=1)]:
   with self.subTest(params=params):
    result=self.run_policy(self.blob(**params));self.assertEqual(result.returncode,1);self.assertIn('called=0',result.stdout)
 def test_actual_factory_crc_length_vector(self):
  b=self.blob();h=bytearray(b);h[32]^=1;d=bytearray(b);d[-1]^=1
  for blob,loaded in [(h,None),(d,None),(b,len(b)-1),(self.blob(vector=struct.pack('>II',10,1)),None),(self.blob(vector=struct.pack('>II',0,0)),None)]:
   result=self.run_policy(blob,loaded);self.assertEqual(result.returncode,1);self.assertIn('called=0',result.stdout)
