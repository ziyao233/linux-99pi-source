/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2008 - 2022 Xel Technology. */

#ifndef _XLNID_OSDEP2_H_
#define _XLNID_OSDEP2_H_

static inline bool xlnid_removed(void __iomem *addr)
{
	return unlikely(!addr);
}
#define XLNID_REMOVED(a) xlnid_removed(a)

static inline void xlnid_write_reg_direct(struct xlnid_hw *hw, u32 reg, u32 value)
{
	u8 __iomem *reg_addr;

	reg_addr = READ_ONCE(hw->hw_addr);
	if (XLNID_REMOVED(reg_addr))
		return;
#ifdef DBG
	switch (reg) {
	case XLNID_EIMS:
	case XLNID_EIMC:
	case XLNID_EIAM:
	case XLNID_EIAC:
	case XLNID_EICR:
	case XLNID_EICS:
		printk("%s: Reg - 0x%05X, value - 0x%08X\n", __func__,
		       reg, value);
	default:
		break;
	}
#endif /* DBG */

	writel(value, reg_addr + reg);
}

static inline void xlnid_write_reg64(struct xlnid_hw *hw, u32 reg, u64 value)
{
	u8 __iomem *reg_addr;

	reg_addr = READ_ONCE(hw->hw_addr);
	if (XLNID_REMOVED(reg_addr))
		return;
	writeq(value, reg_addr + reg);
}

#endif /* _XLNID_OSDEP2_H_ */
