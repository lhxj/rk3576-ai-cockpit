#!/usr/bin/env python3
"""Generate Host-only cold M0 inputs. Never changes board or release readiness."""
import argparse
import hashlib
import json
from pathlib import Path


def number(value):
    return int(value, 0) if isinstance(value, str) else value


def layout(contract):
    h, r = contract["host_proposal"], contract["rpmsg"]
    code, code_size = number(h["code_linux_pa"]), number(h["code_size"])
    window, shared = number(h["shared_window_linux_pa"]), number(h["shared_m0_address"])
    ring, align = number(r["vring0"]["size"]), number(r["vring_alignment"])
    count, stride = r["buffer_count"], r["buffer_size"] + h["buffer_header_size"]
    pa = window + shared - 0x20000000
    pool_size = 2 * count * stride
    assert 0x20000000 <= shared < 0x40000000
    assert shared + 2 * ring + pool_size <= 0x40000000, "shared M0 window overflow"
    assert not ((code | window) & 0x3ff), "remap base must be 1 KiB aligned"
    assert ring == number(r["vring1"]["size"]) and align == 0x1000
    assert not (shared & (align - 1)), "vring address alignment"
    assert stride == 512 and count == 64 and r["vring_num"] == 2
    assert shared == number(r["vring0"]["m0_address"])
    assert shared + ring == number(r["vring1"]["m0_address"])
    assert number(r['link_id']) == 4 and r['role'] == {'linux': 'master', 'm0': 'remote'}, "BUS M0 role/link contract"
    m = contract['mailbox']
    assert m['linux_rx'] == {'controller': 0, 'channel': 0, 'irq': 125}, "Linux RX mailbox contract"
    assert m['linux_tx'] == {'controller': 4, 'channel': 0, 'irq': 129}, "Linux TX mailbox contract"
    assert m['m0_rx'] == {'controller': 4, 'channel': 0, 'irq': 175}, "M0 RX mailbox contract"
    assert m['m0_tx']['controller'] == 0 and m['m0_tx']['channel'] == 0, "M0 TX mailbox contract"
    regions = [("rtos", code, code_size), ("vring0", pa, ring),
               ("vring1", pa + ring, ring), ("payload", pa + 2 * ring, pool_size)]
    for index, (name, base, size) in enumerate(regions):
        assert 0 <= base < base + size <= 0x100000000
        for other, other_base, other_size in regions[index + 1:]:
            assert base + size <= other_base or other_base + other_size <= base, f"overlap {name}/{other}"
    return dict(code_pa=code, code_size=code_size, window_pa=window, shared_m0=shared,
                shared_pa=pa, ring_size=ring, align=align, pool_pa=pa + 2 * ring,
                pool_m0=shared + 2 * ring, pool_size=pool_size,
                shared_size=2 * ring + pool_size, stride=stride, count=count,
                link=number(r["link_id"]), service=r["service_name"], regions=regions)


def generate(contract, entry):
    x = layout(contract)
    assert 0 < entry < x["code_size"] and entry & 1, "M0 Thumb entry required"
    assert entry == number(contract['host_proposal']['m0_entry']), "entry differs from contract"
    header = '/* Generated Host proposal. No deployment authorization. */\n#ifndef AMP_CONTRACT_H\n#define AMP_CONTRACT_H\n'
    for name, key in [("AMP_CODE_LINUX_PA", "code_pa"), ("AMP_CODE_SIZE", "code_size"),
                      ("AMP_SHARED_WINDOW_LINUX_PA", "window_pa"), ("AMP_SHARED_M0", "shared_m0"),
                      ("AMP_SHARED_LINUX_PA", "shared_pa"), ("AMP_SHARED_SIZE", "shared_size"),
                      ("AMP_POOL_M0", "pool_m0"), ("AMP_POOL_LINUX_PA", "pool_pa"),
                      ("AMP_POOL_SIZE", "pool_size"), ("AMP_LINK_ID", "link")]:
        header += f'#define {name} 0x{x[key]:08x}\n'
    header += f'#define AMP_SERVICE_NAME "{x["service"]}"\n#define AMP_COLD_START_UNCACHED 1\n#endif\n'
    # This loader uses physical load as CODE mapping, not the FIT entry property.
    its = f'''/dts-v1/;
/ {{
    description = "HOST PROPOSAL - cold BUS M0";
    #address-cells = <1>;
    images {{ mcu {{
        description = "bus_mcu"; data = /incbin/("rttmcu.bin");
        type = "standalone"; arch = "arm"; compression = "none";
        load = <0x{x['code_pa']:x}>; entry = <0x{entry:x}>;
        rockchip,mcu-shared-window-base = <0x{x['window_pa']:x}>;
        hash {{ algo = "sha256"; }};
    }}; }};
    configurations {{ default = "conf";
        conf {{ loadables = "mcu"; }};
    }};
}};
'''
    # Symbols resolved against the original board DTB on Host only. Clock IDs
    # are from fixed 521833e2 rockchip,rk3576-cru.h, not copied EVB ownership.
    dts = f'''/dts-v1/;
/plugin/;
/ {{
    fragment@0 {{ target-path = "/reserved-memory";
        __overlay__ {{ #address-cells = <2>; #size-cells = <2>;
            m0_code: mcu@{x['code_pa']:x} {{ reg = <0 0x{x['code_pa']:x} 0 0x{x['code_size']:x}>; no-map; }};
            m0_rings: rpmsg@{x['shared_pa']:x} {{ reg = <0 0x{x['shared_pa']:x} 0 0x{2*x['ring_size']:x}>; no-map; }};
            m0_pool: rpmsg-dma@{x['pool_pa']:x} {{ compatible = "shared-dma-pool";
                reg = <0 0x{x['pool_pa']:x} 0 0x{x['pool_size']:x}>; no-map; }};
        }};
    }};
    fragment@1 {{ target-path = "/";
        __overlay__ {{ #address-cells = <2>; #size-cells = <2>;
            mcu-amp {{ compatible = "rockchip,mcu-amp";
                clocks = <&cru 207>, <&cru 208>, <&cru 209>, <&cru 139>, <&cru 150>;
                assigned-clocks = <&cru 150>; assigned-clock-rates = <24000000>;
                pinctrl-names = "default"; pinctrl-0 = <&uart5m0_xfer>;
                status = "okay";
            }};
            rpmsg@{x['shared_pa']:x} {{ compatible = "rockchip,rpmsg";
                reg = <0 0x{x['shared_pa']:x} 0 0x{2*x['ring_size']:x}>;
                mbox-names = "rpmsg-rx", "rpmsg-tx";
                mboxes = <&mailbox0 0>, <&mailbox4 0>;
                rockchip,vdev-nums = <1>; rockchip,link-id = <0x{x['link']:x}>;
                project,require-shared-dma-pool;
                memory-region = <&m0_pool>; status = "okay";
            }};
        }};
    }};
    fragment@2 {{ target = <&mailbox0>; __overlay__ {{ rockchip,txpoll-period-ms = <1>; status = "okay"; }}; }};
    fragment@3 {{ target = <&mailbox4>; __overlay__ {{ rockchip,txpoll-period-ms = <1>; status = "okay"; }}; }};
    fragment@4 {{ target = <&uart5>; __overlay__ {{ status = "disabled"; }}; }};
}};
'''
    return {"amp_contract.h": header, "amp.its": its,
            "rk3576-lubancat-3-v2-m0-host.dtso": dts,
            "layout.json": json.dumps({"status": "DRAFT_NOT_DEPLOYABLE", **x,
                                       "m0_entry": entry}, indent=2) + "\n"}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--contract", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--entry", type=number)
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    c = json.loads(a.contract.read_text())
    files = generate(c, a.entry if a.entry is not None else number(c['host_proposal']['m0_entry']))
    if a.check:
        for name, content in files.items():
            assert (a.output / name).read_text() == content, f"mismatch: {name}"
        print("HOST_PROPOSAL_CONTRACT_PASS; release readiness unchanged")
    else:
        a.output.mkdir(parents=True, exist_ok=True)
        for name, content in files.items():
            (a.output / name).write_text(content)
        print(hashlib.sha256(a.contract.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
