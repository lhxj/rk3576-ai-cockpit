#!/usr/bin/env python3
"""Bounded source/byte/Host checks. A PASS is not a board coherency PASS."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

HAL_SHA = "277de3fd4b0e640654ee73bb3308be2ef01e3aad"
RTOS_SHA = "1d0de06c394f89be35a4b6966e766b56f035c19d"
BL31_SHA = "1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07"
ELF_SHA = "a0942d6635ac30bc0d86953a00114b657d2e4c156ff7fa10c2bed204798933cd"
TRANSPORT_SHA = "7b971ef22788c6fd21e1119e93174aaa05b0aa03090fea43e89690770f909071"
KERNEL_SHA = "521833e2d28decbd6473d5717f1f96cc4108e208"
PREFIX = "bsp/rockchip/common/drivers/rpmsg-lite/lib/"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def checked_repo(root, commit):
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(root), "status", "--porcelain",
                                     "--untracked-files=no"], text=True).strip()
    require(head == commit and not dirty, "repository differs from fixed source: " + str(root))


def verify(args):
    checked_repo(args.hal, HAL_SHA)
    checked_repo(args.rtos, RTOS_SHA)
    files = {}

    def read(root, name, label):
        data = (root / name).read_bytes()
        files[label + "/" + name] = sha(data)
        return data.decode()

    system = read(args.hal, "lib/CMSIS/Device/RK3576/Source/Templates/system_rk3576_mcu.c", "hal")
    cache = read(args.hal, "lib/hal/src/hal_cache.c", "hal")
    config = read(args.rtos, "bsp/rockchip/rk3576-mcu/rtconfig.h", "rtos")
    halconf = read(args.rtos, "bsp/rockchip/rk3576-mcu/hal_conf.h", "rtos")
    platform = read(args.rtos, PREFIX + "rpmsg_lite/porting/platform/RK3576/rpmsg_platform.c", "rtos")
    environment = read(args.rtos, PREFIX + "rpmsg_lite/porting/environment/rpmsg_env_rt-thread.c", "rtos")
    compiler = read(args.rtos, PREFIX + "include/rpmsg_compiler.h", "rtos")
    vqueue = read(args.rtos, PREFIX + "virtio/virtqueue.c", "rtos")
    drv = read(args.rtos, "bsp/rockchip/common/drivers/drv_cache.c", "rtos")
    require("#define RT_USING_CACHE" in config and "HAL_DCACHE_MODULE_ENABLED" in halconf,
            "candidate no longer selects cache")
    require("DCACHE->CACHE_CTRL &= ~DCACHE_CACHE_CTRL_CACHE_BYPASS_MASK" in system,
            "SystemInit cache-enable path changed")
    require("return cpuAddr - 0x20000000 + sram_addr_base" in cache and
            "#define DECODED_ADDR(x) (x + HAL_CACHE_DECODED_ADDR_BASE)" in cache,
            "cache maintenance address view changed")
    require("HAL_CACHE_DECODED_ADDR_BASE          0x47800000" in halconf,
            "fixed maintenance offset changed")
    init = drv[drv.index("int rt_hw_cpu_cache_init(void)"):drv.index("/** @} */", drv.index("int rt_hw_cpu_cache_init(void)"))]
    require("HAL_DCACHE_EnableInt()" in init and "HAL_DCACHE_Enable()" not in init,
            "board cache init responsibility changed")
    require('"dsb" : : : "memory"' in compiler and all(
        re.search(r"void " + name + r"\(void\)\s*\{\s*MEM_BARRIER\(\);\s*\}", environment)
        for name in ("env_mb", "env_rmb", "env_wmb")), "M0 barrier path changed")
    require("env_map_patova((uint32_t)(vq->vq_ring.desc[*avail_idx].addr))" in vqueue,
            "remote descriptor address path changed")
    require("return platform_patova(address)" in environment, "environment address delegate changed")

    metadata = json.loads((args.kernel / "sources.json").read_text())
    for item in metadata:
        require(item["source_commit"] == KERNEL_SHA, "kernel source pin changed")
        require(sha((args.kernel / item["path"]).read_bytes()) == item["sha256"], "downloaded source hash differs")
    ring = read(args.kernel, "include/linux/virtio_ring.h", "kernel")
    dma = read(args.kernel, "kernel/dma/coherent.c", "kernel")
    mapping = read(args.kernel, "kernel/dma/mapping.c", "kernel")
    iomem = read(args.kernel, "kernel/iomem.c", "kernel")
    io = read(args.kernel, "arch/arm64/include/asm/io.h", "kernel")
    barrier = read(args.kernel, "arch/arm64/include/asm/barrier.h", "kernel")
    bus = read(args.kernel, "drivers/rpmsg/virtio_rpmsg_bus.c", "kernel")
    virtio = read(args.kernel, "drivers/virtio/virtio_ring.c", "kernel")
    original = args.transport.read_bytes()
    require(sha(original) == TRANSPORT_SHA, "transport source hash differs")
    transport = original.decode()
    require("RPMSG_VRING_ALIGN, vdev, true, ctx," in transport, "old weak barrier flag changed")
    require("ioremap(rpvdev->vring[index], RPMSG_VRING_SIZE)" in transport,
            "ring mapping changed")
    require("memremap(phys_addr, size, MEMREMAP_WC)" in dma and
            "dma_assign_coherent_memory(dev, rmem->priv)" in dma and
            "dma_alloc_from_dev_coherent(dev, size, dma_handle, &cpu_addr)" in mapping and
            "ioremap_wc(offset, size)" in iomem and "PROT_NORMAL_NC" in io,
            "reserved payload mapping chain changed")
    require("dma_alloc_coherent(vdev->dev.parent," in bus and
            "vring_map_one_sg" in virtio and "(dma_addr_t)sg_phys(sg)" in virtio,
            "payload/descriptor mapping path changed")
    require("_PAGE_IOREMAP PROT_DEVICE_nGnRE" in io and
            "dma_rmb();" in ring and "dma_wmb();" in ring and
            "#define __dma_rmb()\tdmb(oshld)" in barrier and
            "#define __dma_wmb()\tdmb(oshst)" in barrier and
            "#define __smp_wmb()\tdmb(ishst)" in barrier,
            "Linux barrier/mapping types changed")
    require("vq->weak_barriers = weak_barriers" in virtio, "virtqueue flag propagation changed")
    if args.draft:
        draft = args.draft.read_text()
        require(draft == transport.replace("RPMSG_VRING_ALIGN, vdev, true, ctx,",
                                           "RPMSG_VRING_ALIGN, vdev, false, ctx,"),
                "draft contains changes besides the barrier flag")

    binary = args.bl31.read_bytes()
    require(sha(binary) == BL31_SHA, "wrong BL31 payload")
    expected = {0x4005ECDC: 0xD2880400, 0x4005ECE4: 0xF2A4C000,
                0x4005ECEC: 0x52A40001, 0x4005ECF0: 0xB9003801,
                0x4005ECF4: 0xB9003C02, 0x4005ECF8: 0xB9004002}
    for address, word in expected.items():
        require(struct.unpack_from("<I", binary, address - 0x40040000)[0] == word,
                f"BL31 CODE store differs at {address:#x}")
    elf = args.elf.read_bytes()
    require(sha(elf) == ELF_SHA, "wrong existing candidate ELF")
    reset = subprocess.check_output([str(args.objdump), "--disassemble=Reset_Handler", str(args.elf)], text=True)
    sysinit = subprocess.check_output([str(args.objdump), "--disassemble=SystemInit", str(args.elf)], text=True)
    require("<SystemInit>" in reset and "<entry>" in reset and
            reset.index("<SystemInit>") < reset.index("<entry>"), "startup ordering changed")
    require("43810000" in sysinit and "bics" in sysinit and "#64" in sysinit,
            "candidate cache controller/bypass clear differs")

    # Execute only the extracted conversion function with standard Host GCC.
    match = re.search(r"void \*platform_patova\(uint32_t addr\)\s*\{.*?\n\}", platform, re.S)
    require(match is not None, "address function missing")
    with tempfile.TemporaryDirectory(prefix="amp-patova-host-") as temp:
        path = Path(temp)
        (path / "test.c").write_text("#include <stdint.h>\n#define HAL_MCU_CORE\n" + match[0] + r'''
int main(void) {
    if ((uintptr_t)platform_patova(0x47d00000) != 0x27d00000) return 1;
    if ((uintptr_t)platform_patova(0x47d08000) != 0x27d08000) return 2;
    return 0;
}
''')
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(path / "test.c"),
                        "-o", str(path / "test")], check=True, timeout=20)
        subprocess.run([str(path / "test")], check=True, timeout=5)
    # Derived from equality of source PA-offset and inverse TRM mapping.
    offset_match = re.search(r"addr\s*-=\s*(0x[0-9a-fA-F]+)", match[0])
    require(offset_match is not None, "fixed PA offset missing")
    implied_b17 = 0x20000000 + int(offset_match[1], 16)
    m0 = 0x27D00000
    hypothetical_pa = implied_b17 + m0 - 0x20000000
    require(hypothetical_pa - 0x20000000 == m0, "fixed-offset algebra mismatch")
    require((hypothetical_pa - 0x30000000 + 0x20000000) != m0,
            "alternate synthetic base should expose mismatch")
    return {"status": "STATIC_HOST_EVIDENCE_PASS; BOARD_COHERENCY_UNVERIFIED",
            "board_access": False, "selected_final_scheme": None,
            "source_hashes": files, "kernel_source_pin": KERNEL_SHA,
            "kernel_download_sources": metadata,
            "bl31_sha256": BL31_SHA, "candidate_elf_sha256": ELF_SHA,
            "m0_system_init": "cache enabled; bypass cleared before RT-Thread entry",
            "linux_vring_mapping": "Device nGnRE if no-map reserved mapping succeeds",
            "linux_reserved_payload_mapping": "Normal NC/WC if non-reusable pool attaches and allocation succeeds",
            "linux_barrier_original": "weak=true; inner-shareable",
            "linux_barrier_draft": "weak=false; DMA outer-shareable + full mb",
            "source_implied_b17_not_runtime": hex(implied_b17),
            "hypothetical_vring0_pa_not_deployment": hex(hypothetical_pa),
            "bl31_code_range": {"begin": "0x20000000", "end": "caller CODE value"},
            "range_example": {"hypothetical_code": "0x47800000",
                              "m0_view_inside": 0x20000000 <= m0 < 0x47800000,
                              "decoded_pa_inside": 0x20000000 <= hypothetical_pa < 0x47800000,
                              "comparison_address_view": "SOURCE_INFERRED; TRM detail missing"},
            "remaining": ["actual B17 and effective reset remap", "complete uncached coverage/comparator semantics",
                          "pool attachment/no fallback and Linux DMA identity", "current kernel exact source match",
                          "runtime configuration and no aliases"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("hal", "rtos", "kernel", "transport", "bl31", "elf", "objdump"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--draft", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--negative-checks", action="store_true",
                        help="require rejection of a mutated BL31 and a non-minimal draft")
    args = parser.parse_args()
    try:
        result = verify(args)
        if args.negative_checks:
            require(args.draft is not None, "negative draft check requires --draft")
            negative_results = []
            with tempfile.TemporaryDirectory(prefix="amp-cache-negative-") as temp:
                root = Path(temp)
                bad_binary = bytearray(args.bl31.read_bytes())
                bad_binary[0] ^= 1
                (root / "wrong-bl31.bin").write_bytes(bad_binary)
                (root / "wrong-draft.c").write_text(args.draft.read_text() + "\n/* synthetic extra edit */\n")
                for field, name, expected in (
                    ("bl31", "wrong-bl31.bin", "wrong BL31 payload"),
                    ("draft", "wrong-draft.c", "draft contains changes besides the barrier flag"),
                ):
                    changed = argparse.Namespace(**vars(args))
                    setattr(changed, field, root / name)
                    try:
                        verify(changed)
                    except ValueError as error:
                        require(str(error) == expected, "negative check failed for an unexpected reason")
                        negative_results.append({"case": name, "rejected": True, "reason": str(error)})
                    else:
                        raise ValueError("negative evidence unexpectedly accepted: " + name)
            result["negative_checks"] = negative_results
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(result, indent=2) + "\n")
        print(result["status"])
        print("source-implied B17=0x40000000; actual CON17 remains unknown")
    except (ValueError, subprocess.CalledProcessError) as error:
        print("EVIDENCE_CHECK_FAIL:", error)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
