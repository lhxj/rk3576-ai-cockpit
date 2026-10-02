#!/usr/bin/env python3
"""Fail closed on RK3576 AMP contract/source mismatch or missing release evidence.

The .yaml contract uses JSON syntax (valid YAML 1.2); only stdlib is required.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def integer(value):
    return int(value, 0) if isinstance(value, str) else value


def match(pattern, body, label):
    result = re.search(pattern, body, re.MULTILINE | re.DOTALL)
    if not result:
        raise ValueError(f"missing {label}")
    return result.group(1)


def dts_region(body, label):
    block = match(rf'\b{re.escape(label)}\s*:\s*[^{{]+\{{([^}}]+)\}}', body, label)
    cells = match(r'\breg\s*=\s*<([^>]+)>', block, f'{label} reg')
    parts = [int(part, 0) for part in cells.split()]
    if len(parts) != 4:
        raise ValueError(f'{label} reg must be two 64-bit cells')
    return (parts[0] << 32) | parts[1], (parts[2] << 32) | parts[3]


def m0_entry_checks(entry, load, code_origin, code_size, explicit_fit_entry):
    """M0 Thumb PC is local; the standalone loader's SMC argument is FIT PA."""
    return {
        'M0 entry is a bounded Thumb local address':
            isinstance(entry, int) and bool(entry & 1) and
            code_origin <= (entry & ~1) < code_origin + code_size,
        'FIT entry equals M0 local entry': explicit_fit_entry == entry,
        'M0 entry is distinct from physical code load': entry != load,
    }


def board_capability_checks(boot):
    """False or a textual claim must not open a hardware capability gate."""
    return {
        'actual U-Boot AMP support': boot.get('uboot_amp_enabled_on_board') is True,
        'actual BL31 M0 SMC support': boot.get('bl31_mcu_smc_verified_on_board') is True,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--contract', type=Path, default=ROOT / 'docs/amp/AMP_PLATFORM_CONTRACT.yaml')
    ap.add_argument('--rtos', type=Path, default=ROOT.parent / 'rtos')
    ap.add_argument('--linux-dts', type=Path)
    args = ap.parse_args()
    c = json.loads(args.contract.read_text())
    bsp = args.rtos / 'bsp/rockchip/rk3576-mcu'
    its = (bsp / 'Image/amp.its').read_text()
    linker = (bsp / 'gcc_link.ld.S').read_text()
    echo = (bsp / 'applications/amp_echo.c').read_text()
    linux_echo = (ROOT / 'scripts/board/amp_echo_linux/rk3576_amp_echo_test.c').read_text()
    hal = args.rtos / 'bsp/rockchip/common/hal/middleware/rpmsg-lite/lib/include/platform/RK3576'
    phead, chead = (hal / 'rpmsg_platform.h').read_text(), (hal / 'rpmsg_config.h').read_text()
    port = (args.rtos / 'bsp/rockchip/common/drivers/rpmsg-lite/lib/rpmsg_lite/porting/platform/RK3576/rpmsg_platform.c').read_text()
    failures = []

    def check(name, ok):
        print(f"{'PASS' if ok else 'FAIL'} {name}")
        if not ok:
            failures.append(name)

    load = integer(match(r'load\s*=\s*<(0x[0-9a-fA-F]+)>', its, 'FIT load'))
    code = integer(match(r'DDR\s*\(rxw\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+)', linker, 'code origin'))
    shared = integer(match(r'#else\s+LINUX_RPMSG\s*\(rxw\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+)', linker, 'non-MOS shared origin'))
    m0_link = int(match(r'AMP_ECHO_LINK_ID\s+RL_PLATFORM_SET_LINK_ID\(0U,\s*(\d+)U\)', echo, 'M0 link'))
    m0_service = match(r'#define AMP_ECHO_SERVICE\s+"([^"]+)"', echo, 'M0 service')
    linux_service = match(r'#define AMP_ECHO_SERVICE\s+"([^"]+)"', linux_echo, 'Linux service')
    check('SoC/M0 identity', c['soc'] == 'rk3576' and c['remote'] == {'core': 'bus_m0', 'architecture': 'cortex-m0'})
    check('candidate FIT load recorded', load == integer(c['rtos']['candidate_fit_load_linux_pa']))
    check('candidate linker origin recorded', code == integer(c['rtos']['candidate_linker_m0_origin']))
    check('M0 shared origin', shared == integer(c['shared_memory']['m0_address']))
    check('M0 link-id', m0_link == integer(c['rpmsg']['link_id']))
    check('RTOS NS and Linux ID service', m0_service == linux_service == c['rpmsg']['service_name'])
    check('Linux rpmsg_device_id entry', bool(re.search(r'\{\s*\.name\s*=\s*AMP_ECHO_SERVICE\s*\}', linux_echo)))
    check('M0 remote init/NS announce', 'rpmsg_lite_remote_init' in echo and 'rpmsg_ns_announce' in echo)
    for macro, expected, source in (
        ('VRING_ALIGN', c['rpmsg']['vring_alignment'], phead),
        ('VRING_SIZE', c['rpmsg']['vring0']['size'], phead),
        ('RL_BUFFER_COUNT', c['rpmsg']['buffer_count'], chead),
        ('RL_BUFFER_PAYLOAD_SIZE', c['rpmsg']['buffer_size'], chead),
    ):
        raw = match(rf'#define\s+{macro}\s+\(?\s*(0x[0-9a-fA-F]+|\d+)', source, macro)
        check(f'{macro} matches contract', integer(raw) == integer(expected))
    check('M0 vring offsets', c['rpmsg']['vring_num'] == 2 and
          integer(c['rpmsg']['vring1']['m0_address']) == integer(c['rpmsg']['vring0']['m0_address']) + integer(c['rpmsg']['vring0']['size']))
    check('M0 link4 mailbox clear client', '"mbox-clr4"' in port)
    check('candidate M0 mailbox map', c['mailbox']['m0_rx']['controller'] == 4 and
          c['mailbox']['m0_rx']['irq'] == 175 and c['mailbox']['m0_tx']['controller'] == 0)

    mandatory = {
        'final RTOS load': c['rtos']['load_address'], 'final RTOS entry': c['rtos']['entry_address'],
        'final code region': c['rtos']['code_region'],
        'final data region': c['rtos']['data_region'],
        'final heap region': c['rtos']['heap_region'],
        'final stack region': c['rtos']['stack_region'],
        'final Linux shared PA': c['shared_memory']['linux_pa'],
        'actual CON16': c['address_translation']['con16_final_value'],
        'actual CON17': c['address_translation']['con17_final_value'],
        'final vring0 PA': c['rpmsg']['vring0']['linux_pa'],
        'final vring1 PA': c['rpmsg']['vring1']['linux_pa'],
        'final buffer PA': c['rpmsg']['buffer_base']['linux_pa'],
        'final buffer M0 address': c['rpmsg']['buffer_base']['m0_address'],
        'final shared-memory attribute': c['shared_memory']['attribute'],
        'coherency scheme': c['coherency']['selected_scheme'],
        'FIT load source': c['boot']['load_source'],
        'actual FIT verification policy': c['boot']['fit_verification_policy_on_board'],
        'exact kernel source': c['linux']['kernel_source_commit'],
        'final LubanCat AMP DTS': c['linux']['board_amp_dts'],
        'offline recovery media': c['recovery']['offline_media'],
        'bootloader backup': c['recovery']['bootloader_backup_sha256'],
    }
    for label, value in mandatory.items():
        check(label, value is not None)
    for label, ok in board_capability_checks(c['boot']).items():
        check(label, ok)
    if c['rtos']['load_address'] is not None:
        check('final FIT load equals source ITS', integer(c['rtos']['load_address']) == load)
    if c['rtos']['entry_address'] is not None:
        explicit = re.search(r'\bentry\s*=\s*<(0x[0-9a-fA-F]+)>', its)
        # rockchip_amp passes `load` as the CODE remap base. M0 starts from
        # its reset vector; neither that SMC argument nor the PA is e_entry.
        for label, ok in m0_entry_checks(
                integer(c['rtos']['entry_address']), load, code,
                integer(c['rtos']['candidate_linker_size']),
                integer(explicit.group(1)) if explicit else None).items():
            check(label, ok)
    if c['address_translation']['con16_final_value'] is not None and c['rtos']['load_address'] is not None:
        check('CON16 code-base remap matches FIT load',
              (integer(c['address_translation']['con16_final_value']) & 0xfffffc00) == integer(c['rtos']['load_address']))
    if c['address_translation']['con17_final_value'] is not None:
        base = integer(c['address_translation']['con17_final_value']) & 0xfffffc00
        for ring in ('vring0', 'vring1'):
            item = c['rpmsg'][ring]
            if item['linux_pa'] is not None:
                check(f'{ring} CON17 PA/M0 mapping', integer(item['m0_address']) - 0x20000000 + base == integer(item['linux_pa']))
        if c['shared_memory']['linux_pa'] is not None:
            check('shared-memory CON17 PA/M0 mapping',
                  integer(c['shared_memory']['m0_address']) - 0x20000000 + base == integer(c['shared_memory']['linux_pa']))
        buffer = c['rpmsg']['buffer_base']
        if buffer['linux_pa'] is not None and buffer['m0_address'] is not None:
            check('buffer CON17 PA/M0 mapping',
                  integer(buffer['m0_address']) - 0x20000000 + base == integer(buffer['linux_pa']))
    if c['rtos']['load_address'] is not None and c['shared_memory']['linux_pa'] is not None:
        a, b = integer(c['rtos']['load_address']), integer(c['shared_memory']['linux_pa'])
        code_size, shared_size = integer(c['rtos']['candidate_linker_size']), integer(c['shared_memory']['size'])
        check('RTOS physical code disjoint from shared DDR', a + code_size <= b or b + shared_size <= a)
    if args.linux_dts:
        dts = args.linux_dts.read_text()
        link = match(r'rockchip,link-id\s*=\s*<(0x[0-9a-fA-F]+)>', dts, 'Linux DTS link')
        mboxes = match(r'mboxes\s*=\s*<([^>]+)>', dts, 'Linux DTS mboxes')
        check('Linux DTS link-id', integer(link) == integer(c['rpmsg']['link_id']))
        check('Linux RX0/TX4 mailbox', '&mailbox0 0' in mboxes and '&mailbox4 0' in mboxes)
        names = match(r'mbox-names\s*=\s*([^;]+);', dts, 'Linux DTS mbox-names')
        check('Linux mbox order RX then TX', re.findall(r'"([^"]+)"', names) == ['rpmsg-rx', 'rpmsg-tx'])
        vring_base, vring_reserve = dts_region(dts, 'rpmsg_reserved')
        dma_base, dma_reserve = dts_region(dts, 'rpmsg_dma_reserved')
        code_base, code_reserve = dts_region(dts, 'mcu_reserved')
        check('DTS code reserve contains FIT physical load',
              c['rtos']['load_address'] is not None and code_base <= load and
              load + integer(c['rtos']['candidate_linker_size']) <= code_base + code_reserve)
        check('DTS code reserve disjoint from vring reserve',
              code_base + code_reserve <= vring_base or vring_base + vring_reserve <= code_base)
        check('DTS code reserve disjoint from DMA reserve',
              code_base + code_reserve <= dma_base or dma_base + dma_reserve <= code_base)
        check('DTS vring reserve disjoint from DMA reserve',
              vring_base + vring_reserve <= dma_base or dma_base + dma_reserve <= vring_base)
        for index, ring in enumerate(('vring0', 'vring1')):
            pa = c['rpmsg'][ring]['linux_pa']
            expected = vring_base + index * integer(c['rpmsg']['vring0']['size'])
            check(f'{ring} PA in Linux DTS', pa is not None and integer(pa) == expected)
        check('both vrings fit Linux reserve', vring_reserve >= 2 * integer(c['rpmsg']['vring0']['size']))
        check('buffer PA equals Linux DMA reserve',
              c['rpmsg']['buffer_base']['linux_pa'] is not None and
              integer(c['rpmsg']['buffer_base']['linux_pa']) == dma_base)
        check('Linux DMA reserve holds two buffer directions',
              dma_reserve >= 2 * c['rpmsg']['buffer_count'] * (c['rpmsg']['buffer_size'] + 16))
        check('DTS RPMsg node reg equals vring reserve base',
              bool(re.search(r'\brpmsg\s*:\s*rpmsg@[^\{]+\{[^}]*?\breg\s*=\s*<0x0\s+' +
                             re.escape(f'0x{vring_base:x}'), dts, re.DOTALL)))
    else:
        check('final LubanCat DTS input supplied', False)
    check('release status', c['status'] == 'READY_FOR_CONTROLLED_BOARD_TEST')
    print(f'RESULT: {"BLOCKED" if failures else "PASS"} ({len(failures)} failures)')
    return 1 if failures else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f'RESULT: BLOCKED ({exc})', file=sys.stderr)
        sys.exit(2)
