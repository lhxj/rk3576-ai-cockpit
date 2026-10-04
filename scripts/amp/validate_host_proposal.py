#!/usr/bin/env python3
"""Check built cold proposal and ordinary DT evidence. No board access."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from generate_host_proposal import generate, layout


def fdt(data):
    header = struct.unpack_from('>10I', data)
    assert header[0] == 0xd00dfeed and header[1] <= len(data)
    offset, strings = header[2], header[3]
    end = offset + header[9]
    stack, nodes = [], {}
    while offset < end:
        token = struct.unpack_from('>I', data, offset)[0]
        offset += 4
        if token == 1:
            zero = data.index(0, offset)
            stack.append(data[offset:zero].decode())
            offset = (zero + 4) & ~3
            nodes['/' + '/'.join(stack[1:])] = {}
        elif token == 2:
            stack.pop()
        elif token == 3:
            length, name_offset = struct.unpack_from('>II', data, offset)
            offset += 8
            name_start = strings + name_offset
            name = data[name_start:data.index(0, name_start)].decode()
            nodes['/' + '/'.join(stack[1:])][name] = data[offset:offset+length]
            offset = (offset + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            return nodes
        else:
            raise ValueError(token)
    raise ValueError('incomplete FDT')


def cells(value):
    assert len(value) % 4 == 0
    return struct.unpack('>' + 'I' * (len(value)//4), value)


def ranges(value):
    c = cells(value)
    assert len(c) % 4 == 0
    return [(c[i]*2**32+c[i+1], c[i+2]*2**32+c[i+3]) for i in range(0,len(c),4) if c[i+2] or c[i+3]]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--contract',type=Path,required=True)
    p.add_argument('--rtos',type=Path,required=True)
    p.add_argument('--m0-build',type=Path,required=True)
    p.add_argument('--dt-fit-build',type=Path,required=True)
    p.add_argument('--linux-driver',type=Path,required=True)
    p.add_argument('--linux-transport',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    c=json.loads(a.contract.read_text()); x=layout(c); results=[]
    def check(condition, label):
        if not condition: raise ValueError(label)
        results.append(label)
    expected=generate(c,0x141)
    for name,text in expected.items():
        check((a.dt_fit_build/name).read_text()==text,'generated '+name)
    bsp=a.rtos/'bsp/rockchip/rk3576-mcu'
    check((bsp/'amp_contract.h').read_text()==expected['amp_contract.h'],'RTOS generated header equality')
    conf=(a.m0_build/'rtconfig.h').read_text()
    for option in ['RT_USING_CACHE','RT_USING_I2C','RT_USING_I2C3','RT_USING_I2C6','RT_USING_I2C7','RT_USING_I2C8','RT_USING_ADC','RT_USING_SARADC','RT_SERIAL_USING_DMA','RT_USING_HWTIMER','RT_USING_PIN','RT_USING_PERI_EN']:
        check(not re.search(r'^#define '+option+r'\b',conf,re.M),'disabled '+option)
    check('warning:' not in (a.m0_build/'build.log').read_text(),'M0 build no warning')
    elf=(a.m0_build/'rtthread.elf').read_bytes()
    eh=struct.unpack_from('<16sHHIIIIIHHHHHH',elf)
    check(eh[0][:5]==b'\x7fELF\x01' and eh[2]==40 and eh[4]==0x141,'ARM ELF entry 0x141')
    loads=[]
    for i in range(eh[10]):
        ph=struct.unpack_from('<8I',elf,eh[5]+i*eh[9])
        if ph[0]==1:
            check(ph[2]+ph[5]<=x['code_size'] and ph[6]!=7,'ELF bounded non-RWX LOAD '+str(i))
            loads.append((ph[2],ph[2]+ph[5]))
    check(len(loads)==2 and loads[0][1]<=loads[1][0],'ELF LOAD segments disjoint')
    vector=struct.unpack_from('<II',(a.m0_build/'rtthread.bin').read_bytes())
    check(vector==(x['code_size'],eh[4]),'BIN initial MSP and reset vector match code reserve and Thumb ELF entry')
    dis=(a.m0_build/'disassembly.txt').read_text().split('<SystemInit>:',1)[1].split('\n\n',1)[0]
    check('43810000' in dis and 'movs\tr1, #64' in dis and 'orrs' in dis and 'dsb' in dis and 'isb' in dis and 'bics' not in dis,'SystemInit compiled bypass plus DSB ISB')
    driver=a.linux_driver.read_text(); echo=(bsp/'applications/amp_echo.c').read_text()
    check(f'"{x["service"]}"' in driver and '.name = AMP_ECHO_SERVICE' in driver,'Linux NS matching name')
    check('AMP_ECHO_SERVICE AMP_SERVICE_NAME' in echo and 'rpmsg_ns_announce' in echo and 'rpmsg_lite_remote_init' in echo,'RTOS remote NS contract')
    transport=a.linux_transport.read_text()
    check('RPMSG_VRING_ALIGN, vdev, false, ctx,' in transport,'Linux strong virtqueue barriers')
    check('if (require_pool)' in transport and 'M0 shared DMA pool attach failed' in transport and '!of_property_read_bool(np, "reusable")' in transport,'Linux required no-map pool failure exits')
    original=fdt((a.dt_fit_build/'running.dtb').read_bytes())
    built=fdt((a.dt_fit_build/'running-amp-host.dtb').read_bytes())
    symbols=original['/__symbols__']
    gpio3_path=symbols['gpio3'].rstrip(b'\0').decode()
    gpio3_handle=cells(original[gpio3_path]['phandle'])[0]
    handles={cells(props['phandle'])[0]: path for path,props in original.items() if 'phandle' in props}
    def enabled(path):
        while path:
            if original.get(path,{}).get('status',b'okay\0') not in (b'okay\0',b'ok\0'): return False
            path=path.rpartition('/')[0]
        return True
    uart_owners=[]
    for path,props in original.items():
        if not enabled(path): continue
        for key,value in props.items():
            if re.fullmatch(r'pinctrl-\d+',key):
                for handle in cells(value):
                    group=handles.get(handle)
                    if group is None: continue
                    for sub,pin_props in original.items():
                        if (sub==group or sub.startswith(group+'/')) and 'rockchip,pins' in pin_props:
                            pins=cells(pin_props['rockchip,pins'])
                            for i in range(0,len(pins),4):
                                if pins[i]==3 and pins[i+1] in (28,29): uart_owners.append((path,key,sub))
            elif key=='gpios' or key.endswith('-gpios'):
                values=cells(value); index=0
                if 'gpio-hog' in props and path.rpartition('/')[0]==gpio3_path:
                    if any(values[i] in (28,29) for i in range(0,len(values),2)): uart_owners.append((path,key,'hog'))
                    continue
                while index<len(values):
                    controller=handles.get(values[index]); n=cells(original.get(controller,{}).get('#gpio-cells',b'\0\0\0\0'))[0]
                    if not n: break
                    if values[index]==gpio3_handle and values[index+1] in (28,29): uart_owners.append((path,key,'gpio'))
                    index+=1+n
    check(not uart_owners,'observed DT has no enabled pinctrl/GPIO owner of GPIO3_D4/D5')
    mbox=[]
    for sym, irq in [('mailbox0',125),('mailbox4',129)]:
        path=symbols[sym].rstrip(b'\0').decode()
        check(cells(original[path]['interrupts'])==(0,irq,4),sym+' GIC IRQ')
        mbox.extend([cells(built[path]['phandle'])[0],0])
        check(built[path]['status']==b'okay\0',sym+' enabled only in Host DT')
    node=built[f'/rpmsg@{x["shared_pa"]:x}']
    check(cells(node['rockchip,link-id'])==(x['link'],),'DT link-id')
    check(cells(node['mboxes'])==tuple(mbox),'DT mailbox RX0 TX4 ch0')
    check(cells(node['reg'])==(0,x['shared_pa'],0,2*x['ring_size']),'DT vring address/size')
    for name,base,size in x['regions']:
        if name=='vring1': continue
        unit={'rtos':'mcu','vring0':'rpmsg','payload':'rpmsg-dma'}[name]
        n=built[f'/reserved-memory/{unit}@{base:x}']
        check(cells(n['reg'])==(0,base,0,2*size if name=='vring0' else size),'DT reservation '+name)
        check('no-map' in n and 'reusable' not in n,'DT uncached '+name)
        if name=='payload': check(cells(node['memory-region'])==cells(n['phandle']),'DT payload phandle')
    memories=[]
    for path, props in original.items():
        if props.get('device_type')==b'memory\0': memories+=ranges(props['reg'])
    for name,base,size in x['regions']:
        check(any(start<=base and base+size<=start+length for start,length in memories),'inside observed RAM '+name)
        for path,props in original.items():
            if path.startswith('/reserved-memory/') and 'reg' in props:
                for start,length in ranges(props['reg']):
                    check(base+size<=start or start+length<=base,'no existing reserve overlap '+name+' '+path)
    allowed={('/mailbox@2ae50000','status'),('/mailbox@2ae54000','status'),
             ('/mailbox@2ae50000','rockchip,txpoll-period-ms'),('/mailbox@2ae54000','rockchip,txpoll-period-ms')}
    for path,props in original.items():
        check(path in built,'original DT node preserved '+path)
        if path=='/__symbols__': continue
        for prop,value in props.items():
            check((path,prop) in allowed or built[path].get(prop)==value,'original DT property preserved '+path+'/'+prop)
    # Execute only our own small Host unit test using the actual project header.
    with tempfile.TemporaryDirectory() as tmp:
        q=Path(tmp); src=q/'mapping.c'
        rpmsg=bsp.parent/'common/drivers/rpmsg-lite/lib/include/platform/RK3576'
        src.write_text('''#include <assert.h>
#include <stdint.h>
#include "rpmsg_config.h"
#include "rpmsg_platform.h"
#include "amp_address.h"
#ifdef SYS_TIMER
#error Echo must not initialize a shared RK timer
#endif
_Static_assert(HAL_CACHE_DECODED_ADDR_BASE == AMP_CODE_LINUX_PA, "code decode contract");
_Static_assert(RL_PLATFORM_SET_LINK_ID(0,4) == AMP_LINK_ID, "M0 link encoding");
_Static_assert(RL_BUFFER_COUNT == '''+str(x['count'])+''', "buffer count");
_Static_assert(RL_BUFFER_PAYLOAD_SIZE == '''+str(c['rpmsg']['buffer_size'])+''', "payload size");
_Static_assert(VRING_SIZE == '''+str(x['ring_size'])+''', "ring extent");
_Static_assert(VRING_ALIGN == '''+str(x['align'])+''', "ring alignment");
int main(void) {
 for (uint32_t i=0; i<AMP_POOL_SIZE; ++i) {
  uint32_t p=AMP_POOL_LINUX_PA+i, m=AMP_POOL_M0+i;
  assert(amp_pool_pa_to_m0(p)==m);
  assert(amp_pool_m0_to_pa(m)==p);
 }
 assert(amp_pool_pa_to_m0(AMP_POOL_LINUX_PA-1)==0);
 assert(amp_pool_pa_to_m0(AMP_POOL_LINUX_PA+AMP_POOL_SIZE)==0);
 assert(amp_pool_pa_to_m0(0xffffffffU)==0);
 assert(amp_pool_m0_to_pa(AMP_POOL_M0-1)==0);
 assert(amp_pool_m0_to_pa(AMP_POOL_M0+AMP_POOL_SIZE)==0);
 if (sizeof(uintptr_t)>4) assert(amp_pool_m0_to_pa((uintptr_t)0x100000000ULL+AMP_POOL_M0)==0);
 return 0;
}
''')
        subprocess.run(['cc','-Wall','-Wextra','-Werror','-I',str(bsp),'-I',str(rpmsg),str(src),'-o',str(q/'mapping')],check=True)
        subprocess.run([str(q/'mapping')],check=True)
    results.append('actual RPMsg macros, timer exclusion, 65536 PA/M0 round trips and invalid/truncation boundaries')
    fit=fdt((a.dt_fit_build/'amp-host.itb').read_bytes())['/images/mcu']
    check(cells(fit['load'])==(x['code_pa'],) and cells(fit['entry'])==(eh[4],),'FIT load/ELF local entry')
    check(cells(fit['rockchip,mcu-shared-window-base'])==(x['window_pa'],),'FIT explicit shared-window setter parameter')
    check(fit['data']==(a.m0_build/'rtthread.bin').read_bytes(),'FIT payload equality')
    check(fit['arch']==b'arm\0' and fit['type']==b'standalone\0','FIT target metadata')
    summary={'status':'HOST_PROPOSAL_PASS_NOT_DEPLOYABLE','checks':len(results),
             'results':results,'contract_sha256':hashlib.sha256(a.contract.read_bytes()).hexdigest(),
             'observed_ram_ranges':memories,'layout':x,
             'remaining_gate':'Actual boot capability/signing/load source/recovery; current CON values not proved'}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(summary,indent=2)+'\n')
    print(summary['status'],len(results),'checks')


if __name__=='__main__':
    main()
