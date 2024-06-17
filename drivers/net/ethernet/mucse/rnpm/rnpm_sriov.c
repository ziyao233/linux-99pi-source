#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#ifdef NETIF_F_HW_VLAN_CTAG_TX
#include <linux/if_vlan.h>
#endif

#include "rnpm.h"
#include "rnpm_type.h"
#include "rnpm_sriov.h"

#ifdef CONFIG_PCI_IOV
static int __rnpm_enable_sriov(struct rnpm_adapter *adapter)
{
	struct rnpm_hw *hw = &adapter->hw;
	int num_vf_macvlans, i;
	struct vf_macvlans *mv_list;
	u32 v;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	adapter->flags |= RNPM_FLAG_SRIOV_ENABLED;
	e_info(probe, "SR-IOV enabled with %d VFs\n", adapter->num_vfs);

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);

	/* Enable VMDq flag so device will be set in VM mode */
	adapter->flags |= RNPM_FLAG_VMDQ_ENABLED;
	if (!adapter->ring_feature[RING_F_VMDQ].limit)
		adapter->ring_feature[RING_F_VMDQ].limit = 1;
	adapter->ring_feature[RING_F_VMDQ].offset = adapter->num_vfs;

	num_vf_macvlans = hw->mac.num_rar_entries -
		(RNPM_MAX_PF_MACVLANS + 1 + adapter->num_vfs);

	adapter->mv_list = mv_list = kcalloc(num_vf_macvlans,
			sizeof(struct vf_macvlans),
			GFP_KERNEL);
	if (mv_list) {
		/* Initialize list of VF macvlans */
		INIT_LIST_HEAD(&adapter->vf_mvs.l);
		for (i = 0; i < num_vf_macvlans; i++) {
			mv_list->vf = -1;
			mv_list->free = true;
			mv_list->rar_entry = hw->mac.num_rar_entries -
				(i + adapter->num_vfs + 1);
			list_add(&mv_list->l, &adapter->vf_mvs.l);
			mv_list++;
		}
	}

	/* Initialize default switching mode VEB */
	wr32(hw, RNPM_DMA_CONFIG, rd32(hw, RNPM_DMA_CONFIG) & (~DMA_VEB_BYPASS));
	adapter->flags2 |= RNPM_FLAG2_BRIDGE_MODE_VEB;
	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);

	//ETH_BYPASS
	rd32(hw, RNPM_ETH_BYPASS);

	wr32(hw, RNPM_HOST_FILTER_EN, 1);
	wr32(hw, RNPM_REDIR_EN, 1);
	v = rd32(hw, RNPM_MRQC_IOV_EN);
	v |= RNPM_IOV_ENABLED;
	wr32(hw, RNPM_MRQC_IOV_EN, v);

	wr32(hw, RNPM_ETH_DMAC_FCTRL, rd32(hw, RNPM_ETH_DMAC_FCTRL) |
			RNPM_FCTRL_BROADCASE_BYPASS);
	//wr32(hw, RNPM_ETH_DMAC_MCSTCTRL, v);

	/* If call to enable VFs succeeded then allocate memory
	 * for per VF control structures.
	 */
	adapter->vfinfo =
		kcalloc(adapter->num_vfs,
				sizeof(struct vf_data_storage), GFP_KERNEL);
	if (adapter->vfinfo) {
		/* limit trafffic classes based on VFs enabled */
		/* TODO analyze VF need support pfc or traffic classes */
		/* We do not support RSS w/ SR-IOV */
		adapter->ring_feature[RING_F_RSS].limit = 2;

		/* Disable RSC when in SR-IOV mode */
		adapter->flags2 &= ~(RNPM_FLAG2_RSC_CAPABLE |
				RNPM_FLAG2_RSC_ENABLED);

		/* enable spoof checking for all VFs */
		//for (i = 0; i < adapter->num_vfs; i++)
		//adapter->vfinfo[i].spoofchk_enabled = true;
		return 0;
	}

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	return -ENOMEM;
}

/* Note this function is called when the user wants to enable SR-IOV
 * VFs using the now deprecated module parameter
 */
void rnpm_enable_sriov(struct rnpm_adapter *adapter)
{
	int pre_existing_vfs = 0;

	pre_existing_vfs = pci_num_vf(adapter->pdev);
	if (!pre_existing_vfs && !adapter->num_vfs)
		return;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	if (!pre_existing_vfs)
		dev_warn(&adapter->pdev->dev,
				"Enabling SR-IOV VFs using the module parameter is deprecated - please use the pci sysfs interface.\n");

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	/* If there are pre-existing VFs then we have to force
	 * use of that many - over ride any module parameter value.
	 * This may result from the user unloading the PF driver
	 * while VFs were assigned to guest VMs or because the VFs
	 * have been created via the new PCI SR-IOV sysfs interface.
	 */
	if (pre_existing_vfs) {
		adapter->num_vfs = pre_existing_vfs;
		dev_warn(&adapter->pdev->dev,
				"Virtual Functions already enabled for this device - Please reload all VF drivers to avoid spoofed packet errors\n");
	} else {
		int err;
		/*
		 * The n10 supports up to 64 VFs per physical function
		 * but this implementation limits allocation to 127 so that
		 * basic networking resources are still available to the
		 * physical function.  If the user requests greater than
		 * 64 VFs then it is an error - reset to default of zero.
		 */
		adapter->num_vfs = min_t(unsigned int, adapter->num_vfs, RNPM_MAX_VF_CNT-1);

		err = pci_enable_sriov(adapter->pdev, adapter->num_vfs);
		if (err) {
			e_err(probe, "Failed to enable PCI sriov: %d\n", err);
			adapter->num_vfs = 0;
			return;
		}
		dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	}

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	if (!__rnpm_enable_sriov(adapter))
		return;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	/* If we have gotten to this point then there is no memory available
	 * to manage the VF devices - print message and bail.
	 */
	e_err(probe, "Unable to allocate memory for VF Data Storage - "
			"SRIOV disabled\n");
	rnpm_disable_sriov(adapter);

}

static bool rnpm_vfs_are_assigned(struct rnpm_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct pci_dev *vfdev;
	int dev_id;
	unsigned vendor_id;
	

	if (adapter->pdev->device == RNPM_DEV_ID_N10_PF0) {
		vendor_id = 0x1dab;
		dev_id = RNPM_DEV_ID_N10_PF0_VF;
	} else if (adapter->pdev->device == RNPM_DEV_ID_N10_PF1) {
		vendor_id = 0x1dab;
		dev_id = RNPM_DEV_ID_N10_PF1_VF;
	} else if (adapter->pdev->device == RNPM_DEV_ID_N10_PF0_N) {
		vendor_id = PCI_VENDOR_ID_MUCSE;
		dev_id = RNPM_DEV_ID_N10_PF0_VF_N;
	} else {
		vendor_id = PCI_VENDOR_ID_MUCSE;
		dev_id = RNPM_DEV_ID_N10_PF1_VF_N;
	}

	/* loop through all the VFs to see if we own any that are assigned */
	vfdev = pci_get_device(vendor_id, dev_id, NULL);
	while (vfdev) {
		/* if we don't own it we don't care */
		if (vfdev->is_virtfn && vfdev->physfn == pdev) {
			/* if it is assigned we cannot release it */
			if (vfdev->dev_flags & PCI_DEV_FLAGS_ASSIGNED)
				return true;
		}

		vfdev = pci_get_device(vendor_id, dev_id, vfdev);
	}

	return false;
}

#endif /* #ifdef CONFIG_PCI_IOV */
int rnpm_disable_sriov(struct rnpm_adapter *adapter)
{
	struct rnpm_hw *hw = &adapter->hw;
	u32 v;
	int rss;

	// disable
	v = rd32(hw, RNPM_MRQC_IOV_EN);
	v &= ~(RNPM_IOV_ENABLED);
	wr32(hw, RNPM_MRQC_IOV_EN, v);

	/* set num VFs to 0 to prevent access to vfinfo */
	adapter->num_vfs = 0;

	/* free VF control structures */
	kfree(adapter->vfinfo);
	adapter->vfinfo = NULL;

	/* free macvlan list */
	kfree(adapter->mv_list);
	adapter->mv_list = NULL;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	/* if SR-IOV is already disabled then there is nothing to do */
	if (!(adapter->flags & RNPM_FLAG_SRIOV_ENABLED))
		return 0;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
#ifdef CONFIG_PCI_IOV
	/*
	 * If our VFs are assigned we cannot shut down SR-IOV
	 * without causing issues, so just leave the hardware
	 * available but disabled
	 */
	if (rnpm_vfs_are_assigned(adapter)) {
		e_dev_warn("Unloading driver while VFs are assigned - VFs will not be deallocated\n");
		return -EPERM;
	}
	/* disable iov and allow time for transactions to clear */
	pci_disable_sriov(adapter->pdev);
#endif
	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);

	/* set default pool back to 0 */

	/* Disable VMDq flag so device will be set in VM mode */
	if (adapter->ring_feature[RING_F_VMDQ].limit == 1)
		adapter->flags &= ~RNPM_FLAG_VMDQ_ENABLED;
	adapter->ring_feature[RING_F_VMDQ].offset = 0;

	rss = min_t(int, adapter->max_ring_pair_counts, num_online_cpus());
	adapter->ring_feature[RING_F_RSS].limit = rss;

	/* take a breather then clean up driver data */
	msleep(100);
	adapter->flags &= ~RNPM_FLAG_SRIOV_ENABLED;

	dbg("%s:%d flags:0x%x\n", __func__, __LINE__, adapter->flags);
	return 0;
}


static int rnpm_pci_sriov_enable(struct pci_dev *dev, int num_vfs)
{
#ifdef CONFIG_PCI_IOV
	// todo fix me 
	struct rnpm_adapter *adapter = pci_get_drvdata(dev);
	//struct rnpm_pf_adapter *pf_adapter = pci_get_drvdata(pdev);
	int err = 0;
	int i;
	int pre_existing_vfs = pci_num_vf(dev);
	
	if (adapter->flags & RNPM_FLAG_MUTIPORT_ENABLED) {
		err = -EACCES;
		goto err_out;
	}


	if (pre_existing_vfs && pre_existing_vfs != num_vfs)
		err = rnpm_disable_sriov(adapter);
	else if (pre_existing_vfs && pre_existing_vfs == num_vfs)
		goto out;

	if (err)
		goto err_out;

	/* While the SR-IOV capability structure reports total VFs to be
	 * 64 we limit the actual number that can be allocated to 63 so
	 * that some transmit/receive resources can be reserved to the
	 * PF.  The PCI bus driver already checks for other values out of
	 * range.
	 */
	if (num_vfs > (RNPM_MAX_VF_FUNCTIONS-1)) {
		err = -EPERM;
		goto err_out;
	}

	adapter->num_vfs = num_vfs;

	err = __rnpm_enable_sriov(adapter);
	if (err)
		goto err_out;

	for (i = 0; i < adapter->num_vfs; i++)
		rnpm_vf_configuration(dev, (i | 0x10000000));

	err = pci_enable_sriov(dev, num_vfs);
	if (err) {
		e_dev_warn("Failed to enable PCI sriov: %d\n", err);
		goto err_out;
	}
	dbg("flags:0x%x\n", adapter->flags);
	rnpm_sriov_reinit(adapter);

out:
	return num_vfs;

err_out:
	return err;
#endif
	return 0;
}

static int rnpm_pci_sriov_disable(struct pci_dev *dev)
{
	struct rnpm_adapter *adapter = pci_get_drvdata(dev);
	int err;
	u32 current_flags = adapter->flags;

	err = rnpm_disable_sriov(adapter);

	/* Only reinit if no error and state changed */
	if (!err && current_flags != adapter->flags) {
		/* rnpm_disable_sriov() doesn't clear VMDQ flag */
		adapter->flags &= ~RNPM_FLAG_VMDQ_ENABLED;
#ifdef CONFIG_PCI_IOV
		rnpm_sriov_reinit(adapter);
#endif
	}

	return err;
}


static int rnpm_set_vf_multicasts(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	int entries = (msgbuf[0] & RNPM_VT_MSGINFO_MASK)
		>> RNPM_VT_MSGINFO_SHIFT;
	u16 *hash_list = (u16 *)&msgbuf[1];
	struct vf_data_storage *vfinfo = &adapter->vfinfo[vf];
	struct rnpm_hw *hw = &adapter->hw;
	int i;
	u32 vector_bit;
	u32 vector_reg;
	u32 mta_reg;

	/* only so many hash values supported */
	entries = min(entries, RNPM_MAX_VF_MC_ENTRIES);

	// enable multicast and unicast filter
	mta_reg = rd32(hw, RNPM_ETH_DMAC_MCSTCTRL);
	wr32(hw, RNPM_ETH_DMAC_MCSTCTRL, mta_reg | RNPM_MCSTCTRL_MULTICASE_TBL_EN | RNPM_MCSTCTRL_UNICASE_TBL_EN);

	/*
	 * salt away the number of multi cast addresses assigned
	 * to this VF for later use to restore when the PF multi cast
	 * list changes
	 */
	vfinfo->num_vf_mc_hashes = entries;

	/*
	 * VFs are limited to using the MTA hash table for their multicast
	 * addresses
	 */
	for (i = 0; i < entries; i++) {
		vfinfo->vf_mc_hashes[i] = hash_list[i];
	}
	for (i = 0; i < vfinfo->num_vf_mc_hashes; i++) {
		vector_reg = (vfinfo->vf_mc_hashes[i] >> 5) & 0x7F;
		vector_bit = vfinfo->vf_mc_hashes[i] & 0x1F;
		mta_reg = rd32(hw, RNPM_ETH_MUTICAST_HASH_TABLE(vector_reg));
		mta_reg |= (1 << vector_bit);
		wr32(hw, RNPM_ETH_MUTICAST_HASH_TABLE(vector_reg), mta_reg);
	}

	return 0;
}

static void rnpm_restore_vf_macvlans(struct rnpm_adapter *adapter)
{
	struct rnpm_hw *hw = &adapter->hw;
	struct list_head *pos;
	struct vf_macvlans *entry;

	hw_dbg(hw, "%s Staring..\n", __func__);

	list_for_each(pos, &adapter->vf_mvs.l) {
		entry = list_entry(pos, struct vf_macvlans, l);
		if (!entry->free) {

			hw_dbg(hw, "  vf:%d MACVLAN: RAR[%d] <= %pM\n",
				entry->vf, entry->rar_entry, entry->vf_macvlan);

			hw->mac.ops.set_rar(hw, entry->rar_entry,
					entry->vf_macvlan,
					entry->vf, RNPM_RAH_AV);
		}
	}
	hw_dbg(hw, "%s Done\n", __func__);
}

void rnpm_restore_vf_multicasts(struct rnpm_adapter *adapter)
{
	struct rnpm_hw *hw = &adapter->hw;
	struct vf_data_storage *vfinfo;
	int i, j;
	u32 vector_bit;
	u32 vector_reg;
	u32 mta_reg;

	hw_dbg(hw, "%s num_vf:%d\n", __func__, adapter->num_vfs);

	for (i = 0; i < adapter->num_vfs; i++) {
		vfinfo = &adapter->vfinfo[i];
		for (j = 0; j < vfinfo->num_vf_mc_hashes; j++) {
			hw->addr_ctrl.mta_in_use++;
			vector_reg = (vfinfo->vf_mc_hashes[j] >> 5) & 0x7F;
			vector_bit = vfinfo->vf_mc_hashes[j] & 0x1F;
			mta_reg = rd32(hw, RNPM_ETH_MUTICAST_HASH_TABLE(vector_reg));
			mta_reg |= (1 << vector_bit);
			wr32(hw, RNPM_ETH_MUTICAST_HASH_TABLE(vector_reg), mta_reg);

			hw_dbg(hw, " VF:%2d mc_hash:0x%x, MTA[%2d][%2d]=1\n",
					i, vfinfo->vf_mc_hashes[j], vector_reg, vector_bit);
		}
	}

	/* Restore any VF macvlans */
	rnpm_restore_vf_macvlans(adapter);
}

static int rnpm_set_vf_vlan(struct rnpm_adapter *adapter, int add, int vid,
		u32 vf)
{
	/* VLAN 0 is a special case, don't allow it to be removed */
	if (!vid && !add)
		return 0;

	return adapter->hw.mac.ops.set_vfta(&adapter->hw, vid, vf, (bool)add);
}

static s32 rnpm_set_vf_lpe(struct rnpm_adapter *adapter, u32 *msgbuf, u32 vf)
{
#if 0
	struct rnpm_hw *hw = &adapter->hw;
	int max_frame = msgbuf[1];
	u32 max_frs;

	/*
	 * For n10EB we have to keep all PFs and VFs operating with
	 * the same max_frame value in order to avoid sending an oversize
	 * frame to a VF.  In order to guarantee this is handled correctly
	 * for all cases we have several special exceptions to take into
	 * account before we can enable the VF for receive
	 */
	if (adapter->hw.mac.type == rnpm_mac_n10EB) {
		struct net_device *dev = adapter->netdev;
		int pf_max_frame = dev->mtu + ETH_HLEN;
		u32 reg_offset, vf_shift, vfre;
		s32 err = 0;

		switch (adapter->vfinfo[vf].vf_api) {
		case rnpm_mbox_api_11:
			/*
			 * Version 1.1 supports jumbo frames on VFs if PF has
			 * jumbo frames enabled which means legacy VFs are
			 * disabled
			 */
			if (pf_max_frame > ETH_FRAME_LEN)
				break;
		default:
			/*
			 * If the PF or VF are running w/ jumbo frames enabled
			 * we need to shut down the VF Rx path as we cannot
			 * support jumbo frames on legacy VFs
			 */
			if ((pf_max_frame > ETH_FRAME_LEN) ||
					(max_frame > (ETH_FRAME_LEN + ETH_FCS_LEN)))
				err = -EINVAL;
			break;
		}

		/* determine VF receive enable location */
		vf_shift = vf % 32;
		reg_offset = vf / 32;

		/* enable or disable receive depending on error */
		vfre = RNPM_READ_REG(hw, RNPM_VFRE(reg_offset));
		if (err)
			vfre &= ~(1 << vf_shift);
		else
			vfre |= 1 << vf_shift;
		//RNPM_WRITE_REG(hw, RNPM_VFRE(reg_offset), vfre);

		if (err) {
			e_err(drv, "VF max_frame %d out of range\n", max_frame);
			return err;
		}
	}

	/* MTU < 68 is an error and causes problems on some kernels */
	if (max_frame > RNPM_MAX_JUMBO_FRAME_SIZE) {
		e_err(drv, "VF max_frame %d out of range\n", max_frame);
		return -EINVAL;
	}

	/* pull current max frame size from hardware */
	max_frs = RNPM_READ_REG(hw, RNPM_MAXFRS);
	max_frs &= RNPM_MHADD_MFS_MASK;
	max_frs >>= RNPM_MHADD_MFS_SHIFT;

	if (max_frs < max_frame) {
		max_frs = max_frame << RNPM_MHADD_MFS_SHIFT;
		//RNPM_WRITE_REG(hw, RNPM_MAXFRS, max_frs);
	}

	e_info(hw, "VF requests change max MTU to %d\n", max_frame);
#endif
	return 0;
}

static void __maybe_unused rnpm_set_vmolr(struct rnpm_hw *hw, u32 vf, bool aupe)
{
#if 0
	u32 vmolr = RNPM_READ_REG(hw, RNPM_VMOLR(vf));
	vmolr |= (RNPM_VMOLR_ROMPE |
			RNPM_VMOLR_BAM);
	if (aupe)
		vmolr |= RNPM_VMOLR_AUPE;
	else
		vmolr &= ~RNPM_VMOLR_AUPE;
	RNPM_WRITE_REG(hw, RNPM_VMOLR(vf), vmolr);
#endif
}

static void __maybe_unused rnpm_clear_vmvir(struct rnpm_adapter *adapter,
											u32 vf)
{
	// struct rnpm_hw *hw = &adapter->hw;

	//RNPM_WRITE_REG(hw, RNPM_VMVIR(vf), 0);
}
static inline void rnpm_vf_reset_event(struct rnpm_adapter *adapter, u32 vf)
{
	struct rnpm_hw *hw = &adapter->hw;
	struct vf_data_storage *vfinfo = &adapter->vfinfo[vf];
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	u8 num_tcs = netdev_get_num_tc(adapter->netdev);


	/* add PF assigned VLAN or VLAN 0 */
	//	rnpm_set_vf_vlan(adapter, true, vfinfo->pf_vlan, vf);

	/* reset offloads to defaults */
	//	rnpm_set_vmolr(hw, vf, !vfinfo->pf_vlan);

	/* set outgoing tags for VFs */
	if (!vfinfo->pf_vlan && !vfinfo->pf_qos && !num_tcs) {
		//		rnpm_clear_vmvir(adapter, vf);
	} else {
		if (vfinfo->pf_qos || !num_tcs)
			rnpm_set_vmvir(adapter, vfinfo->pf_vlan,
					vfinfo->pf_qos, vf);
		else
			rnpm_set_vmvir(adapter, vfinfo->pf_vlan,
					adapter->default_up, vf);

		//if (vfinfo->spoofchk_enabled)
		//	hw->mac.ops.set_vlan_anti_spoofing(hw, true, vf);
	}

	/* reset multicast table array for vf */
	adapter->vfinfo[vf].num_vf_mc_hashes = 0;

	/* Flush and reset the mta with the new values */
	rnpm_set_rx_mode(adapter->netdev);

	/* clear this rar_entry */
	hw->mac.ops.clear_rar(hw, rar_entry);

	/* reset VF api back to unknown */
	adapter->vfinfo[vf].vf_api = 0;
}

static int rnpm_set_vf_mac(struct rnpm_adapter *adapter,
		int vf, unsigned char *mac_addr)
{
	struct rnpm_hw *hw = &adapter->hw;
	/* this rar_entry may be cofict with mac vlan with pf */
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	int vf_ring = vf*2;  //FIXME

	memcpy(adapter->vfinfo[vf].vf_mac_addresses, mac_addr, 6);

	hw->mac.ops.set_rar(hw, rar_entry, mac_addr, vf_ring/2, RNPM_RAH_AV);

	return 0;
}

static int rnpm_set_vf_macvlan(struct rnpm_adapter *adapter,
		int vf, int index, unsigned char *mac_addr)
{
	struct rnpm_hw *hw = &adapter->hw;
	struct list_head *pos;
	struct vf_macvlans *entry;
#if 1
	if (index <= 1) {
		list_for_each(pos, &adapter->vf_mvs.l) {
			entry = list_entry(pos, struct vf_macvlans, l);
			if (entry->vf == vf) {
				entry->vf = -1;
				entry->free = true;
				entry->is_macvlan = false;
				hw->mac.ops.clear_rar(hw, entry->rar_entry);
			}
		}
	}

	/*
	 * If index was zero then we were asked to clear the uc list
	 * for the VF.  We're done.
	 */
	if (!index)
		return 0;

	entry = NULL;

	list_for_each(pos, &adapter->vf_mvs.l) {
		entry = list_entry(pos, struct vf_macvlans, l);
		if (entry->free)
			break;
	}

	/*
	 * If we traversed the entire list and didn't find a free entry
	 * then we're out of space on the RAR table.  Also entry may
	 * be NULL because the original memory allocation for the list
	 * failed, which is not fatal but does mean we can't support
	 * VF requests for MACVLAN because we couldn't allocate
	 * memory for the list management required.
	 */
	if (!entry || !entry->free)
		return -ENOSPC;

	entry->free = false;
	entry->is_macvlan = true;
	entry->vf = vf;
	memcpy(entry->vf_macvlan, mac_addr, ETH_ALEN);

	hw->mac.ops.set_rar(hw, entry->rar_entry, mac_addr, vf, RNPM_RAH_AV);
#endif
	return 0;
}


int rnpm_vf_configuration(struct pci_dev *pdev, unsigned int event_mask)
{
	unsigned char vf_mac_addr[6];
	struct rnpm_adapter *adapter = pci_get_drvdata(pdev);
	unsigned int vfn = (event_mask & 0x3f);

	bool enable = ((event_mask & 0x10000000U) != 0);

	if (enable) {
		eth_zero_addr(vf_mac_addr);
		memcpy(vf_mac_addr, adapter->hw.mac.perm_addr, 6);
		vf_mac_addr[5] = vf_mac_addr[5] + (0x80 | vfn)  ;

		memcpy(adapter->vfinfo[vfn].vf_mac_addresses, vf_mac_addr, 6);
	}

	return 0;
}

static int rnpm_vf_reset_msg(struct rnpm_adapter *adapter, u32 vf)
{
	struct rnpm_hw *hw = &adapter->hw;
	unsigned char *vf_mac = adapter->vfinfo[vf].vf_mac_addresses;
	u32 msgbuf[5];
	// u32 reg_offset, vf_shift;
	u8 *addr = (u8 *)(&msgbuf[1]);

	e_info(probe, "VF Reset msg received from vf %d. cmd:0x%x\n",
			vf, msgbuf[0]);

	/* reset the filters for the device */
	rnpm_vf_reset_event(adapter, vf);

	/* set vf mac address */
	if (!is_zero_ether_addr(vf_mac))
		rnpm_set_vf_mac(adapter, vf, vf_mac);

#if 0
	/* enable transmit for vf */

	/* enable receive for vf */
	/*
	 * The n10 cannot support a mix of jumbo and non-jumbo PF/VFs.
	 * For more info take a look at rnpm_set_vf_lpe
	 */
	if (adapter->hw.mac.type == rnpm_mac_n10EB) {
		struct net_device *dev = adapter->netdev;
		int pf_max_frame = dev->mtu + ETH_HLEN;

		if (pf_max_frame > ETH_FRAME_LEN)
			reg &= ~(1 << vf_shift);
	}
	RNPM_WRITE_REG(hw, RNPM_VFRE(reg_offset), reg);
#endif

	/* enable VF mailbox for further messages */
	adapter->vfinfo[vf].clear_to_send = true;

	/* Enable counting of spoofed packets in the SSVPC register */

	/* reply to reset with ack and vf mac address */
	msgbuf[0] = RNPM_VF_RESET;
	if (!is_zero_ether_addr(vf_mac)) {
		msgbuf[0] |= RNPM_VT_MSGTYPE_ACK;
		memcpy(addr, vf_mac, ETH_ALEN);
	} else {
		msgbuf[0] |= RNPM_VT_MSGTYPE_NACK;
		dev_warn(&adapter->pdev->dev,
				"VF %d has no MAC address assigned, you may have to assign one manually\n",
				vf);
	}

	/*
	 * Piggyback the multicast filter type so VF can compute the
	 * correct vectors
	 */
	msgbuf[RNPM_VF_MC_TYPE_WORD] = 0;
	/* setup link status , pause mode, ft padding mode */

	/* link status */
	// to-do
	/* pause mode */
	msgbuf[RNPM_VF_MC_TYPE_WORD] |= (0xff & hw->fc.current_mode) << 16;
	if (adapter->priv_flags & RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH) {
		msgbuf[RNPM_VF_MC_TYPE_WORD] |= (0x01 << 8);
	} else {
		msgbuf[RNPM_VF_MC_TYPE_WORD] |= (0x00 << 8);
	}
	/* mc_type */
	msgbuf[RNPM_VF_MC_TYPE_WORD] |= rd32(hw, RNPM_ETH_DMAC_MCSTCTRL) & 0x3;

	msgbuf[RNPM_VF_DMA_VERSION_WORD] = rd32(hw, RNPM_DMA_VERSION);;
	/* now vf maybe has no irq handler if it is the first reset*/
	rnpm_write_mbx(hw, msgbuf, RNPM_VF_PERMADDR_MSG_LEN, vf);

	return 0;
}

static int rnpm_set_vf_mac_addr(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	u8 *new_mac = ((u8 *)(&msgbuf[1]));

	if (!is_valid_ether_addr(new_mac)) {
		e_warn(drv, "VF %d attempted to set invalid mac\n", vf);
		return -1;
	}

	if (adapter->vfinfo[vf].pf_set_mac &&
			memcmp(adapter->vfinfo[vf].vf_mac_addresses, new_mac,
				ETH_ALEN)) {
		e_warn(drv,
				"VF %d attempted to override administratively set MAC address\n"
				"Reload the VF driver to resume operations\n",
				vf);
		return -1;
	}

	return rnpm_set_vf_mac(adapter, vf, new_mac) < 0;
}

static int rnpm_set_vf_vlan_msg(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	// struct rnpm_hw *hw = &adapter->hw;
	int add = ((msgbuf[0] & RNPM_VT_MSGINFO_MASK) >> RNPM_VT_MSGINFO_SHIFT);
	int vid = (msgbuf[1] & RNPM_VLVF_VLANID_MASK);
	int err;
	//u8 tcs = netdev_get_num_tc(adapter->netdev);

	if (adapter->vfinfo[vf].pf_vlan) {
		e_warn(drv,
				"VF %d attempted to override administratively set VLAN configuration\n"
				"Reload the VF driver to resume operations\n",
				vf);
		return -1;
	}

	if (add)
		adapter->vfinfo[vf].vlan_count++;
	else if (adapter->vfinfo[vf].vlan_count)
		adapter->vfinfo[vf].vlan_count--;

	err = rnpm_set_vf_vlan(adapter, add, vid, vf);

	return err;
}

static int rnpm_set_vf_vlan_strip_msg(struct rnpm_adapter *adapter, u32 *msgbuf, u32 vf)
{
	struct rnpm_hw *hw = &adapter->hw;
	int vlan_strip_on = !!(msgbuf[1] >> 31);
	int queue_cnt = msgbuf[1] & 0xffff;
	int err = 0, i;

	vf_dbg("strip_on:%d queeu_cnt:%d, %d %d\n", vlan_strip_on, queue_cnt, msgbuf[2], msgbuf[3]);

	for (i = 0; i < queue_cnt; i++) {
		if (vlan_strip_on) {
			hw_queue_strip_rx_vlan(hw, msgbuf[2+i], true);
		} else {
			hw_queue_strip_rx_vlan(hw, msgbuf[2+i], false);
		}
	}

	return err;
}

static int rnpm_set_vf_macvlan_msg(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	u8 *new_mac = ((u8 *)(&msgbuf[1]));
	int index = (msgbuf[0] & RNPM_VT_MSGINFO_MASK) >>
		RNPM_VT_MSGINFO_SHIFT;
	int err;

	if (adapter->vfinfo[vf].pf_set_mac && index > 0) {
		e_warn(drv,
				"VF %d requested MACVLAN filter but is administratively denied\n",
				vf);
		return -1;
	}

	/* An non-zero index indicates the VF is setting a filter */
	if (index) {
		if (!is_valid_ether_addr(new_mac)) {
			e_warn(drv, "VF %d attempted to set invalid mac\n", vf);
			return -1;
		}
	}

	err = rnpm_set_vf_macvlan(adapter, vf, index, new_mac);
	if (err == -ENOSPC)
		e_warn(drv,
				"VF %d has requested a MACVLAN filter but there is no space for it\n",
				vf);

	return err < 0;

	return 0;
}

static int rnpm_negotiate_vf_api(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	adapter->vfinfo[vf].vf_api = 0;

	return 0;
}

static int rnpm_get_vf_reg(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	// struct net_device *dev = adapter->netdev;
	u32 reg = msgbuf[1];

	if (reg == 0) {//FIXME check regs
		return -1;
	}

	msgbuf[1] = rd32(&adapter->hw, reg);

	return 0;
}

static int rnpm_get_vf_queues(struct rnpm_adapter *adapter,
		u32 *msgbuf, u32 vf)
{
	struct net_device *dev = adapter->netdev;
	// struct rnpm_ring_feature *vmdq = &adapter->ring_feature[RING_F_VMDQ];
	unsigned int default_tc = 0;
	u8 num_tcs = netdev_get_num_tc(dev);

	/* verify the PF is supporting the correct APIs */

	/* only allow 1 Tx queue for bandwidth limiting */
	msgbuf[RNPM_VF_TX_QUEUES] = 1;
	msgbuf[RNPM_VF_RX_QUEUES] = 1;

	/* if TCs > 1 determine which TC belongs to default user priority */
	if (num_tcs > 1)
		default_tc = netdev_get_prio_tc_map(dev, adapter->default_up);

	/* notify VF of need for VLAN tag stripping, and correct queue */
	if (num_tcs)
		msgbuf[RNPM_VF_TRANS_VLAN] = num_tcs;
	else if (adapter->vfinfo[vf].pf_vlan || adapter->vfinfo[vf].pf_qos)
		msgbuf[RNPM_VF_TRANS_VLAN] = 1;
	else
		msgbuf[RNPM_VF_TRANS_VLAN] = 0;

	/* notify VF of default queue */
	msgbuf[RNPM_VF_DEF_QUEUE] = default_tc;

	return 0;
}

static int __maybe_unused rnpm_rcv_msg_from_vf(struct rnpm_adapter *adapter, u32 vf)
{
	u32 mbx_size = RNPM_VFMAILBOX_SIZE;
	u32 msgbuf[RNPM_VFMAILBOX_SIZE];
	struct rnpm_hw *hw = &adapter->hw;
	s32 retval;

	//vf_dbg("msg from vf:%d \n", vf);
	//dbg("msg from vf:%d \n", vf);

	retval = rnpm_read_mbx(hw, msgbuf, mbx_size, vf);
	if (retval) {
		pr_err("Error receiving message from VF\n");
		return retval;
	}
	vf_dbg("msg[0]=0x%08x\n", msgbuf[0]);

	/* this is a message we already processed, do nothing */
	if (msgbuf[0] & (RNPM_VT_MSGTYPE_ACK | RNPM_VT_MSGTYPE_NACK))
		return retval;

	/* flush the ack before we write any messages back */

	/* this is a vf reset irq */
	if (msgbuf[0] == RNPM_VF_RESET) {
		//dbg("vf %d up \n", vf);
		return rnpm_vf_reset_msg(adapter, vf);
	}

	/*
	 * until the vf completes a virtual function reset it should not be
	 * allowed to start any configuration.
	 */
	if (!adapter->vfinfo[vf].clear_to_send) {
		msgbuf[0] |= RNPM_VT_MSGTYPE_NACK;
		rnpm_write_mbx(hw, msgbuf, 1, vf);
		return retval;
	}

	switch ((msgbuf[0] & 0xFFFF)) {
	case RNPM_VF_SET_MAC_ADDR:
		retval = rnpm_set_vf_mac_addr(adapter, msgbuf, vf);
		break;
	case RNPM_VF_SET_MULTICAST:
		retval = rnpm_set_vf_multicasts(adapter, msgbuf, vf);
		break;
	case RNPM_VF_SET_VLAN:
		retval = rnpm_set_vf_vlan_msg(adapter, msgbuf, vf);
		break;
	case RNPM_VF_SET_VLAN_STRIP:
		retval = rnpm_set_vf_vlan_strip_msg(adapter, msgbuf, vf);
		break;
	case RNPM_VF_SET_LPE:
		retval = rnpm_set_vf_lpe(adapter, msgbuf, vf);
		break;
	case RNPM_VF_SET_MACVLAN:
		retval = rnpm_set_vf_macvlan_msg(adapter, msgbuf, vf);
		break;
	case RNPM_VF_API_NEGOTIATE:
		retval = rnpm_negotiate_vf_api(adapter, msgbuf, vf);
		break;
	case RNPM_VF_GET_QUEUES:
		retval = rnpm_get_vf_queues(adapter, msgbuf, vf);
		break;
	case RNPM_VF_REG_RD:
		retval = rnpm_get_vf_reg(adapter, msgbuf, vf);
		break;
	case RNPM_PF_REMOVE:
		//dbg("vf %d down\n", vf);
		adapter->vfinfo[vf].clear_to_send = false;
		retval = 1;
		break;
	default:
		e_err(drv, "Unhandled Msg %8.8x\n", msgbuf[0]);
		retval = RNPM_ERR_MBX;
		break;
	}

	/* notify the VF of the results of what it sent us */
	if (retval)
		msgbuf[0] |= RNPM_VT_MSGTYPE_NACK;
	else
		msgbuf[0] |= RNPM_VT_MSGTYPE_ACK;

	msgbuf[0] |= RNPM_VT_MSGTYPE_CTS;

	rnpm_write_mbx(hw, msgbuf, mbx_size, vf);

	return retval;
}

static void __maybe_unused rnpm_rcv_ack_from_vf(struct rnpm_adapter *adapter,
												u32 vf)
{
	struct rnpm_hw *hw = &adapter->hw;
	u32 msg = RNPM_VT_MSGTYPE_NACK;

	/* if device isn't clear to send it shouldn't be reading either */
	if (!adapter->vfinfo[vf].clear_to_send)
		rnpm_write_mbx(hw, &msg, 1, vf);
}

void rnpm_msg_task(struct rnpm_pf_adapter *pf_adapter)
{
	// struct rnpm_hw *hw = &pf_adapter->hw;
	// u32 vf;

	// printk("msg irq !!!!\n");
	rnpm_fw_msg_handler(pf_adapter);

	// fixme no support sriov now
//    for (vf = 0; vf < adapter->num_vfs; vf++) {
//        /* process any reset requests */
//		//if (!rnpm_check_for_rst(hw, vf))
//		//	rnpm_vf_reset_event(adapter, vf);
//
//		/* process any messages pending */
//		if (!rnpm_check_for_msg(hw, vf))
//			rnpm_rcv_msg_from_vf(adapter, vf);
//
//		/* process any acks */
//		if (!rnpm_check_for_ack(hw, vf))
//			rnpm_rcv_ack_from_vf(adapter, vf);
//    }
}

/* try to send mailbox to all active vf */
void rnpm_msg_post_status(struct rnpm_adapter *adapter, enum PF_STATUS status)
{
	u32 msgbuf[RNPM_VFMAILBOX_SIZE];
	struct rnpm_hw *hw = &adapter->hw;
	struct rnpm_mbx_info *mbx = &hw->mbx;
	u32 vf;
	dbg("call rnpm_msg_post_status");

	for (vf = 0; vf < adapter->num_vfs; vf++) {
		if (adapter->vfinfo[vf].clear_to_send) {
			dbg("now send msg to vf %d\n", vf);
			switch (status) {
			case PF_FCS_STATUS:
				msgbuf[0] = RNPM_PF_SET_FCS;
				if (adapter->netdev->features & NETIF_F_RXFCS) {
					msgbuf[1] = 1;
				} else {
					msgbuf[1] = 0;
				}
				break;
			case PF_PAUSE_STATUS:
				msgbuf[0] = RNPM_PF_SET_PAUSE;
				msgbuf[1] = hw->fc.requested_mode;
				break;
			case PF_FT_PADDING_STATUS:
				msgbuf[0] = RNPM_PF_SET_FT_PADDING;
				if (adapter->priv_flags &
					RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH) {
					msgbuf[1] = 1;
				} else {
					msgbuf[1] = 0;
				}

				break;
			default:
				break;
			}
			//dbg("msg 0 is %x\n", msgbuf[0]);
			//dbg("msg 1 is %x\n", msgbuf[1]);
			//
			mbx->ops.write(hw, msgbuf, 2, vf);
		}
	}

}

void rnpm_disable_tx_rx(struct rnpm_adapter *adapter)
{
	// struct rnpm_hw *hw = &adapter->hw;

	/* disable transmit and receive for all vfs */
}

void rnpm_ping_all_vfs(struct rnpm_adapter *adapter)
{
	struct rnpm_hw *hw = &adapter->hw;
	u32 ping;
	int i;

	for (i = 0 ; i < adapter->num_vfs; i++) {
		ping = RNPM_PF_CONTROL_PRING_MSG;
		/* only send to active vf */
		//if (adapter->vfinfo[i].clear_to_send) {
		ping |= RNPM_VT_MSGTYPE_CTS;
		rnpm_write_mbx(hw, &ping, 1, i);
		//	}
	}
}

int rnpm_get_vf_ringnum(int vf, int num)
{       
        //fix me if ring alloc reset
        
        return (vf * 2 + num);
}

int rnpm_setup_ring_maxrate(struct rnpm_adapter *adapter,
		int ring, u64 max_rate)
{
	u64 x, y, result;
#define RNPM_SAMPING_1SEC_INTERNAL (180000000)
        /* set hardware samping internal 1S */
        rnpm_wr_reg(adapter->hw.hw_addr + RNPM_DMA_REG_TX_FLOW_CTRL_TM(ring),
                   RNPM_SAMPING_1SEC_INTERNAL / 10);
	
	x = max_rate;
	y = do_div(x, 10);
	result = x;
	result = x * 3;
        rnpm_wr_reg(adapter->hw.hw_addr + RNPM_DMA_REG_TX_FLOW_CTRL_TH(ring),
                   result);
        
        return 0;
}

#ifdef HAVE_NDO_SET_VF_MIN_MAX_TX_RATE
int rnpm_ndo_set_vf_bw(struct net_device *netdev, int vf,
                      int __always_unused min_tx_rate, int max_tx_rate)
#else
int rnpm_ndo_set_vf_bw(struct net_device *netdev, int vf, int max_tx_rate)
#endif /* HAVE_NDO_SET_VF_MIN_MAX_TX_RATE */
{
        struct rnpm_adapter *adapter = netdev_priv(netdev);
        /* limit vf ring rate */
        int ring_max_rate;
        int vf_ring;
        int link_speed;

        if (vf >= RNPM_MAX_VF_CNT - 1)
                return -EINVAL;

        // todo
        link_speed = 10000;
        //link_speed = rnpm_link_mbps(adapter);
        /* rate limit cannot be less than 10Mbs or greater than link speed */
        if (max_tx_rate && ((max_tx_rate <= 10) ||
				(max_tx_rate > link_speed)))
                return -EINVAL;

        ring_max_rate = max_tx_rate / PF_RING_CNT_WHEN_IOV_ENABLED;

        vf_ring = rnpm_get_vf_ringnum(vf, 0);
        rnpm_setup_ring_maxrate(adapter, vf_ring, ring_max_rate);
        vf_ring = rnpm_get_vf_ringnum(vf, 1);
        rnpm_setup_ring_maxrate(adapter, vf_ring, ring_max_rate);
        return 0;
}


int rnpm_ndo_set_vf_mac(struct net_device *netdev, int vf, u8 *mac)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	if (!is_valid_ether_addr(mac) || (vf >= adapter->num_vfs))
		return -EINVAL;
	adapter->vfinfo[vf].pf_set_mac = true;
	dev_info(&adapter->pdev->dev, "setting MAC %pM on VF %d\n", mac, vf);
	dev_info(&adapter->pdev->dev, "Reload the VF driver to make this"
			" change effective.");
	if (test_bit(__RNPM_DOWN, &adapter->state)) {
		dev_warn(&adapter->pdev->dev, "The VF MAC address has been set,"
				" but the PF device is not up.\n");
		dev_warn(&adapter->pdev->dev, "Bring the PF device up before"
				" attempting to use the VF device.\n");
	}
	return rnpm_set_vf_mac(adapter, vf, mac);
}

static int __maybe_unused rnpm_link_mbps(struct rnpm_adapter *adapter)
{
	switch (adapter->link_speed) {
	case RNPM_LINK_SPEED_100_FULL:
		return 100;
	case RNPM_LINK_SPEED_1GB_FULL:
		return 1000;
	case RNPM_LINK_SPEED_10GB_FULL:
		return 10000;
	default:
		return 0;
	}
}

#if 0
static void rnpm_set_vf_rate_limit(struct rnpm_adapter *adapter, int vf)
{
	struct rnpm_ring_feature *vmdq = &adapter->ring_feature[RING_F_VMDQ];
	struct rnpm_hw *hw = &adapter->hw;
	u32 bcnrc_val = 0;
	u16 queue, queues_per_pool;
	u16 tx_rate = adapter->vfinfo[vf].tx_rate;

	if (tx_rate) {
		/* start with base link speed value */
		bcnrc_val = adapter->vf_rate_link_speed;

		/* Calculate the rate factor values to set */
		bcnrc_val <<= RNPM_RTTBCNRC_RF_INT_SHIFT;
		bcnrc_val /= tx_rate;

		/* clear everything but the rate factor */
		bcnrc_val &= RNPM_RTTBCNRC_RF_INT_MASK |
			RNPM_RTTBCNRC_RF_DEC_MASK;

		/* enable the rate scheduler */
		bcnrc_val |= RNPM_RTTBCNRC_RS_ENA;
	}

	/*
	 * Set global transmit compensation time to the MMW_SIZE in RTTBCNRM
	 * register. Typically MMW_SIZE=0x014 if 9728-byte jumbo is supported
	 * and 0x004 otherwise.
	 */
	switch (hw->mac.type) {
	case rnpm_mac_n10EB:
		RNPM_WRITE_REG(hw, RNPM_RTTBCNRM, 0x4);
		break;
	case rnpm_mac_X540:
		RNPM_WRITE_REG(hw, RNPM_RTTBCNRM, 0x14);
		break;
	default:
		break;
	}

	/* determine how many queues per pool based on VMDq mask */
	queues_per_pool = __ALIGN_MASK(1, ~vmdq->mask);

	/* write value for all Tx queues belonging to VF */
	for (queue = 0; queue < queues_per_pool; queue++) {
		unsigned int reg_idx = (vf * queues_per_pool) + queue;

		RNPM_WRITE_REG(hw, RNPM_RTTDQSEL, reg_idx);
		RNPM_WRITE_REG(hw, RNPM_RTTBCNRC, bcnrc_val);
	}
}

void rnpm_check_vf_rate_limit(struct rnpm_adapter *adapter)
{
	int i;

	/* VF Tx rate limit was not set */
	if (!adapter->vf_rate_link_speed)
		return;

	if (rnpm_link_mbps(adapter) != adapter->vf_rate_link_speed) {
		adapter->vf_rate_link_speed = 0;
		dev_info(&adapter->pdev->dev,
				"Link speed has been changed. VF Transmit rate is disabled\n");
	}

	for (i = 0; i < adapter->num_vfs; i++) {
		if (!adapter->vf_rate_link_speed)
			adapter->vfinfo[i].tx_rate = 0;

		rnpm_set_vf_rate_limit(adapter, i);
	}
}

int rnpm_ndo_set_vf_ring_rate(struct net_device *netdev, int vf, int tx_rate)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_adapter *ctrl = adapter->controler;
	int link_speed;

	/* verify VF is active */
	if (vf >= ctrl->num_vfs)
		return -EINVAL;

	/* verify link is up */
	if (!adapter->link_up)
		return -EINVAL;

	/* verify we are linked at 10Gbps */
	link_speed = rnpm_link_mbps(adapter);
	if (link_speed != 10000)
		return -EINVAL;

	/* rate limit cannot be less than 10Mbs or greater than link speed */
	if (tx_rate && ((tx_rate <= 10) || (tx_rate > link_speed)))
		return -EINVAL;

	/* store values */
	adapter->vf_rate_link_speed = link_speed;
	adapter->vfinfo[vf].tx_rate = tx_rate;

	/* update hardware configuration */
	rnpm_set_vf_rate_limit(adapter, vf);
	return 0;
}
#endif


int rnpm_ndo_get_vf_config(struct net_device *netdev, int vf, struct ifla_vf_info *ivi)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	if (vf >= adapter->num_vfs)
		return -EINVAL;
	ivi->vf = vf;
	memcpy(&ivi->mac, adapter->vfinfo[vf].vf_mac_addresses, ETH_ALEN);
	//ivi->tx_rate = adapter->vfinfo[vf].tx_rate;
	ivi->vlan = adapter->vfinfo[vf].pf_vlan;
	ivi->qos = adapter->vfinfo[vf].pf_qos;
#ifdef HAVE_VF_SPOOFCHK_CONFIGURE
	ivi->spoofchk = adapter->vfinfo[vf].spoofchk_enabled;
#endif
	return 0;
}
int rnpm_pci_sriov_configure(struct pci_dev *dev, int num_vfs)
{
	vf_dbg("\n\n !!!! %s:%d num_vfs:%d\n",
			__func__, __LINE__, num_vfs);
	if (num_vfs == 0)
		return rnpm_pci_sriov_disable(dev);
	else
		return rnpm_pci_sriov_enable(dev, num_vfs);
}
