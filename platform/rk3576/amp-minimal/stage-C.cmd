echo P030 RPMsg C v1 with proven tickdiag-v5 - factory default unchanged
if part number mmc 0 boot p029_part; then true; else echo STOP missing boot partition; exit 1; fi
if test "${p029_part}" = "2"; then true; else echo STOP unexpected boot partition; exit 1; fi
if setenv filesize; then true; else echo STOP clear size Image; exit 1; fi
if size mmc 0:${p029_part} /amp-p029/Image; then true; else echo STOP size Image; exit 1; fi
if test "${filesize}" = "0x2930200"; then true; else echo STOP length Image; exit 1; fi
if setenv filesize; then true; else echo STOP clear read Image; exit 1; fi
if load mmc 0:${p029_part} 0x40400000 /amp-p029/Image 2930200; then true; else echo STOP load Image; exit 1; fi
if test "${filesize}" = "0x2930200"; then true; else echo STOP short read Image; exit 1; fi
if setenv filesize; then true; else echo STOP clear size initrd; exit 1; fi
if size mmc 0:${p029_part} /amp-p029/initrd; then true; else echo STOP size initrd; exit 1; fi
if test "${filesize}" = "0x64c22e"; then true; else echo STOP length initrd; exit 1; fi
if setenv filesize; then true; else echo STOP clear read initrd; exit 1; fi
if load mmc 0:${p029_part} 0x4a200000 /amp-p029/initrd 64c22e; then true; else echo STOP load initrd; exit 1; fi
if test "${filesize}" = "0x64c22e"; then true; else echo STOP short read initrd; exit 1; fi
if setenv filesize; then true; else echo STOP clear size stage-C.dtb; exit 1; fi
if size mmc 0:${p029_part} /amp-p029/stage-C.dtb; then true; else echo STOP size stage-C.dtb; exit 1; fi
if test "${filesize}" = "0x4b455"; then true; else echo STOP length stage-C.dtb; exit 1; fi
if setenv filesize; then true; else echo STOP clear read stage-C.dtb; exit 1; fi
if load mmc 0:${p029_part} 0x48300000 /amp-p029/stage-C.dtb 4b455; then true; else echo STOP load stage-C.dtb; exit 1; fi
if test "${filesize}" = "0x4b455"; then true; else echo STOP short read stage-C.dtb; exit 1; fi
if setenv bootargs 'storagemedia=emmc androidboot.storagemedia=emmc androidboot.mode=normal root=/dev/mmcblk0p3 boot_part=2 earlyprintk console=ttyFIQ0 console=tty1 consoleblank=0 loglevel=7 rootwait rw rootfstype=ext4 amp_test_stage=C dyndbg="file virtio_rpmsg_bus.c +p"'; then true; else echo STOP set bootargs; exit 1; fi
if setenv bootargs_ext 'root=/dev/mmcblk0p3'; then true; else echo STOP set root override; exit 1; fi
setenv fdt_high 0xffffffffffffffff
setenv initrd_high 0xffffffffffffffff
if setenv filesize; then true; else echo STOP clear signed FIT size; exit 1; fi
if size mmc 0:${p029_part} /amp-p029/rpmsg-c-v1/amp-signed.itb; then true; else echo STOP signed FIT size; exit 1; fi
if test "${filesize}" = "0x20000"; then true; else echo STOP signed FIT length; exit 1; fi
if amp_m0load /amp-p029/rpmsg-c-v1/amp-signed.itb 0x48300000; then
  echo P030 M0 diagnostic loader returned success - immediately boot prepared Linux
  booti 0x40400000 0x4a200000:64c22e 0x48300000
else
  echo STOP M0 attempt failed - cold power cycle required
fi
echo STOP no same-session boot or retry permitted
exit 1
