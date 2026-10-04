// SPDX-License-Identifier: GPL-2.0-only
/* One-shot, read-only RK3576 SYS_SGRF probe through Rockchip SiP firmware. */
#include <linux/arm-smccc.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/rockchip/rockchip_sip.h>

#define RK3576_SYS_SGRF_CON16 0x26004060UL
#define RK3576_SYS_SGRF_CON17 0x26004064UL

static unsigned long target_addr;
module_param(target_addr, ulong, 0400);
MODULE_PARM_DESC(target_addr, "Only 0x26004060 (CON16) or 0x26004064 (CON17)");

static int __init rk3576_sgrf_smc_probe_init(void)
{
	struct arm_smccc_res res = { 0 };

	if (target_addr != RK3576_SYS_SGRF_CON16 &&
	    target_addr != RK3576_SYS_SGRF_CON17)
		return -EINVAL;

	/* Matches Rockchip sip_smc_secure_reg_read(): no register write. */
	arm_smccc_smc(SIP_ACCESS_REG, 0, target_addr, SECURE_REG_RD,
		      0, 0, 0, 0, &res);
	pr_info("rk3576_sgrf_smc_probe: addr=0x%08lx status=0x%lx value=0x%08lx\n",
		target_addr, res.a0, res.a1 & 0xffffffffUL);

	/* Intentional one-shot failure: no module remains loaded. */
	return -ENODEV;
}

module_init(rk3576_sgrf_smc_probe_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("One-shot read of RK3576 BUS M0 mapping registers via SiP SMC");
