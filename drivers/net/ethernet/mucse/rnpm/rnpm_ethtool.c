/* ethtool support for N10M */
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/highmem.h>
#include <linux/uaccess.h>

#include "rnpm.h"
#include "rnpm_mbx.h"
#include "rnpm_phy.h"
#include "rnpm_sriov.h"
#include "rnpm_mbx_fw.h"

#ifdef SIOCETHTOOL
#define RNPM_ALL_RAR_ENTRIES 16

// #ifdef ETHTOOL_TEST

enum {
	NETDEV_STATS,
	RNPM_STATS
};

struct rnpm_stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

/* rnpm allocates num_tx_queues and num_rx_queues symmetrically so
 * we set the num_rx_queues to evaluate to num_tx_queues. This is
 * used because we do not have a good way to get the max number of
 * rx queues with CONFIG_RPS disabled.
 */
#ifdef HAVE_TX_MQ
#ifdef HAVE_NETDEV_SELECT_QUEUE
#ifdef NO_REAL_QUEUE_NUM
#define RNPM_NUM_RX_QUEUES netdev->num_tx_queues
#define RNPM_NUM_TX_QUEUES netdev->num_tx_queues
#else
#define RNPM_NUM_RX_QUEUES netdev->real_num_rx_queues
#define RNPM_NUM_TX_QUEUES netdev->real_num_tx_queues

#endif /* NO_REAL_QUEUE_NUM */
#else
#define RNPM_NUM_RX_QUEUES adapter->indices
#define RNPM_NUM_TX_QUEUES adapter->indices
#endif /* HAVE_NETDEV_SELECT_QUEUE */
#else  /* HAVE_TX_MQ */
#define RNPM_NUM_TX_QUEUES 1
#define RNPM_NUM_RX_QUEUES \
	(((struct rnpm_adapter *)netdev_priv(netdev))->num_rx_queues)
#endif /* HAVE_TX_MQ */

#define RNPM_NETDEV_STAT(_net_stat)                                      \
	{                                                                    \
		.stat_string = #_net_stat,                                       \
		.sizeof_stat = sizeof_field(struct net_device_stats, _net_stat), \
		.stat_offset = offsetof(struct net_device_stats, _net_stat)      \
	}
static const struct rnpm_stats rnpm_gstrings_net_stats[] = {
	RNPM_NETDEV_STAT(rx_packets),
	RNPM_NETDEV_STAT(tx_packets),
	RNPM_NETDEV_STAT(rx_bytes),
	RNPM_NETDEV_STAT(tx_bytes),
	RNPM_NETDEV_STAT(rx_errors),
	RNPM_NETDEV_STAT(tx_errors),
	RNPM_NETDEV_STAT(rx_dropped),
	RNPM_NETDEV_STAT(tx_dropped),
	RNPM_NETDEV_STAT(multicast),
	RNPM_NETDEV_STAT(collisions),
	RNPM_NETDEV_STAT(rx_over_errors),
	RNPM_NETDEV_STAT(rx_crc_errors),
	RNPM_NETDEV_STAT(rx_frame_errors),
	RNPM_NETDEV_STAT(rx_fifo_errors),
	RNPM_NETDEV_STAT(rx_missed_errors),
	RNPM_NETDEV_STAT(tx_aborted_errors),
	RNPM_NETDEV_STAT(tx_carrier_errors),
	RNPM_NETDEV_STAT(tx_fifo_errors),
	RNPM_NETDEV_STAT(tx_heartbeat_errors),
};
#define RNPM_GLOBAL_STATS_LEN ARRAY_SIZE(rnpm_gstrings_net_stats)

#define RNPM_HW_STAT(_name, _stat)                               \
	{                                                            \
		.stat_string = _name,                                    \
		.sizeof_stat = sizeof_field(struct rnpm_adapter, _stat), \
		.stat_offset = offsetof(struct rnpm_adapter, _stat)      \
	}
static struct rnpm_stats rnpm_hwstrings_stats[] = {
	RNPM_HW_STAT("dma_to_eth", hw_stats.dma_to_eth),
	RNPM_HW_STAT("dma_to_switch", hw_stats.dma_to_switch),
	// RNPM_HW_STAT("mac_to_mac", hw_stats.mac_to_mac),
	// RNPM_HW_STAT("switch_to_switch", hw_stats.switch_to_switch),
	RNPM_HW_STAT("eth_to_dma", hw_stats.mac_to_dma),
	RNPM_HW_STAT("switch_to_dma", hw_stats.switch_to_dma),
	RNPM_HW_STAT("vlan_add_cnt", hw_stats.vlan_add_cnt),
	RNPM_HW_STAT("vlan_strip_cnt", hw_stats.vlan_strip_cnt),
	//=== drop==
	// RNPM_HW_STAT("invalid_droped_packets", hw_stats.invalid_droped_packets),
	// RNPM_HW_STAT("filter_dropped_packets", hw_stats.filter_dropped_packets),
	// RNPM_HW_STAT("host_l2_match_drop", hw_stats.host_l2_match_drop),
	// RNPM_HW_STAT("redir_input_match_drop", hw_stats.redir_input_match_drop),
	// RNPM_HW_STAT("redir_etype_match_drop", hw_stats.redir_etype_match_drop),
	// RNPM_HW_STAT("redir_tcp_syn_match_drop",
	// hw_stats.redir_tcp_syn_match_drop),
	// RNPM_HW_STAT("redir_tuple5_match_drop",
	// hw_stats.redir_tuple5_match_drop), RNPM_HW_STAT("redir_tcam_match_drop",
	// hw_stats.redir_tcam_match_drop),

	// RNPM_HW_STAT("bmc_dropped_packets", hw_stats.bmc_dropped_packets),
	// RNPM_HW_STAT("switch_dropped_packets", hw_stats.switch_dropped_packets),
	RNPM_HW_STAT("rx_csum_offload_errors", hw_csum_rx_error),
	RNPM_HW_STAT("rx_csum_offload_good", hw_csum_rx_good),
	RNPM_HW_STAT("rx_broadcast_count", hw_stats.mac_rx_broadcast),
	RNPM_HW_STAT("rx_multicast_count", hw_stats.mac_rx_multicast),
};
#define RNPM_HWSTRINGS_STATS_LEN ARRAY_SIZE(rnpm_hwstrings_stats)

struct rnpm_tx_queue_ring_stat {
	u64 hw_head;
	u64 hw_tail;
	u64 sw_to_clean;
	u64 sw_to_next_to_use;
};

struct rnpm_rx_queue_ring_stat {
	u64 hw_head;
	u64 hw_tail;
	u64 sw_to_use;
	u64 sw_to_clean;
};

#define RNPM_QUEUE_STATS_LEN                                      \
	(RNPM_NUM_TX_QUEUES *                                         \
		 (sizeof(struct rnpm_tx_queue_stats) / sizeof(u64) +      \
		  sizeof(struct rnpm_queue_stats) / sizeof(u64) +         \
		  sizeof(struct rnpm_tx_queue_ring_stat) / sizeof(u64)) + \
	 RNPM_NUM_RX_QUEUES *                                         \
		 (sizeof(struct rnpm_rx_queue_stats) / sizeof(u64) +      \
		  sizeof(struct rnpm_queue_stats) / sizeof(u64) +         \
		  sizeof(struct rnpm_rx_queue_ring_stat) / sizeof(u64)))

#define RNPM_STATS_LEN \
	(RNPM_GLOBAL_STATS_LEN + RNPM_HWSTRINGS_STATS_LEN + RNPM_QUEUE_STATS_LEN)
//#define CLOST_SELF_TEST

#ifndef CLOST_SELF_TEST
#ifdef ETHTOOL_TEST
static const char rnpm_gstrings_test[][ETH_GSTRING_LEN] = {
	"Register test  (offline)",
	"Eeprom test    (offline)",
	"Interrupt test (offline)",
	"Loopback test  (offline)",
	"Link test   (on/offline)"};

#define RNPM_TEST_LEN (sizeof(rnpm_gstrings_test) / ETH_GSTRING_LEN)
#else
#define RNPM_TEST_LEN 0
#endif
#else
#define RNPM_TEST_LEN 0
#endif

static int rnpm_get_regs_len(struct net_device *netdev)
{

//#define RNPM_REGS_LEN 1129
#define RNPM_REGS_LEN 1
	return RNPM_REGS_LEN * sizeof(u32);
}

static void
rnpm_get_regs(struct net_device *netdev, struct ethtool_regs *regs, void *p)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	u32 *regs_buff = p;
	int i;

	memset(p, 0, RNPM_REGS_LEN * sizeof(u32));

	for (i = 0; i < RNPM_REGS_LEN; i++)
		regs_buff[i] = rd32(hw, i * sizeof(u32));
}

#ifdef HAVE_ETHTOOL_GET_SSET_COUNT

static const char rnpm_priv_flags_strings[][ETH_GSTRING_LEN] = {
#define RNPM_MAC_LOOPBACK			BIT(0)
#define RNPM_SWITCH_LOOPBACK		BIT(1)
#define RNPM_VEB_ENABLE				BIT(2)
#define RNPM_PCIE_CACHE_ALIGN_PATCH BIT(3)
#define RNPM_PADDING_DEBUG			BIT(4)
#define RNPM_PTP_FEATURE			BIT(5)
#define RNPM_SIMULATE_DOWN			BIT(6)
#define RNPM_TO_RPU					BIT(7)
#define RNPM_LEN_ERR				BIT(8)
	"mac_loopback",
	"switch_loopback",
	"veb_enable",
	"pcie_patch",
	"padding_debug",
	"ptp_performance_debug",
	"simulate_link_down",
	"to_rpu",
	"mask_len_err",
};

#define RNPM_PRIV_FLAGS_STR_LEN ARRAY_SIZE(rnpm_priv_flags_strings)
#endif

#ifdef HAVE_ETHTOOL_PHY_STATISTICS
static const char rnpm_phy_statistics_strings[][ETH_GSTRING_LEN] = {
	"RX crc good (64~1518)",
	"RX crc good (>1518)",
	"RX crc good (<64)",
	"RX crc wrong (64~1518)",
	"RX crc wrong (>1518)",
	"RX crc wrong (<64)",
	"RX SFD missed (nosfd)",
	"TX crc good (64~1518)",
	"TX crc good (>1518)",
	"TX crc good (<64)",
	"TX crc wrong (64~1518)",
	"TX crc wrong (>1518)",
	"TX crc wrong (<64)",
	"TX SFD missed (nosfd)",
};

#define RNPM_PHY_STATISTICS_STR_LEN ARRAY_SIZE(rnpm_phy_statistics_strings)
#endif

static void rnpm_get_drvinfo(struct net_device *netdev,
							 struct ethtool_drvinfo *drvinfo)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	strlcpy(drvinfo->driver, rnpm_driver_name, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, rnpm_driver_version, sizeof(drvinfo->version));
#ifdef N10
	snprintf(drvinfo->fw_version,
			 sizeof(drvinfo->fw_version),
			 "%d.%d.%d.%d 0x%08x",
			 ((char *)&(hw->fw_version))[3],
			 ((char *)&(hw->fw_version))[2],
			 ((char *)&(hw->fw_version))[1],
			 ((char *)&(hw->fw_version))[0],
			 hw->fw_uid);
#else
	snprintf(drvinfo->fw_version,
			 sizeof(drvinfo->fw_version),
			 "dma:0x%x nic:0x%x",
			 rd32(hw, RNPM_DMA_VERSION),
			 rd32(hw, RNPM_TOP_NIC_VERSION));
#endif
	strlcpy(
		drvinfo->bus_info, pci_name(adapter->pdev), sizeof(drvinfo->bus_info));
	drvinfo->n_stats = RNPM_STATS_LEN;
	drvinfo->testinfo_len = RNPM_TEST_LEN;
	drvinfo->regdump_len = rnpm_get_regs_len(netdev);
#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
	drvinfo->n_priv_flags = RNPM_PRIV_FLAGS_STR_LEN;
#endif
}

static int rnpm_set_autoneg_adv_from_hw(struct rnpm_hw *hw,
										struct ethtool_link_ksettings *ks)
{
	u32 value_r0 = 0, value_r4 = 0, value_r9 = 0;

	/* Read autoneg state from phy */
	if (hw->phy_type == PHY_TYPE_SGMII) {
		rnpm_mbx_phy_read(hw, 0x0, &value_r0);
		/* Not support AN, return directly */
		if (!(value_r0 & BIT(12)) || !hw->link)
			return 0;

		rnpm_mbx_phy_read(hw, 0x4, &value_r4);
		rnpm_mbx_phy_read(hw, 0x9, &value_r9);
		if (value_r4 & 0x100)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 100baseT_Full);
		if (value_r4 & 0x80)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 100baseT_Half);
		if (value_r4 & 0x40)
			ethtool_link_ksettings_add_link_mode(ks, advertising, 10baseT_Full);
		if (value_r4 & 0x20)
			ethtool_link_ksettings_add_link_mode(ks, advertising, 10baseT_Half);

		if (value_r9 & 0x200)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Full);
		if (value_r9 & 0x100)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Half);
	}

	return 0;
}

/**
 * rnpm_phy_type_to_ethtool - convert the phy_types to ethtool link modes
 * @adapter: adapter struct with hw->phy_type
 * @ks: ethtool link ksettings struct to fill out
 *
 **/
static void rnpm_phy_type_to_ethtool(struct rnpm_adapter *adapter,
									 struct ethtool_link_ksettings *ks)
{
	struct rnpm_hw *hw = &adapter->hw;
	u32 supported_link = hw->supported_link;
	u8 phy_type = hw->phy_type;

	ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
	ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);

	if (phy_type == PHY_TYPE_NONE) {
		if (supported_link & RNPM_LINK_SPEED_10GB_FULL) {
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseT_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseT_Full);
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseSR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseSR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseLR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseLR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseER_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseER_Full);
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
		}

		if ((supported_link & RNPM_LINK_SPEED_10GB_FULL) ||
			(supported_link & RNPM_LINK_SPEED_1GB_FULL)) {
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseX_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseX_Full);
#else
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Full);
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
		}
	}

	if (phy_type == PHY_TYPE_SGMII) {

		ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
		ethtool_link_ksettings_add_link_mode(ks, supported, 100baseT_Full);
		ethtool_link_ksettings_add_link_mode(ks, supported, 10baseT_Full);
		ethtool_link_ksettings_add_link_mode(ks, supported, 100baseT_Half);
		ethtool_link_ksettings_add_link_mode(ks, supported, 10baseT_Half);

		rnpm_set_autoneg_adv_from_hw(hw, ks);
	}

	if (rnpm_fw_is_old_ethtool(hw) &&
		(supported_link & RNPM_LINK_SPEED_40GB_FULL)) {
		supported_link |= RNPM_SFP_MODE_40G_CR4 | RNPM_SFP_MODE_40G_SR4 |
						  PHY_TYPE_40G_BASE_LR4;
	}

	if (supported_link & RNPM_SFP_MODE_40G_CR4) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 40000baseCR4_Full);
		ethtool_link_ksettings_add_link_mode(
			ks, advertising, 40000baseCR4_Full);
	}
	if (supported_link & RNPM_SFP_MODE_40G_SR4) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 40000baseSR4_Full);
		ethtool_link_ksettings_add_link_mode(
			ks, advertising, 40000baseSR4_Full);
	}
	if (supported_link & RNPM_SFP_MODE_40G_LR4) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 40000baseLR4_Full);
		ethtool_link_ksettings_add_link_mode(
			ks, advertising, 40000baseLR4_Full);
	}

	if (hw->is_backplane) {
		if (phy_type == RNPM_LINK_SPEED_40GB_FULL) {
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 40000baseKR4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 40000baseKR4_Full);
		}
		if (phy_type == PHY_TYPE_10G_BASE_KR) {
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseKR_Full);
			if (supported_link & RNPM_LINK_SPEED_10GB_FULL)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 10000baseKR_Full);
		}
	}

#if 1

	if (supported_link & RNPM_SFP_MODE_1G_LX ||
		supported_link & RNPM_SFP_MODE_1G_SX) {
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
		ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseX_Full);
		if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseX_Full);
#else
		ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
		if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Full);
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
	}

	if (phy_type == PHY_TYPE_1G_BASE_KX) {

		if (hw->is_backplane) {
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 1000baseKX_Full);
			if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 1000baseKX_Full);
		}

		if(supported_link & RNPM_SFP_MODE_1G_T) {
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
			if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 1000baseT_Full);
		}
	}

#else
	if (phy_type == PHY_TYPE_1G_BASE_KX) {
		switch (hw->phy.media_type) {
			case rnpm_media_type_backplane:
				ethtool_link_ksettings_add_link_mode(
					ks, supported, 1000baseKX_Full);
				if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
					ethtool_link_ksettings_add_link_mode(
						ks, advertising, 1000baseKX_Full);
				break;
			case rnpm_media_type_copper:
				ethtool_link_ksettings_add_link_mode(
					ks, supported, 1000baseT_Full);
				if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
					ethtool_link_ksettings_add_link_mode(
						ks, advertising, 1000baseT_Full);
				break;
			case rnpm_media_type_fiber:
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
				ethtool_link_ksettings_add_link_mode(
					ks, supported, 1000baseX_Full);
				if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
					ethtool_link_ksettings_add_link_mode(
						ks, advertising, 1000baseX_Full);
#else
				ethtool_link_ksettings_add_link_mode(
					ks, supported, 1000baseT_Full);
				if (supported_link & RNPM_LINK_SPEED_1GB_FULL)
					ethtool_link_ksettings_add_link_mode(
						ks, advertising, 1000baseT_Full);
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
				break;
			default:
				break;
		}
	}
#endif
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
	/* need to add new 10G PHY types */
	if (phy_type == PHY_TYPE_10G_BASE_SR) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 10000baseSR_Full);
		if (supported_link & RNPM_LINK_SPEED_10GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseSR_Full);
	}
	if (phy_type == PHY_TYPE_10G_BASE_ER) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 10000baseER_Full);
		if (supported_link & RNPM_LINK_SPEED_10GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseER_Full);
	}
	if (phy_type == PHY_TYPE_10G_BASE_LR) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 10000baseLR_Full);
		if (supported_link & RNPM_LINK_SPEED_10GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseLR_Full);
	}
#else
	/* need to keep backward compatibility with older kernels */
	if (phy_type == PHY_TYPE_10G_BASE_SR || phy_type == PHY_TYPE_10G_BASE_ER ||
		phy_type == PHY_TYPE_10G_BASE_LR) {
		ethtool_link_ksettings_add_link_mode(ks, supported, 10000baseT_Full);
		if (supported_link & RNPM_LINK_SPEED_10GB_FULL)
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseT_Full);
	}
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
}

/**
 * rnpm_get_settings_link_up - Get Link settings for when link is up
 * @hw: hw structure
 * @ks: ethtool ksettings to fill in
 * @netdev: network interface device structure
 **/
static void rnpm_get_settings_link_up(struct rnpm_hw *hw,
									  struct ethtool_link_ksettings *ks,
									  struct net_device *netdev)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct ethtool_link_ksettings cap_ksettings;

	/* Initialize supported and advertised settings based on phy settings */
	switch (hw->phy_type) {
		case PHY_TYPE_40G_BASE_CR4:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 40000baseCR4_Full);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 40000baseCR4_Full);
			break;

		case PHY_TYPE_40G_BASE_SR4:
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 40000baseSR4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 40000baseSR4_Full);
			break;
		case PHY_TYPE_40G_BASE_LR4:
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 40000baseLR4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 40000baseLR4_Full);
			break;
		case PHY_TYPE_10G_BASE_SR:
		case PHY_TYPE_10G_BASE_LR:
		case PHY_TYPE_10G_BASE_ER:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseSR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseSR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseLR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseLR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseER_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseER_Full);
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseX_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseX_Full);
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseT_Full);
			if (hw->speed == SPEED_10000)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 10000baseT_Full);
			break;
			/*
			case PHY_TYPE_10G_BASE_T:
				ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
				ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
				ethtool_link_ksettings_add_link_mode(ks, supported,
			10000baseT_Full); ethtool_link_ksettings_add_link_mode(ks,
			supported, 1000baseT_Full); ethtool_link_ksettings_add_link_mode(ks,
			supported, 100baseT_Full); if (hw->speed == SPEED_10000)
					ethtool_link_ksettings_add_link_mode(ks, advertising,
			10000baseT_Full); if (hw->speed == SPEED_1000)
					ethtool_link_ksettings_add_link_mode(ks, advertising,
			1000baseT_Full); if (hw->speed == SPEED_100)
					ethtool_link_ksettings_add_link_mode(ks, advertising,
			100baseT_Full); break;
				*/

		case PHY_TYPE_1G_BASE_KX:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			if (!!hw->is_backplane) {
				ethtool_link_ksettings_add_link_mode(
					ks, supported, 1000baseKX_Full);
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 1000baseKX_Full);
			}
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseX_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseX_Full);
#endif
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Full);
			break;

		case PHY_TYPE_SGMII:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Full);
			ethtool_link_ksettings_add_link_mode(ks, supported, 100baseT_Full);
			ethtool_link_ksettings_add_link_mode(ks, supported, 10baseT_Full);
			ethtool_link_ksettings_add_link_mode(ks, supported, 1000baseT_Half);
			ethtool_link_ksettings_add_link_mode(ks, supported, 100baseT_Half);
			ethtool_link_ksettings_add_link_mode(ks, supported, 10baseT_Half);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 100baseT_Full);
			ethtool_link_ksettings_add_link_mode(ks, advertising, 10baseT_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseT_Half);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 100baseT_Half);
			ethtool_link_ksettings_add_link_mode(ks, advertising, 10baseT_Half);
#if 0
			if (hw->speed == SPEED_1000)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 1000baseT_Full);
			if (hw->speed == SPEED_100)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 100baseT_Full);
			if (hw->speed == SPEED_10)
				ethtool_link_ksettings_add_link_mode(
					ks, advertising, 10baseT_Full);
#endif
			break;

		case PHY_TYPE_40G_BASE_KR4:
		case PHY_TYPE_10G_BASE_KR:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 40000baseKR4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseKR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 1000baseKX_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, supported, 10000baseKX4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 40000baseKR4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseKR_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 10000baseKX4_Full);
			ethtool_link_ksettings_add_link_mode(
				ks, advertising, 1000baseKX_Full);
			break;

		default:
			/* if we got here and link is up something bad is afoot */
			netdev_info(netdev,
						"WARNING: Link is up but PHY type 0x%x is not "
						"recognized, or incorrect cable is in use\n",
						hw->phy_type);
	}

	/* Now that we've worked out everything that could be supported by the
	 * current PHY type, get what is supported by the NVM and intersect
	 * them to get what is truly supported
	 */
	memset(&cap_ksettings, 0, sizeof(struct ethtool_link_ksettings));
	rnpm_phy_type_to_ethtool(adapter, &cap_ksettings);
	ethtool_intersect_link_masks(ks, &cap_ksettings);

	/* Set speed and duplex */
	ks->base.speed = adapter->speed;
	ks->base.duplex = hw->duplex;
}

/**
 * rnpm_get_settings_link_down - Get the Link settings when link is down
 * @hw: hw structure
 * @ks: ethtool ksettings to fill in
 * @netdev: network interface device structure
 *
 * Reports link settings that can be determined when link is down
 **/
static void rnpm_get_settings_link_down(struct rnpm_hw *hw,
										struct ethtool_link_ksettings *ks,
										struct net_device *netdev)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	/* link is down and the driver needs to fall back on
	 * supported phy types to figure out what info to display
	 */
	rnpm_phy_type_to_ethtool(adapter, ks);

	/* With no link speed and duplex are unknown */
	ks->base.speed = SPEED_UNKNOWN;
	ks->base.duplex = hw->duplex;
}

/**
 * rnpm_set_autoneg_state_from_hw - Set the autoneg state from hardware
 * @hw: hw structure
 * @ks: ethtool ksettings to fill in
 *
 * Set the autoneg state from hardware, like PHY
 **/
static int rnpm_set_autoneg_state_from_hw(struct rnpm_hw *hw,
										  struct ethtool_link_ksettings *ks)
{
	int ret;
	struct rnpm_adapter *adapter = hw->back;

	ks->base.autoneg = (adapter->an ? AUTONEG_ENABLE : AUTONEG_DISABLE);

	/* Read autoneg state from phy */
	if (hw->phy_type == PHY_TYPE_SGMII) {
		u32 value_r0 = 0;
		ret = rnpm_mbx_phy_read(hw, 0x0, &value_r0);
		if (ret)
			return -1;

		ks->base.autoneg =
			(value_r0 & BIT(12)) ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	}

	return 0;
}

static bool fiber_unsupport(u32 supported_link, u8 phy_type)
{
	if ((phy_type == PHY_TYPE_10G_BASE_KR) ||
		(phy_type == PHY_TYPE_10G_BASE_SR) ||
		(phy_type == PHY_TYPE_10G_BASE_LR) ||
		(phy_type == PHY_TYPE_10G_BASE_ER)) {
		if (!(supported_link & RNPM_LINK_SPEED_10GB_FULL))
			return true;
	}

	if ((phy_type == PHY_TYPE_40G_BASE_KR4) ||
		(phy_type == PHY_TYPE_40G_BASE_SR4) ||
		(phy_type == PHY_TYPE_40G_BASE_CR4) ||
		(phy_type == PHY_TYPE_40G_BASE_LR4)) {
		if (!(supported_link & RNPM_LINK_SPEED_40GB_FULL))
			return true;
	}

	if (phy_type == PHY_TYPE_1G_BASE_KX) {
		if (!(supported_link & RNPM_LINK_SPEED_1GB_FULL))
			return true;
	}

	return false;
}

/**
 * rnpm_get_link_ksettings - Get Link Speed and Duplex settings
 * @netdev: network interface device structure
 * @ks: ethtool ksettings
 *
 * Reports speed/duplex settings based on media_type
 **/
static int rnpm_get_link_ksettings(struct net_device *netdev,
								   struct ethtool_link_ksettings *ks)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	bool link_up;
	int err;

	ethtool_link_ksettings_zero_link_mode(ks, supported);
	ethtool_link_ksettings_zero_link_mode(ks, advertising);

	/* update hw from firmware */

	if (test_bit(__RNPM_DOWN, &adapter->pf_adapter->state) ||
		test_bit(__RNPM_RESETTING, &adapter->pf_adapter->state))
		return -1;

	err = rnpm_mbx_get_lane_stat(hw);
	if (err || fiber_unsupport(hw->supported_link, hw->phy_type)) {
		return -1;
	}
	/* update hw->phy.media_type by hw->phy_type */
	switch (hw->phy_type) {
		case PHY_TYPE_NONE:
			hw->phy.media_type = rnpm_media_type_unknown;
			break;
		case PHY_TYPE_1G_BASE_KX:
			if (hw->is_backplane)
				hw->phy.media_type = rnpm_media_type_backplane;
			else if (hw->is_sgmii)
				hw->phy.media_type = rnpm_media_type_copper;
			else {
				if ((hw->supported_link & RNPM_LINK_SPEED_1GB_FULL) ||
					(hw->supported_link & RNPM_SFP_MODE_1G_LX))
					hw->phy.media_type = rnpm_media_type_fiber;
				else
					hw->phy.media_type = rnpm_media_type_unknown;
			}
			break;
		case PHY_TYPE_SGMII:
			hw->phy.media_type = rnpm_media_type_copper;
			ks->base.phy_address = adapter->phy_addr;
			break;
		case PHY_TYPE_10G_BASE_KR:
		case PHY_TYPE_25G_BASE_KR:
		case PHY_TYPE_40G_BASE_KR4:
			hw->phy.media_type = rnpm_media_type_backplane;
			break;
		case PHY_TYPE_10G_BASE_SR:
		case PHY_TYPE_40G_BASE_SR4:
		case PHY_TYPE_40G_BASE_CR4:
		case PHY_TYPE_40G_BASE_LR4:
		case PHY_TYPE_10G_BASE_LR:
		case PHY_TYPE_10G_BASE_ER:
			hw->phy.media_type = rnpm_media_type_fiber;
			break;
		default:
			hw->phy.media_type = rnpm_media_type_unknown;
			break;
	}

	if (hw->supported_link & RNPM_SFP_CONNECTOR_DAC) {
		hw->phy.media_type = rnpm_media_type_da;
	}

	if ((hw->supported_link & RNPM_SFP_TO_SGMII) ||
		(hw->supported_link & RNPM_SFP_MODE_1G_T) ||
		(hw->supported_link & RNPM_SFP_MODE_10G_BASE_T)) {
		hw->phy.media_type = rnpm_media_type_copper;
	}

	/* Check Whether there is media on port */
	if (hw->phy.media_type == rnpm_media_type_fiber) {
		/* If adapter->sfp.mod_abs is 0, there is no media on port. */
		if (!adapter->sfp.mod_abs) {
			hw->phy.media_type = rnpm_media_type_unknown;
			hw->phy_type = PHY_TYPE_NONE;
		}
	}

	/* Now set the settings that don't rely on link being up/down */
	/* Set autoneg settings */
	rnpm_set_autoneg_state_from_hw(hw, ks);

	link_up = hw->link;
	if (link_up)
		rnpm_get_settings_link_up(hw, ks, netdev);
	else
		rnpm_get_settings_link_down(hw, ks, netdev);

	/* Set media type settings */
	switch (hw->phy.media_type) {
		case rnpm_media_type_backplane:
			ethtool_link_ksettings_add_link_mode(ks, supported, Backplane);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Backplane);
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			ks->base.port = PORT_NONE;
			break;
		case rnpm_media_type_copper:
			ethtool_link_ksettings_add_link_mode(ks, supported, TP);
			ethtool_link_ksettings_add_link_mode(ks, advertising, TP);
			if (PHY_TYPE_SGMII == hw->phy_type)
				ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			if (AUTONEG_ENABLE == ks->base.autoneg)
				ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			else
				ethtool_link_ksettings_del_link_mode(ks, advertising, Autoneg);
			ks->base.port = PORT_TP;
			break;
		case rnpm_media_type_da:
		case rnpm_media_type_cx4:
			ethtool_link_ksettings_add_link_mode(ks, supported, FIBRE);
			ethtool_link_ksettings_add_link_mode(ks, advertising, FIBRE);
			ks->base.port = PORT_DA;
			break;
		case rnpm_media_type_fiber:
			ethtool_link_ksettings_add_link_mode(ks, supported, FIBRE);
			ethtool_link_ksettings_add_link_mode(ks, advertising, FIBRE);
			ks->base.port = PORT_FIBRE;
			break;
		case rnpm_media_type_unknown:
		default:
			ethtool_link_ksettings_add_link_mode(ks, supported, Autoneg);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Autoneg);
			ks->base.port = PORT_OTHER;
			break;
	}

	/* Set flow control settings */
	ethtool_link_ksettings_add_link_mode(ks, supported, Pause);
	ethtool_link_ksettings_add_link_mode(ks, supported, Asym_Pause);

	switch (hw->fc.requested_mode) {
		case rnpm_fc_full:
			ethtool_link_ksettings_add_link_mode(ks, advertising, Pause);
			break;
		case rnpm_fc_tx_pause:
			ethtool_link_ksettings_add_link_mode(ks, advertising, Asym_Pause);
			break;
		case rnpm_fc_rx_pause:
			ethtool_link_ksettings_add_link_mode(ks, advertising, Pause);
			ethtool_link_ksettings_add_link_mode(ks, advertising, Asym_Pause);
			break;
		default:
			ethtool_link_ksettings_del_link_mode(ks, advertising, Pause);
			ethtool_link_ksettings_del_link_mode(ks, advertising, Asym_Pause);
			break;
	}
	rnpm_logd(LOG_ETHTOOL,
			  "%s %s set link: speed=%d port=%d duplex=%d autoneg=%d "
			  "phy_address=%d\n",
			  __func__,
			  netdev->name,
			  ks->base.speed,
			  ks->base.port,
			  ks->base.duplex,
			  ks->base.autoneg,
			  ks->base.phy_address);
	// ks->base.transceiver = XCVR_INTERNAL;
	// transceiver
	return 0;
}

static int rnpm_wol_exclusion(struct rnpm_adapter *adapter,
							  struct ethtool_wolinfo *wol)
{
	struct rnpm_hw *hw = &adapter->hw;
	int retval = 0;

	/* WOL not supported for all devices */
	if (!rnpm_wol_supported(adapter, hw->device_id, hw->subsystem_device_id)) {
		retval = 1;
		wol->supported = 0;
	}

	return retval;
}

static void rnpm_get_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	wol->supported = WAKE_UCAST | WAKE_MCAST | WAKE_BCAST | WAKE_MAGIC;
	wol->wolopts = 0;

	/* we now can't wol */
	if (rnpm_wol_exclusion(adapter, wol) ||
		!device_can_wakeup(&adapter->pdev->dev))
		return;

	if (adapter->wol & RNPM_WUFC_EX)
		wol->wolopts |= WAKE_UCAST;
	if (adapter->wol & RNPM_WUFC_MC)
		wol->wolopts |= WAKE_MCAST;
	if (adapter->wol & RNPM_WUFC_BC)
		wol->wolopts |= WAKE_BCAST;
	if (adapter->wol & RNPM_WUFC_MAG)
		wol->wolopts |= WAKE_MAGIC;
}

/**
 * rnpm_set_wol - set the WakeOnLAN configuration
 * @netdev: the netdev in question
 * @wol: the ethtool WoL setting data
 **/
static int rnpm_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	if (wol->wolopts & (WAKE_PHY | WAKE_ARP | WAKE_MAGICSECURE))
		return -EOPNOTSUPP;

	if (rnpm_wol_exclusion(adapter, wol))
		return wol->wolopts ? -EOPNOTSUPP : 0;

	adapter->wol = 0;

	if (wol->wolopts & WAKE_UCAST)
		adapter->wol |= RNPM_WUFC_EX;
	if (wol->wolopts & WAKE_MCAST)
		adapter->wol |= RNPM_WUFC_MC;
	if (wol->wolopts & WAKE_BCAST)
		adapter->wol |= RNPM_WUFC_BC;
	if (wol->wolopts & WAKE_MAGIC)
		adapter->wol |= RNPM_WUFC_MAG;

	device_set_wakeup_enable(&adapter->pdev->dev, adapter->wol);

	return 0;
}

/* ethtool register test data */
struct rnpm_reg_test {
	u16 reg;
	u8 array_len;
	u8 test_type;
	u32 mask;
	u32 write;
};

/* In the hardware, registers are laid out either singly, in arrays
 * spaced 0x40 bytes apart, or in contiguous tables.  We assume
 * most tests take place on arrays or single registers (handled
 * as a single-element array) and special-case the tables.
 * Table tests are always pattern tests.
 *
 * We also make provision for some required setup steps by specifying
 * registers to be written without any read-back testing.
 */

#define PATTERN_TEST	1
#define SET_READ_TEST	2
#define WRITE_NO_TEST	3
#define TABLE32_TEST	4
#define TABLE64_TEST_LO 5
#define TABLE64_TEST_HI 6

/* default n10 register test */
static struct rnpm_reg_test reg_test_n10[] = {
	{RNPM_DMA_CONFIG, 1, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF},
	/*
	{ RNPM_FCRTL_n10(0), 1, PATTERN_TEST, 0x8007FFF0, 0x8007FFF0 },
	{ RNPM_FCRTH_n10(0), 1, PATTERN_TEST, 0x8007FFF0, 0x8007FFF0 },
	{ RNPM_PFCTOP, 1, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ RNPM_VLNCTRL, 1, PATTERN_TEST, 0x00000000, 0x00000000 },
	{ RNPM_RDBAL(0), 4, PATTERN_TEST, 0xFFFFFF80, 0xFFFFFF80 },
	{ RNPM_RDBAH(0), 4, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ RNPM_RDLEN(0), 4, PATTERN_TEST, 0x000FFF80, 0x000FFFFF },
	{ RNPM_RXDCTL(0), 4, WRITE_NO_TEST, 0, IXGBE_RXDCTL_ENABLE },
	{ RNPM_RDT(0), 4, PATTERN_TEST, 0x0000FFFF, 0x0000FFFF },
	{ RNPM_RXDCTL(0), 4, WRITE_NO_TEST, 0, 0 },
	{ RNPM_FCRTH(0), 1, PATTERN_TEST, 0x8007FFF0, 0x8007FFF0 },
	{ RNPM_FCTTV(0), 1, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ RNPM_TDBAL(0), 4, PATTERN_TEST, 0xFFFFFF80, 0xFFFFFFFF },
	{ RNPM_TDBAH(0), 4, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ RNPM_TDLEN(0), 4, PATTERN_TEST, 0x000FFF80, 0x000FFF80 },
	{ RNPM_RXCTRL, 1, SET_READ_TEST, 0x00000001, 0x00000001 },
	{ RNPM_RAL(0), 16, TABLE64_TEST_LO, 0xFFFFFFFF, 0xFFFFFFFF },
	{ RNPM_RAL(0), 16, TABLE64_TEST_HI, 0x8001FFFF, 0x800CFFFF },
	{ RNPM_MTA(0), 128, TABLE32_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	*/
	{.reg = 0}};

/* write and read check */
static bool reg_pattern_test(
	struct rnpm_adapter *adapter, u64 *data, int reg, u32 mask, u32 write)
{
	u32 pat, val, before;
	static const u32 test_pattern[] = {
		0x5A5A5A5A, 0xA5A5A5A5, 0x00000000, 0xFFFFFFFF};

	for (pat = 0; pat < ARRAY_SIZE(test_pattern); pat++) {
		before = readl(adapter->hw.hw_addr + reg);
		writel((test_pattern[pat] & write), (adapter->hw.hw_addr + reg));
		val = readl(adapter->hw.hw_addr + reg);
		if (val != (test_pattern[pat] & write & mask)) {
			e_err(drv,
				  "pattern test reg %04X failed: got "
				  "0x%08X expected 0x%08X\n",
				  reg,
				  val,
				  (test_pattern[pat] & write & mask));
			*data = reg;
			writel(before, adapter->hw.hw_addr + reg);
			return 1;
		}
		writel(before, adapter->hw.hw_addr + reg);
	}
	return 0;
}

static bool reg_set_and_check(
	struct rnpm_adapter *adapter, u64 *data, int reg, u32 mask, u32 write)
{
	u32 val, before;

	before = readl(adapter->hw.hw_addr + reg);
	writel((write & mask), (adapter->hw.hw_addr + reg));
	val = readl(adapter->hw.hw_addr + reg);
	if ((write & mask) != (val & mask)) {
		e_err(drv,
			  "set/check reg %04X test failed: got 0x%08X "
			  "expected 0x%08X\n",
			  reg,
			  (val & mask),
			  (write & mask));
		*data = reg;
		writel(before, (adapter->hw.hw_addr + reg));
		return 1;
	}
	writel(before, (adapter->hw.hw_addr + reg));
	return 0;
}

__maybe_unused static bool rnpm_reg_test(struct rnpm_adapter *adapter,
										 u64 *data)
{
	struct rnpm_reg_test *test;
	struct rnpm_hw *hw = &adapter->hw;
	// u32 value, before, after;
	u32 i;

	if (RNPM_REMOVED(hw->hw_addr)) {
		e_err(drv, "Adapter removed - register test blocked\n");
		*data = 1;
		return true;
	}

	test = reg_test_n10;
	/*
	 * Perform the remainder of the register test, looping through
	 * the test table until we either fail or reach the null entry.
	 */
	while (test->reg) {
		for (i = 0; i < test->array_len; i++) {
			bool b = false;

			switch (test->test_type) {
				case PATTERN_TEST:
					b = reg_pattern_test(adapter,
										 data,
										 test->reg + (i * 0x40),
										 test->mask,
										 test->write);
					break;
				case SET_READ_TEST:
					b = reg_set_and_check(adapter,
										  data,
										  test->reg + (i * 0x40),
										  test->mask,
										  test->write);
					break;
				case WRITE_NO_TEST:
					wr32(hw, test->reg + (i * 0x40), test->write);
					break;
				case TABLE32_TEST:
					b = reg_pattern_test(adapter,
										 data,
										 test->reg + (i * 4),
										 test->mask,
										 test->write);
					break;
				case TABLE64_TEST_LO:
					b = reg_pattern_test(adapter,
										 data,
										 test->reg + (i * 8),
										 test->mask,
										 test->write);
					break;
				case TABLE64_TEST_HI:
					b = reg_pattern_test(adapter,
										 data,
										 (test->reg + 4) + (i * 8),
										 test->mask,
										 test->write);
					break;
			}
			if (b)
				return true;
		}
		test++;
	}

	*data = 0;
	return false;
}

#ifndef CLOST_SELF_TEST
static u64 rnpm_link_test(struct rnpm_adapter *adapter, u64 *data)
{
	struct rnpm_hw *hw = &adapter->hw;
	bool link_up = false;
	u32 link_speed = 0;
	*data = 0;

	hw->mac.ops.check_link(hw, &link_speed, &link_up, true);
	if (link_up)
		*data = 0;
	else
		*data = 1;
	return *data;
}

#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
static int rnpm_diag_test_count(struct net_device *netdev)
{
	return 0;
}

#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */

static void rnpm_diag_test(struct net_device *netdev,
						   struct ethtool_test *eth_test,
						   u64 *data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	bool if_running = netif_running(netdev);

	set_bit(__RNPM_TESTING, &adapter->state);
	if (eth_test->flags == ETH_TEST_FL_OFFLINE) {
		if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED) {
			int i;

			for (i = 0; i < adapter->num_vfs; i++) {
				if (adapter->vfinfo[i].clear_to_send) {
					netdev_warn(netdev,
								"%s",
								"offline diagnostic is not "
								"supported when VFs are "
								"present\n");
					data[0] = 1;
					data[1] = 1;
					data[2] = 1;
					data[3] = 1;
					eth_test->flags |= ETH_TEST_FL_FAILED;
					clear_bit(__RNPM_TESTING, &adapter->state);
					goto skip_ol_tests;
				}
			}
		}

		/* Offline tests */
		e_info(hw, "offline testing starting\n");

		if (if_running)
			rnpm_close(netdev);

		/* bringing adapter down disables SFP+ optics */
		if (hw->mac.ops.enable_tx_laser)
			hw->mac.ops.enable_tx_laser(hw);

		/* Link test performed before hardware reset so autoneg doesn't
		 * interfere with test result
		 */
		if (rnpm_link_test(adapter, &data[4]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		rnpm_reset(adapter);
		e_info(hw, "register testing starting\n");
		if (rnpm_reg_test(adapter, &data[0]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		data[1] = 1;
		data[2] = 1;
		/*
		rnpm_reset(adapter);
		e_info(hw, "eeprom testing starting\n");
		if (rnpm_eeprom_test(adapter, &data[1]))
			eth_test->flags |= ETH_TEST_FL_FAILED;


		rnpm_reset(adapter);
		e_info(hw, "interrupt testing starting\n");
		if (rnpm_intr_test(adapter, &data[2]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		*/
		/* If SRIOV or VMDq is enabled then skip MAC
		 * loopback diagnostic. */
		if (adapter->flags &
			(RNPM_FLAG_SRIOV_ENABLED | RNPM_FLAG_VMDQ_ENABLED)) {
			e_info(hw,
				   "Skip MAC loopback diagnostic in VT "
				   "mode\n");
			data[3] = 0;
			goto skip_loopback;
		}

		data[3] = 1;
		/* loopback test is not added now */
		/*
		rnpm_reset(adapter);
		e_info(hw, "loopback testing starting\n");
		// todo Loopback test
		if (rnpm_loopback_test(adapter, &data[3]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		*/
	skip_loopback:
		rnpm_reset(adapter);
		/* clear testing bit and return adapter to previous state */
		clear_bit(__RNPM_TESTING, &adapter->state);
		if (if_running)
			rnpm_open(netdev);
	} else {
		e_info(hw, "online testing starting\n");

		/* if adapter is down, SFP+ optics will be disabled */
		if (!if_running && hw->mac.ops.enable_tx_laser)
			hw->mac.ops.enable_tx_laser(hw);

		/* Online tests */
		if (rnpm_link_test(adapter, &data[4]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		/* Offline tests aren't run; pass by default */
		data[0] = 0;
		data[1] = 0;
		data[2] = 0;
		data[3] = 0;

		clear_bit(__RNPM_TESTING, &adapter->state);
	}

	/* if adapter was down, ensure SFP+ optics are disabled again */
	if (!if_running && hw->mac.ops.disable_tx_laser)
		hw->mac.ops.disable_tx_laser(hw);
skip_ol_tests:
	msleep_interruptible(4 * 1000);
}

#endif // CLOST_SELF_TEST

#ifdef ETHTOOL_GLINKSETTINGS
/**
 * rnpm_set_link_ksettings - Set Speed and Duplex
 * @netdev: network interface device structure
 * @ks: ethtool ksettings
 *
 * Set speed/duplex per media_types advertised/forced
 **/
static int rnpm_set_link_ksettings(struct net_device *netdev,
								   const struct ethtool_link_ksettings *ks)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	struct ethtool_link_ksettings safe_ks;
	struct ethtool_link_ksettings copy_ks;
	bool autoneg_changed = false, duplex_changed = false;
	int timeout = 50;
	int err = 0;
	u8 autoneg;
	u32 advertising_link_speed;

	/* copy the ksettings to copy_ks to avoid modifying the origin */
	memcpy(&copy_ks, ks, sizeof(struct ethtool_link_ksettings));

	/* save autoneg out of ksettings */
	autoneg = copy_ks.base.autoneg;
	rnpm_logd(LOG_ETHTOOL,
			  "%s %s set link: speed=%d port=%d duplex=%d autoneg=%d "
			  "phy_address=%d\n",
			  __func__,
			  netdev->name,
			  copy_ks.base.speed,
			  copy_ks.base.port,
			  copy_ks.base.duplex,
			  copy_ks.base.autoneg,
			  copy_ks.base.phy_address);

	/* get our own copy of the bits to check against */
	memset(&safe_ks, 0, sizeof(struct ethtool_link_ksettings));
	safe_ks.base.cmd = copy_ks.base.cmd;
	safe_ks.base.link_mode_masks_nwords = copy_ks.base.link_mode_masks_nwords;

	if (rnpm_get_link_ksettings(netdev, &safe_ks)) {
		/* return err */
		return 0;
	}

	/* Checkout the media_type */
	if (hw->phy.media_type != rnpm_media_type_fiber &&
		hw->phy.media_type != rnpm_media_type_copper &&
		hw->phy.media_type != rnpm_media_type_backplane &&
		hw->phy.media_type != rnpm_media_type_cx4 &&
		hw->phy.media_type != rnpm_media_type_da)
		return -EOPNOTSUPP;

	/* Get link modes supported by hardware and check against modes
	 * requested by user.  Return an error if unsupported mode was set.
	 */
	if (!bitmap_subset(copy_ks.link_modes.advertising,
					   safe_ks.link_modes.supported,
					   __ETHTOOL_LINK_MODE_MASK_NBITS))
		return -EINVAL;

	/* set autoneg back to what it currently is */
	copy_ks.base.autoneg = safe_ks.base.autoneg;

	memset(&advertising_link_speed, 0, sizeof(u32));

	/* Check autoneg */
	if (autoneg == AUTONEG_ENABLE) {
		/* If autoneg was not already enabled */
		if (!(adapter->an)) {
			/* If autoneg is not supported, return error */
			if (!ethtool_link_ksettings_test_link_mode(
					&safe_ks, supported, Autoneg)) {
				netdev_info(netdev, "Autoneg not supported on this phy\n");
				err = -EINVAL;
				goto done;
			}
			/* Autoneg is allowed to change */
			autoneg_changed = true;
		}

		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10baseT_Full))
			advertising_link_speed |= RNPM_LINK_SPEED_10_FULL;
		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 100baseT_Full))
			advertising_link_speed |= RNPM_LINK_SPEED_100_FULL;
		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 1000baseT_Full) ||
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 1000baseX_Full) ||
#endif
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 1000baseKX_Full))
			advertising_link_speed |= RNPM_LINK_SPEED_1GB_FULL;

		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10baseT_Half))
			advertising_link_speed |= RNPM_LINK_SPEED_10_HALF;
		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 100baseT_Half))
			advertising_link_speed |= RNPM_LINK_SPEED_100_HALF;
		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 1000baseT_Half))
			advertising_link_speed |= RNPM_LINK_SPEED_1GB_HALF;
		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseT_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseKX4_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseKR_Full) ||
#ifdef HAVE_ETHTOOL_NEW_10G_BITS
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseCR_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseSR_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 10000baseLR_Full))
#else
			0)
#endif /* HAVE_ETHTOOL_NEW_10G_BITS */
			advertising_link_speed |= RNPM_LINK_SPEED_10GB_FULL;

		if (ethtool_link_ksettings_test_link_mode(
				ks, advertising, 40000baseKR4_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 40000baseCR4_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 40000baseSR4_Full) ||
			ethtool_link_ksettings_test_link_mode(
				ks, advertising, 40000baseLR4_Full))
			advertising_link_speed |= RNPM_LINK_SPEED_40GB_FULL;

		if (advertising_link_speed) {
			hw->phy.autoneg_advertised = advertising_link_speed;
		} else {
				err = -EINVAL;
				goto done;
		}
		if (hw->is_sgmii && hw->mac.autoneg == false)
			autoneg_changed = true;
		hw->mac.autoneg = true;
	} else {
		/* If autoneg is currently enabled */
		if (adapter->an) {
			/* If autoneg is supported 10GBASE_T is the only PHY
			 * that can disable it, so otherwise return error
			 */
			if (ethtool_link_ksettings_test_link_mode(
					&safe_ks, supported, Autoneg) &&
				hw->phy.media_type != rnpm_media_type_copper) {
				netdev_info(netdev, "Autoneg cannot be disabled on this phy\n");
				err = -EINVAL;
				goto done;
			}
			/* Autoneg is allowed to change */
			autoneg_changed = true;
		}

#if 0
		if (ks->base.duplex == DUPLEX_HALF && hw->duplex != 0) {
			netdev_info(netdev, "unsupported duplex\n");
			return -EINVAL;
		}
#endif
		/* Only allow one speed at a time when autoneg is AUTONEG_DISABLE. */
		switch (ks->base.speed) {
			case SPEED_10:
				advertising_link_speed = RNPM_LINK_SPEED_10_FULL;
				break;
			case SPEED_100:
				advertising_link_speed = RNPM_LINK_SPEED_100_FULL;
				break;
			case SPEED_1000:
				advertising_link_speed = RNPM_LINK_SPEED_1GB_FULL;
				break;
			default:
				netdev_info(netdev, "unsupported speed\n");
				err = -EINVAL;
				goto done;
		}

		hw->mac.autoneg = false;
	}

	hw->phy.autoneg_advertised = RNPM_LINK_SPEED_UNKNOWN;
	/* If speed didn't get set, set it to what it currently is.
	 * This is needed because if advertise is 0 (as it is when autoneg
	 * is disabled) then speed won't get set.
	 */
	// old_link_speed = hw->phy.autoneg_advertised;
	// if (!advertising_link_speed)
	//	advertising_link_speed = old_link_speed;

	if (hw->is_sgmii) {
		// duplex_changed = !!(hw->mac.duplex != ks->base.duplex);
		hw->mac.duplex = ks->base.duplex;
		duplex_changed = true;
	}

	/* If the unsupported speed is set, return -EOPNOTSUPP error. */
	// if ((advertising_link_speed | hw->supported_link) != hw->supported_link)
	//	return -EOPNOTSUPP;

	// if (autoneg_changed || duplex_changed ||
	//	(hw->phy.autoneg_advertised != advertising_link_speed)) {
	/* this sets the link speed and restarts auto-neg */
	while (test_and_set_bit(__RNPM_IN_SFP_INIT, &adapter->state)) {
		timeout--;
		if (!timeout)
			return -EBUSY;
		usleep_range(1000, 2000);
	}

		hw->mac.autotry_restart = true;
		/* set speed */
		err = hw->mac.ops.setup_link(hw, advertising_link_speed, true);
		if (err) {
			e_info(probe, "setup link failed with code %d\n", err);
			// hw->mac.ops.setup_link(hw, old_link_speed, true);
		}
		clear_bit(__RNPM_IN_SFP_INIT, &adapter->state);
		//}

done:
	return err;
}

#else /* ETHTOOL_GLINKSETTINGS */
/**
 * rnpm_get_settings - Get Link Speed and Duplex settings
 * @netdev: network interface device structure
 * @ecmd: ethtool command
 *
 * Reports speed/duplex settings based on media_type.  Since we've backported
 * the new API constructs to use in the old API, this ends up just being a
 * wrapper to rnpm_get_link_ksettings.
 **/
static int rnpm_get_settings(struct net_device *netdev,
							 struct ethtool_cmd *ecmd)
{
	struct ethtool_link_ksettings ks;

	if (rnpm_get_link_ksettings(netdev, &ks)) {
		/* return err */
		return 0;
	}
	_kc_ethtool_ksettings_to_cmd(&ks, ecmd);
	// ecmd->transceiver = XCVR_EXTERNAL;
	return 0;
}

/**
 * rnpm_set_settings - Set Speed and Duplex
 * @netdev: network interface device structure
 * @ecmd: ethtool command
 *
 * Set speed/duplex per media_types advertised/forced
 **/
static int rnpm_set_settings(struct net_device *netdev,
							 struct ethtool_cmd *ecmd)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	struct ethtool_cmd safe_ecmd;
	bool change = false;
	int timeout = 50;
	int err = 0;
	u8 autoneg;
	u32 advertise, speed;
	u32 old_ethtool_advertising = 0;
	u32 old_link_speed, advertising_link_speed = 0;

	/* get our own copy of the bits to check against */
	memset(&safe_ecmd, 0, sizeof(struct ethtool_cmd));
	rnpm_get_settings(netdev, &safe_ecmd);

	/* Checkout the media_type */
	if (hw->phy.media_type != rnpm_media_type_fiber &&
		hw->phy.media_type != rnpm_media_type_copper &&
		hw->phy.media_type != rnpm_media_type_backplane &&
		hw->phy.media_type != rnpm_media_type_cx4 &&
		hw->phy.media_type != rnpm_media_type_da)
		return -EOPNOTSUPP;

	/* save autoneg and speed out of ecmd */
	autoneg = ecmd->autoneg;
	advertise = ecmd->advertising;

	/* set autoneg and speed back to what they currently are */
	ecmd->autoneg = safe_ecmd.autoneg;
	ecmd->advertising = safe_ecmd.advertising;

	/* Due to a bug in ethtool versions < 3.6 this check is necessary */
	old_ethtool_advertising =
		ecmd->supported &
		(ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
		 ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
		 ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full |
		 ADVERTISED_2500baseX_Full | ADVERTISED_10000baseT_Full);
	old_ethtool_advertising |=
		(old_ethtool_advertising | ADVERTISED_20000baseMLD2_Full |
		 ADVERTISED_20000baseKR2_Full);

	if (advertise == old_ethtool_advertising)
		netdev_info(netdev,
					"If you are not setting advertising to %x then you may "
					"have an old version of ethtool. Please update.\n",
					advertise);
	ecmd->cmd = safe_ecmd.cmd;
	/* If ecmd and safe_ecmd are not the same now, then they are
	 * trying to set something that we do not support
	 */
	// if (memcmp(ecmd, &safe_ecmd, sizeof(struct ethtool_cmd)))
	//	return -EOPNOTSUPP;

	/* Check autoneg */
	if (autoneg == AUTONEG_ENABLE) {
		/* If autoneg was not already enabled */
		if (!(adapter->an)) {
			/* If autoneg is not supported, return error */
			if (!(safe_ecmd.supported & SUPPORTED_Autoneg)) {
				netdev_info(netdev, "Autoneg not supported on this phy\n");
				err = -EINVAL;
				goto done;
			}
			/* Autoneg is allowed to change */
			change = true;
		}

		if (advertise & ADVERTISED_10baseT_Full)
			advertising_link_speed |= RNPM_LINK_SPEED_10_FULL;
		if (advertise & ADVERTISED_100baseT_Full)
			advertising_link_speed |= RNPM_LINK_SPEED_100_FULL;
		if (advertise & ADVERTISED_1000baseT_Full ||
			advertise & ADVERTISED_1000baseKX_Full)
			advertising_link_speed |= RNPM_LINK_SPEED_1GB_FULL;
		if (advertise & ADVERTISED_10000baseT_Full ||
			advertise & ADVERTISED_10000baseKX4_Full ||
			advertise & ADVERTISED_10000baseKR_Full)
			advertising_link_speed |= RNPM_LINK_SPEED_10GB_FULL;

		hw->phy.autoneg_advertised = advertising_link_speed;
		if (hw->is_sgmii && hw->mac.autoneg == false)
			change = true;
		hw->mac.autoneg = true;
	} else {
		/* If autoneg is currently enabled */
		if (adapter->an) {
			/* If autoneg is supported 10GBASE_T is the only phy
			 * that can disable it, so otherwise return error
			 */
			if (safe_ecmd.supported & SUPPORTED_Autoneg) {
				netdev_info(netdev, "Autoneg cannot be disabled on this phy\n");
				err = -EINVAL;
				goto done;
			}
			/* Autoneg is allowed to change */
			change = true;
		}

		if (ecmd->duplex == DUPLEX_HALF) {
			netdev_info(netdev, "unsupported duplex\n");
			return -EINVAL;
		}

		/* Only allow one speed at a time when autoneg is AUTONEG_DISABLE. */
		speed = ethtool_cmd_speed(ecmd);
		switch (speed) {
			case SPEED_10:
				advertising_link_speed = RNPM_LINK_SPEED_10_FULL;
				break;
			case SPEED_100:
				advertising_link_speed = RNPM_LINK_SPEED_100_FULL;
				break;
			case SPEED_1000:
				advertising_link_speed = RNPM_LINK_SPEED_1GB_FULL;
				break;
			case SPEED_10000:
				advertising_link_speed = RNPM_LINK_SPEED_10GB_FULL;
				break;
			default:
				netdev_info(netdev, "unsupported speed\n");
				return -EINVAL;
		}

		hw->phy.autoneg_advertised = RNPM_LINK_SPEED_UNKNOWN;
		hw->mac.autoneg = false;
	}

	if (advertise & ~safe_ecmd.supported) {
		err = -EINVAL;
		goto done;
	}

	/* If speed didn't get set, set it to what it currently is.
	 * This is needed because if advertise is 0 (as it is when autoneg
	 * is disabled) then speed won't get set.
	 */
	old_link_speed = hw->phy.autoneg_advertised;
	if (!advertising_link_speed)
		advertising_link_speed = old_link_speed;

	/* If the unsupported speed is set, return -EOPNOTSUPP error. */
	if ((advertising_link_speed | hw->supported_link) != hw->supported_link)
		return -EOPNOTSUPP;

	if (change || (hw->phy.autoneg_advertised != advertising_link_speed)) {
		/* this sets the link speed and restarts auto-neg */
		while (test_and_set_bit(__RNPM_IN_SFP_INIT, &adapter->state)) {
			timeout--;
			if (!timeout)
				return -EBUSY;
			usleep_range(1000, 2000);
		}

		hw->mac.autotry_restart = true;
		/* set speed */
		err = hw->mac.ops.setup_link(hw, advertising_link_speed, true);
		if (err) {
			e_info(probe, "setup link failed with code %d\n", err);
			hw->mac.ops.setup_link(hw, old_link_speed, true);
		}
		clear_bit(__RNPM_IN_SFP_INIT, &adapter->state);

	} else {
		netdev_info(netdev,
					"Nothing changed, exiting without setting anything.\n");
	}

done:
	return err;
}

#endif /* ETHTOOL_GLINKSETTINGS */


/**
 * rnpm_get_pauseparam -  Get Flow Control status
 * @netdev: netdevice structure
 * @pause: buffer to return pause parameters
 *
 * Return tx/rx-pause status
 **/
static void rnpm_get_pauseparam(struct net_device *netdev,
								struct ethtool_pauseparam *pause)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	/* we don't support autoneg */
	pause->autoneg = 0;

	/*
	if (hw->fc.disable_fc_autoneg)
		pause->autoneg = 0;
	else
		pause->autoneg = 1;
	*/

	if (hw->fc.current_mode == rnpm_fc_rx_pause) {
		pause->rx_pause = 1;
	} else if (hw->fc.current_mode == rnpm_fc_tx_pause) {
		pause->tx_pause = 1;
	} else if (hw->fc.current_mode == rnpm_fc_full) {
		pause->rx_pause = 1;
		pause->tx_pause = 1;
	}
}

/**
 * rnpm_set_pauseparam - Set Flow Control parameter
 * @netdev: network interface device structure
 * @pause: return tx/rx flow control status
 **/
static int rnpm_set_pauseparam(struct net_device *netdev,
							   struct ethtool_pauseparam *pause)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	struct rnpm_fc_info fc = hw->fc;

	/* we not support change in dcb mode */
	if (adapter->flags & RNPM_FLAG_DCB_ENABLED)
		return -EINVAL;

	/* we not support autoneg mode */
	if (pause->autoneg == AUTONEG_ENABLE)
		return -EINVAL;

	// fc.disable_fc_autoneg = (pause->autoneg != AUTONEG_ENABLE);
	fc.disable_fc_autoneg = 0;

	if ((pause->rx_pause && pause->tx_pause) || (pause->autoneg))
		fc.requested_mode = rnpm_fc_full;
	else if (pause->rx_pause && !pause->tx_pause)
		fc.requested_mode = rnpm_fc_rx_pause;
	else if (!pause->rx_pause && pause->tx_pause)
		fc.requested_mode = rnpm_fc_tx_pause;
	else
		fc.requested_mode = rnpm_fc_none;

	/* if the thing changed then we'll update and use new autoneg */
	if (memcmp(&fc, &hw->fc, sizeof(struct rnpm_fc_info))) {
		/* to tell all vf new pause status */
		hw->fc = fc;
		rnpm_msg_post_status(adapter, PF_PAUSE_STATUS);
		if (netif_running(netdev))
			rnpm_reinit_locked(adapter);
		else
			rnpm_reset(adapter);
	}

	return 0;
}

#ifdef ETHTOOL_GFECPARAM
static int rnpm_get_fecparam(struct net_device *netdev,
							 struct ethtool_fecparam *fecparam)
{
	int err;
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	err = rnpm_mbx_get_lane_stat(hw);
	if (err)
		return err;

	if (adapter->fec) {
		fecparam->active_fec = ETHTOOL_FEC_BASER;
	} else {
		fecparam->active_fec = ETHTOOL_FEC_NONE;
	}
	fecparam->fec = ETHTOOL_FEC_BASER;

	return 0;
}

static int rnpm_set_fecparam(struct net_device *netdev,
							 struct ethtool_fecparam *fecparam)
{
	// int err;
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	if (fecparam->fec & ETHTOOL_FEC_OFF) {
		return rnpm_set_lane_fun(hw, LANE_FUN_FEC, 0, 0, 0, 0);
	} else if (fecparam->fec & ETHTOOL_FEC_BASER) {
		return rnpm_set_lane_fun(hw, LANE_FUN_FEC, 1, 0, 0, 0);
	}

	return -EINVAL;
}
#endif

static u32 rnpm_get_msglevel(struct net_device *netdev)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	return adapter->msg_enable;
}

static void rnpm_set_msglevel(struct net_device *netdev, u32 data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	adapter->msg_enable = data;
}

#ifdef HAVE_ETHTOOL_SET_PHYS_ID
static int rnpm_set_phys_id(struct net_device *netdev,
							enum ethtool_phys_id_state state)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	switch (state) {
		case ETHTOOL_ID_ACTIVE:
			rnpm_mbx_led_set(hw, 1);
			return 2; /*twic peer seconds*/
		case ETHTOOL_ID_ON:
			rnpm_mbx_led_set(hw, 2);
			break;
		case ETHTOOL_ID_OFF:
			rnpm_mbx_led_set(hw, 3);
			break;
		case ETHTOOL_ID_INACTIVE:
			rnpm_mbx_led_set(hw, 0);
			break;
		default:
			return -ENOENT;
	}

	return 0;
}
#else  /* HAVE_ETHTOOL_SET_PHYS_ID */
static int rnpm_phys_id(struct net_device *netdev, u32 data)
{
	return 0;
}
#endif /* HAVE_ETHTOOL_SET_PHYS_ID */

#ifdef HAVE_ETHTOOL_GET_TS_INFO
static int rnpm_get_ts_info(struct net_device *dev,
							struct ethtool_ts_info *info)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
#ifndef NO_PTP
	/*For we just set it as pf0 */
	if (!(adapter->flags2 & RNPM_FLAG2_PTP_ENABLED))
		return ethtool_op_get_ts_info(dev, info);
	if (adapter->ptp_clock)
		info->phc_index = ptp_clock_index(adapter->ptp_clock);
	else
		info->phc_index = -1;

	ptp_dbg("phc_index is %d\n", info->phc_index);
	info->so_timestamping =
		SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE |
		SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RAW_HARDWARE;

	info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);

	info->rx_filters =
		BIT(HWTSTAMP_FILTER_NONE) | BIT(HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
		BIT(HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
		BIT(HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
		BIT(HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
		BIT(HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
		BIT(HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) | BIT(HWTSTAMP_FILTER_ALL);
#ifdef PTP_802_AS1
	/* 802.AS1 */
	BIT(HWTSTAMP_FILTER_PTP_V2_L2_EVENT) | BIT(HWTSTAMP_FILTER_PTP_V2_L2_SYNC) |
		BIT(HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ);
#endif

#else
	info->phc_index = -1;

#endif
	return 0;
}

#endif /* HAVE_ETHTOOL_GET_TS_INFO */

#ifdef ETHTOOL_SCHANNELS
static unsigned int rnpm_max_channels(struct rnpm_adapter *adapter)
{
	unsigned int max_combined;

	if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED) {
		/* SR-IOV currently only allows 2 queue on the PF */
		max_combined = PF_RING_CNT_WHEN_IOV_ENABLED;
	} else {
		/* support up to 16 queues with RSS */
		max_combined = adapter->max_ring_pair_counts;
		/* should not large than q_vectors ? */
		// if dcb is off
		// max_combined = adapter->num_q_vectors;
	}

	return max_combined;
}

/**
 * rnpm_get_channels - Get the current channels enabled and max supported etc.
 * @dev: network interface device structure
 * @ch: ethtool channels structure
 *
 * We don't support separate tx and rx queues as channels. The other count
 * represents how many queues are being used for control. max_combined counts
 * how many queue pairs we can support. They may not be mapped 1 to 1 with
 * q_vectors since we support a lot more queue pairs than q_vectors.
 **/
static void rnpm_get_channels(struct net_device *dev,
							  struct ethtool_channels *ch)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);

	/* report maximum channels */
	ch->max_combined = rnpm_max_channels(adapter);

	/* report info for other vector */
	ch->max_other = NON_Q_VECTORS;
	ch->other_count = NON_Q_VECTORS;

	/* record RSS queues */
	ch->combined_count = adapter->ring_feature[RING_F_RSS].indices;

	/* nothing else to report if RSS is disabled */
	if (ch->combined_count == 1)
		return;

	/* we do not support ATR queueing if SR-IOV is enabled */
	if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED)
		return;

	/* same thing goes for being DCB enabled */
	if (netdev_get_num_tc(dev) > 1)
		return;

	/* report flow director queues as maximum channels */
	// ch->combined_count = adapter->ring_feature[RING_F_FDIR].indices;
}

/**
 * rnpm_set_channels - Set the new channels count.
 * @dev: network interface device structure
 * @ch: ethtool channels structure
 *
 * The new channels count may not be the same as requested by the user
 * since it gets rounded down to a power of 2 value.
 **/
static int rnpm_set_channels(struct net_device *dev,
							 struct ethtool_channels *ch)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
	unsigned int count = ch->combined_count;

	/* verify they are not requesting separate vectors */
	if (!count || ch->rx_count || ch->tx_count)
		return -EINVAL;

	/* verify other_count has not changed */
	if (ch->other_count != NON_Q_VECTORS)
		return -EINVAL;

	/* verify the number of channels does not exceed hardware limits */
	if (count > rnpm_max_channels(adapter))
		return -EINVAL;

	/* update feature limits from largest to smallest supported values */
	adapter->ring_feature[RING_F_FDIR].limit = count;

	/* cap RSS limit at 16 */
	/*
	if (count > RNPM_MAX_RSS_INDICES)
		count = RNPM_MAX_RSS_INDICES;
	*/
	if (count > adapter->max_ring_pair_counts)
		count = adapter->max_ring_pair_counts;
	/* use this to limit ring num */
	adapter->ring_feature[RING_F_RSS].limit = count;

	/* use setup TC to update any traffic class queue mapping */
	return rnpm_setup_tc(dev, netdev_get_num_tc(dev));
}

#endif /* ETHTOOL_SCHANNELS */

#ifdef ETHTOOL_GMODULEINFO

/**
 * rnpm_get_module_info - get (Q)SFP+ module type info
 * @netdev: network interface device structure
 * @modinfo: module EEPROM size and layout information structure
 **/
static int rnpm_get_module_info(struct net_device *dev,
								struct ethtool_modinfo *modinfo)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
	struct rnpm_hw *hw = &adapter->hw;
	u8 module_id, diag_supported;
	int rc;

	/* Check if firmware supports reading module EEPROM. */
	rc = rnpm_mbx_sfp_module_eeprom_info(
		hw, 0xA0, SFF_MODULE_ID_OFFSET, 1, &module_id);
	if (rc || module_id == 0xff) {
		return -EIO;
	}

	rc = rnpm_mbx_sfp_module_eeprom_info(
		hw, 0xA0, SFF_DIAG_SUPPORT_OFFSET, 1, &diag_supported);
	if (!rc) {
		switch (module_id) {
			case SFF_MODULE_ID_SFF:
			case SFF_MODULE_ID_SFP:

				/* TODO: get info from firmware */
				modinfo->type = ETH_MODULE_SFF_8472;
				modinfo->eeprom_len = ETH_MODULE_SFF_8472_LEN;
				if (!diag_supported)
					modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
				break;
			case SFF_MODULE_ID_QSFP:
			case SFF_MODULE_ID_QSFP_PLUS:
				modinfo->type = ETH_MODULE_SFF_8436;
				modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
				break;
			case SFF_MODULE_ID_QSFP28:
				modinfo->type = ETH_MODULE_SFF_8636;
				modinfo->eeprom_len = RNPM_MODULE_QSFP_MAX_LEN;
				break;
			default:
				netdev_err(
					dev, "SFP module type unrecognized or no SFP connector.\n");
				return -EINVAL;
		}
	}

	return 0;
}

/**
 * rnpm_get_module_eeprom - fills buffer with (Q)SFP+ module memory contents
 * @netdev: network interface device structure
 * @ee: EEPROM dump request structure
 * @data: buffer to be filled with EEPROM contents
 **/
static int rnpm_get_module_eeprom(struct net_device *dev,
								  struct ethtool_eeprom *eeprom,
								  u8 *data)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
	struct rnpm_hw *hw = &adapter->hw;
	u16 start = eeprom->offset, length = eeprom->len;
	int rc = 0;

	memset(data, 0, eeprom->len);

	/* Read A0 portion of the EEPROM */
	if (start < ETH_MODULE_SFF_8436_LEN) {
		if (start + eeprom->len > ETH_MODULE_SFF_8436_LEN)
			length = ETH_MODULE_SFF_8436_LEN - start;
		rc = rnpm_mbx_sfp_module_eeprom_info(hw, 0xA0, start, length, data);
		if (rc)
			return rc;
		start += length;
		data += length;
		length = eeprom->len - length;
	}

	/* Read A2 portion of the EEPROM */
	if (length) {
		start -= ETH_MODULE_SFF_8436_LEN;
		rc = rnpm_mbx_sfp_module_eeprom_info(hw, 0xA2, start, length, data);
	}

	return rc;
}
#endif /* ETHTOOL_GMODULEINFO */

#ifdef HAVE_ETHTOOL_EXTENDED_RINGPARAMS
static void
rnpm_get_ringparam(struct net_device *netdev,
				   struct ethtool_ringparam *ring,
				   struct kernel_ethtool_ringparam __always_unused *ker,
				   struct netlink_ext_ack __always_unused *extack)
#else
static void rnpm_get_ringparam(struct net_device *netdev,
							   struct ethtool_ringparam *ring)
#endif /* HAVE_ETHTOOL_EXTENDED_RINGPARAMS */
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	/* all ring share the same status*/
	ring->rx_max_pending = RNPM_MAX_RXD;
	ring->tx_max_pending = RNPM_MAX_TXD;
	ring->rx_mini_max_pending = 0;
	ring->rx_jumbo_max_pending = 0;
	ring->rx_pending = adapter->rx_ring_item_count;
	ring->tx_pending = adapter->tx_ring_item_count;
	ring->rx_mini_pending = 0;
	ring->rx_jumbo_pending = 0;
}

#ifdef HAVE_ETHTOOL_EXTENDED_RINGPARAMS
static int
rnpm_set_ringparam(struct net_device *netdev,
				   struct ethtool_ringparam *ring,
				   struct kernel_ethtool_ringparam __always_unused *ker,
				   struct netlink_ext_ack __always_unused *extack)
#else
static int rnpm_set_ringparam(struct net_device *netdev,
							  struct ethtool_ringparam *ring)
#endif /* HAVE_ETHTOOL_EXTENDED_RINGPARAMS */
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_ring *temp_ring;
	int i, err = 0;
	u32 new_rx_count, new_tx_count;

	if ((ring->rx_mini_pending) || (ring->rx_jumbo_pending))
		return -EINVAL;
	if ((ring->tx_pending < RNPM_MIN_TXD) ||
		(ring->tx_pending > RNPM_MAX_TXD) ||
		(ring->rx_pending < RNPM_MIN_RXD) ||
		(ring->rx_pending > RNPM_MAX_RXD)) {
		netdev_info(
			netdev,
			"Descriptors requested (Tx: %d / Rx: %d) out of range [%d-%d]\n",
			ring->tx_pending,
			ring->rx_pending,
			RNPM_MIN_TXD,
			RNPM_MAX_TXD);
		return -EINVAL;
	}

	new_tx_count = clamp_t(u32, ring->tx_pending, RNPM_MIN_TXD, RNPM_MAX_TXD);
	new_tx_count = ALIGN(new_tx_count, RNPM_REQ_TX_DESCRIPTOR_MULTIPLE);

	new_rx_count = clamp_t(u32, ring->rx_pending, RNPM_MIN_RXD, RNPM_MAX_RXD);
	new_rx_count = ALIGN(new_rx_count, RNPM_REQ_RX_DESCRIPTOR_MULTIPLE);

	if ((new_tx_count == adapter->tx_ring_item_count) &&
		(new_rx_count == adapter->rx_ring_item_count)) {
		/* nothing to do */
		return 0;
	}

	/* we can't change rx ring len in srivo mode ? */
	/*
	if ((adapter->flags & RNPM_FLAG_SRIOV_ENABLED) &&
		(new_rx_count != adapter->rx_ring_item_count)) {
		return -EINVAL;
	}
	*/

	while (test_and_set_bit(__RNPM_RESETTING, &adapter->state))
		usleep_range(1000, 2000);

	if (!netif_running(adapter->netdev)) {
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i]->count = new_tx_count;
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i]->count = new_rx_count;
		adapter->tx_ring_item_count = new_tx_count;
		adapter->rx_ring_item_count = new_rx_count;
		goto clear_reset;
	}

	/* allocate temporary buffer to store rings in */
	i = max_t(int, adapter->num_tx_queues, adapter->num_rx_queues);
	temp_ring = vmalloc(i * sizeof(struct rnpm_ring));
	if (!temp_ring) {
		err = -ENOMEM;
		goto clear_reset;
	}
	memset(temp_ring, 0x00, i * sizeof(struct rnpm_ring));

	if (new_rx_count != adapter->rx_ring_item_count) {
		for (i = 0; i < adapter->num_rx_queues; i++) {
			struct rnpm_ring *ring = adapter->rx_ring[i];

			ring->reset_count = new_rx_count;
			ring->ring_flags |= RNPM_RING_FLAG_CHANGE_RX_LEN;
		}
	}
	rnpm_down(adapter);
	/*
	 * Setup new Tx resources and free the old Tx resources in that order.
	 * We can then assign the new resources to the rings via a memcpy.
	 * The advantage to this approach is that we are guaranteed to still
	 * have resources even in the case of an allocation failure.
	 */
	if (new_tx_count != adapter->tx_ring_item_count) {
		for (i = 0; i < adapter->num_tx_queues; i++) {
			memcpy(
				&temp_ring[i], adapter->tx_ring[i], sizeof(struct rnpm_ring));

			temp_ring[i].count = new_tx_count;
			err = rnpm_setup_tx_resources(&temp_ring[i], adapter);
			if (err) {
				while (i) {
					i--;
					rnpm_free_tx_resources(&temp_ring[i]);
				}
				goto err_setup;
			}
		}

		for (i = 0; i < adapter->num_tx_queues; i++) {
			rnpm_free_tx_resources(adapter->tx_ring[i]);
			memcpy(
				adapter->tx_ring[i], &temp_ring[i], sizeof(struct rnpm_ring));
		}

		adapter->tx_ring_item_count = new_tx_count;
	}

	/* Repeat the process for the Rx rings if needed */
	if (new_rx_count != adapter->rx_ring_item_count) {
		for (i = 0; i < adapter->num_rx_queues; i++) {
			memcpy(
				&temp_ring[i], adapter->rx_ring[i], sizeof(struct rnpm_ring));
			/* setup ring count */
			if (!(adapter->rx_ring[i]->ring_flags &
				  RNPM_RING_FLAG_DELAY_SETUP_RX_LEN)) {
				temp_ring[i].count = new_rx_count;
			} else {
				/* setup temp count */
				temp_ring[i].count = temp_ring[i].temp_count;
				adapter->rx_ring[i]->reset_count = new_rx_count;
			}
			err = rnpm_setup_rx_resources(&temp_ring[i], adapter);
			if (err) {
				while (i) {
					i--;
					rnpm_free_rx_resources(&temp_ring[i]);
				}
				goto err_setup;
			}
		}

		for (i = 0; i < adapter->num_rx_queues; i++) {
			rnpm_free_rx_resources(adapter->rx_ring[i]);
			memcpy(
				adapter->rx_ring[i], &temp_ring[i], sizeof(struct rnpm_ring));
		}
		adapter->rx_ring_item_count = new_rx_count;
	}

err_setup:
	rnpm_up(adapter);
	vfree(temp_ring);
clear_reset:
	clear_bit(__RNPM_RESETTING, &adapter->state);
	return err;
}

static void rnpm_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	char *p = (char *)data;
	int i;
	struct rnpm_ring *ring;
	u32 dma_ch;

	switch (stringset) {
		/* maybe we don't support test? */
#ifndef CLOST_SELF_TEST
		case ETH_SS_TEST:
			for (i = 0; i < RNPM_TEST_LEN; i++) {
				memcpy(data, rnpm_gstrings_test[i], ETH_GSTRING_LEN);
				data += ETH_GSTRING_LEN;
			}
			break;
#endif /* CLOST_SELF_TEST */
		case ETH_SS_STATS:
			for (i = 0; i < RNPM_GLOBAL_STATS_LEN; i++) {
				memcpy(
					p, rnpm_gstrings_net_stats[i].stat_string, ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < RNPM_HWSTRINGS_STATS_LEN; i++) {
				memcpy(p, rnpm_hwstrings_stats[i].stat_string, ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < RNPM_NUM_TX_QUEUES; i++) {
#define SHORT_STATS

#ifdef SHORT_STATS
				//====  tx ========
				ring = adapter->tx_ring[i];
				dma_ch = ring->rnpm_queue_idx;
				sprintf(p, "---\n     queue%u_tx_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_bytes", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_tx_restart", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_busy", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_done_old", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_clean_desc", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_poll_count", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_irq_more", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_tx_hw_head", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_hw_tail", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_sw_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_sw_next_to_use", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_bytes", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_bytes_to_hw", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_todo_update", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_done_bytes", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_added_vlan_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_irq_miss", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_equal_count", i);
				p += ETH_GSTRING_LEN;
				//====  rx ========
				ring = adapter->rx_ring[i];
				dma_ch = ring->rnpm_queue_idx;
				sprintf(p, "queue%u_rx_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_bytes", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_driver_dropped_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_rsc", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_rsc_flush", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_non_eop_descs", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_alloc_page_failed", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_alloc_buff_failed", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_csum_offload_errs", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_csum_offload_good", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_poll_again_count", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_rm_vlan_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_alloc_rx_page", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_hw_head", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_hw_tail", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_sw_next_to_use", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_sw_next_to_clean", i);
				/* dbg desc */
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_irq_miss", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_equal_count", i);
				p += ETH_GSTRING_LEN;
#else
				//====  tx ========
				ring = adapter->tx_ring[i];
				dma_ch = ring->rnpm_queue_idx;
				sprintf(p, "queue%u_dma%u_tx_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_tx_restart", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_busy", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_done_old", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_clean_desc", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_poll_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_irq_more", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_tx_hw_head", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_hw_tail", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_sw_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_sw_next_to_use", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_bytes_to_hw", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_todo_update", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_done_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_added_vlan_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_irq_miss", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_equal_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				//====  rx ========
				ring = adapter->rx_ring[i];
				dma_ch = ring->rnpm_queue_idx;
				sprintf(p, "queue%u_dma%u_rx_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_rx_driver_drop_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_rsc", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_rsc_flush", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_non_eop_descs", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_alloc_page_failed", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_alloc_buff_failed", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_csum_offload_errs", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_csum_offload_good", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_poll_again_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_rm_vlan_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_alloc_rx_page", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_rx_hw_head", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_hw_tail", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_sw_next_to_use", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_sw_next_to_clean", i, dma_ch);
				/* dbg desc */
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_irq_miss", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_equal_count", i, dma_ch);
				p += ETH_GSTRING_LEN;

#endif /* SHORT_STATS */
			}

			break;
#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
		case ETH_SS_PRIV_FLAGS:
			memcpy(data,
				   rnpm_priv_flags_strings,
				   RNPM_PRIV_FLAGS_STR_LEN * ETH_GSTRING_LEN);
			break;
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */

#ifdef HAVE_ETHTOOL_PHY_STATISTICS
		case ETH_SS_PHY_STATS:
			memcpy(data,
				   rnpm_phy_statistics_strings,
				   RNPM_PHY_STATISTICS_STR_LEN * ETH_GSTRING_LEN);
			break;
#endif /* HAVE_ETHTOOL_PHY_STATISTICS */
	}
}

__maybe_unused static int rnpm_get_dump_flag(struct net_device *netdev,
											 struct ethtool_dump *dump)
{
	struct rnpm_adapter *adapter = (struct rnpm_adapter *)netdev_priv(netdev);
	// struct rnpm_hw *hw = &adapter->hw;
	// struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;

	rnpm_mbx_get_dump(&adapter->hw, 0, NULL, 0);

	dump->flag = adapter->hw.dump.flag;
	dump->len = adapter->hw.dump.len;
	dump->version = adapter->hw.dump.version;

	return 0;
}

__maybe_unused static int rnpm_get_dump_data(struct net_device *netdev,
											 struct ethtool_dump *dump,
											 void *buffer)
{
	int err;
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	if ((err =
			 rnpm_mbx_get_dump(&adapter->hw, dump->flag, buffer, dump->len))) {
		return err;
	}

	dump->flag = adapter->hw.dump.flag;
	dump->len = adapter->hw.dump.len;
	dump->version = adapter->hw.dump.version;

	return 0;
}

__maybe_unused static int rnpm_set_dump(struct net_device *netdev,
										struct ethtool_dump *dump)
{
	// int err;
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	rnpm_mbx_set_dump(&adapter->hw, dump->flag);

	return 0;
}

#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
static int rnpm_get_stats_count(struct net_device *netdev)
{
	return RNPM_STATS_LEN;
}

#else /* HAVE_ETHTOOL_GET_SSET_COUNT */

static int rnpm_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
		/* now we don't support test */
#ifndef CLOST_SELF_TEST
		case ETH_SS_TEST:
			return RNPM_TEST_LEN;
#endif
		case ETH_SS_STATS:
			return RNPM_STATS_LEN;
		case ETH_SS_PRIV_FLAGS:
			return RNPM_PRIV_FLAGS_STR_LEN;
#ifdef HAVE_ETHTOOL_PHY_STATISTICS
		case ETH_SS_PHY_STATS:
			return RNPM_PHY_STATISTICS_STR_LEN;
#endif
		default:
			return -EOPNOTSUPP;
	}
}

/**
 * rnpm_get_priv_flags - report device private flags
 * @dev: network interface device structure
 *
 * The get string set count and the string set should be matched for each
 * flag returned.  Add new strings for each flag to the rnpm_gstrings_priv_flags
 * array.
 *
 * Returns a u32 bitmap of flags.
 **/
static u32 rnpm_get_priv_flags(struct net_device *netdev)
{
	struct rnpm_adapter *adapter = (struct rnpm_adapter *)netdev_priv(netdev);
	// struct rnpm_hw *hw = &adapter->hw;
	struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;
	u32 priv_flags = 0;
	// dbg("adapter priv is %x\n",iface->priv_flags);

	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_MAC_LOOPBACK)
		priv_flags |= RNPM_MAC_LOOPBACK;
	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_SWITCH_LOOPBACK)
		priv_flags |= RNPM_SWITCH_LOOPBACK;
	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_VEB_ENABLE)
		priv_flags |= RNPM_VEB_ENABLE;
	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH)
		priv_flags |= RNPM_PCIE_CACHE_ALIGN_PATCH;
	if (adapter->priv_flags & RNPM_PRIV_FLAG_PADDING_DEBUG)
		priv_flags |= RNPM_PADDING_DEBUG;
	if (adapter->priv_flags & RNPM_PRIV_FLAG_PTP_DEBUG)
		priv_flags |= RNPM_PTP_FEATURE;
	if (adapter->priv_flags & RNPM_PRIV_FLAG_SIMUATE_DOWN)
		priv_flags |= RNPM_SIMULATE_DOWN;
	if (adapter->priv_flags & RNPM_PRIV_FLAG_TO_RPU)
		priv_flags |= RNPM_TO_RPU;
	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_LEN_ERR) {
		priv_flags |= RNPM_LEN_ERR;
	}

	return priv_flags;
}

static int rnpm_priv_status_update(struct rnpm_adapter *adapter)
{
	struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;
	int i;
	u32 priv = 0;
	u32 data_old, data_new;
    	unsigned long flags;

	spin_lock_irqsave(&pf_adapter->priv_flags_lock,flags);
	data_old = rd32(pf_adapter, RNPM_DMA_CONFIG);
	data_new = data_old;
	for (i = 0; i < pf_adapter->adapter_cnt; i++) {
		if (rnpm_port_is_valid(pf_adapter, i)) {
			priv |= pf_adapter->adapter[i]->priv_flags;
		}
	}
	if (priv & RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH) {
		pf_adapter->priv_flags |= RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH;
		SET_BIT(padding_enable, data_new);
	} else {
		pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH);
		CLR_BIT(padding_enable, data_new);
	}

	if (priv & RNPM_PRIV_FLAG_MAC_LOOPBACK) {
		pf_adapter->priv_flags |= RNPM_PRIV_FLAG_MAC_LOOPBACK;
		SET_BIT(mac_loopback, data_new);
	} else {
		pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_MAC_LOOPBACK);
		CLR_BIT(mac_loopback, data_new);
	}

	if (priv & RNPM_PRIV_FLAG_MAC_LOOPBACK) {
		pf_adapter->priv_flags |= RNPM_PRIV_FLAG_SWITCH_LOOPBACK;
		SET_BIT(switch_loopback, data_new);
	} else {
		pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_SWITCH_LOOPBACK);
		CLR_BIT(switch_loopback, data_new);
	}

	if (priv & RNPM_PRIV_FLAG_VEB_ENABLE) {
		pf_adapter->priv_flags |= RNPM_PRIV_FLAG_VEB_ENABLE;
		SET_BIT(veb_enable, data_new);
	} else {
		pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_VEB_ENABLE);
		CLR_BIT(veb_enable, data_new);
	}

	if (data_old != data_new)
		wr32(pf_adapter, RNPM_DMA_CONFIG, data_new);
	spin_unlock_irqrestore(&pf_adapter->priv_flags_lock, flags);
	return 0;
}

static int rnpm_priv_err_mask_set(struct rnpm_adapter *adapter)
{
	struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;
	u32 data_old, data_new;
    	unsigned long flags;

	spin_lock_irqsave(&pf_adapter->priv_flags_lock, flags);
	data_new = data_old = rd32(pf_adapter, RNPM_ETH_ERR_MASK_VECTOR);

	if (pf_adapter->priv_flags & RNPM_PRIV_FLAG_LEN_ERR) {
		// pf_adapter->priv_flags |= RNPM_PRIV_FLAG_LEN_ERR;
		data_new |= (ETH_ERR_PKT_LEN_ERR | ETH_ERR_HDR_LEN_ERR);
	} else {
		// pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_LEN_ERR);
		data_new &= ~(ETH_ERR_PKT_LEN_ERR | ETH_ERR_HDR_LEN_ERR);
	}

	if (data_old != data_new) {
		wr32(pf_adapter, RNPM_ETH_ERR_MASK_VECTOR, data_new);
	}
	spin_unlock_irqrestore(&pf_adapter->priv_flags_lock, flags);
	return 0;
}

/**
 * rnpm_set_priv_flags - set private flags
 * @dev: network interface device structure
 * @flags: bit flags to be set
 **/
static int rnpm_set_priv_flags(struct net_device *netdev, u32 priv_flags)
{
	struct rnpm_adapter *adapter = (struct rnpm_adapter *)netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	u32 orig_flags, new_flags;

	orig_flags = rd32(hw, RNPM_DMA_CONFIG);
	new_flags = orig_flags;

	if (priv_flags & RNPM_MAC_LOOPBACK) {
		SET_BIT(mac_loopback, new_flags);
		adapter->priv_flags |= RNPM_PRIV_FLAG_MAC_LOOPBACK;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_MAC_LOOPBACK) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_MAC_LOOPBACK);
		CLR_BIT(mac_loopback, new_flags);
	}

	if (priv_flags & RNPM_SWITCH_LOOPBACK) {
		SET_BIT(switch_loopback, new_flags);
		adapter->priv_flags |= RNPM_PRIV_FLAG_SWITCH_LOOPBACK;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_SWITCH_LOOPBACK) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_SWITCH_LOOPBACK);
		CLR_BIT(switch_loopback, new_flags);
	}

	if (priv_flags & RNPM_VEB_ENABLE) {
		SET_BIT(veb_enable, new_flags);
		adapter->priv_flags |= RNPM_PRIV_FLAG_VEB_ENABLE;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_VEB_ENABLE) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_VEB_ENABLE);
		CLR_BIT(veb_enable, new_flags);
	}

	if (priv_flags & RNPM_PCIE_CACHE_ALIGN_PATCH) {
		SET_BIT(padding_enable, new_flags);
		adapter->priv_flags |= RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_PCIE_CACHE_ALIGN_PATCH);
		CLR_BIT(padding_enable, new_flags);
	}

	if (priv_flags & RNPM_PADDING_DEBUG)
		adapter->priv_flags |= RNPM_PRIV_FLAG_PADDING_DEBUG;
	else if (adapter->priv_flags & RNPM_PRIV_FLAG_PADDING_DEBUG)
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_PADDING_DEBUG);

	if (priv_flags & RNPM_PTP_FEATURE) {
		adapter->priv_flags |= RNPM_PRIV_FLAG_PTP_DEBUG;
		adapter->flags2 |= ~RNPM_FLAG2_PTP_ENABLED;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_PTP_DEBUG) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_PTP_DEBUG);
		adapter->flags2 &= (~RNPM_FLAG2_PTP_ENABLED);
	}

	if (priv_flags & RNPM_SIMULATE_DOWN) {
		adapter->priv_flags |= RNPM_PRIV_FLAG_SIMUATE_DOWN;
		/* set check link again */
		adapter->flags |= RNPM_FLAG_NEED_LINK_UPDATE;
	} else if (adapter->priv_flags & RNPM_PRIV_FLAG_SIMUATE_DOWN) {
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_SIMUATE_DOWN);
		/* set check link again */
		adapter->flags |= RNPM_FLAG_NEED_LINK_UPDATE;
	}

	if (priv_flags & RNPM_TO_RPU)
		adapter->priv_flags |= RNPM_PRIV_FLAG_TO_RPU;
	else if (adapter->priv_flags & RNPM_PRIV_FLAG_TO_RPU)
		adapter->priv_flags &= (~RNPM_PRIV_FLAG_TO_RPU);

	if (priv_flags & RNPM_LEN_ERR) {
		adapter->pf_adapter->priv_flags |= RNPM_PRIV_FLAG_LEN_ERR;
		rnpm_priv_err_mask_set(adapter);
	} else if (adapter->pf_adapter->priv_flags & RNPM_PRIV_FLAG_LEN_ERR) {
		adapter->pf_adapter->priv_flags &= (~RNPM_PRIV_FLAG_LEN_ERR);
		rnpm_priv_err_mask_set(adapter);
	}

	if (orig_flags != new_flags) {
		/* we not support this in multiports */
		// if (adapter->flags & RNPM_FLAG_MUTIPORT_ENABLED)
		//       return -EINVAL;
		wr32(hw, RNPM_DMA_CONFIG, new_flags);

		rnpm_priv_status_update(adapter);
	}

	/* if ft_padding changed */
	if (CHK_BIT(padding_enable, orig_flags) !=
		CHK_BIT(padding_enable, new_flags))
		rnpm_msg_post_status(adapter, PF_FT_PADDING_STATUS);
	return 0;
}

#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */


/* ethtool register test data */



/**
 * rnpm_get_coalesce - get a netdev's coalesce settings
 * @netdev: the netdev to check
 * @ec: ethtool coalesce data structure
 * @kec: kernel coalesce parameter
 * @extack: kernel extack parameter
 *
 * Gets the coalesce settings for a particular netdev. Note that if user has
 * modified per-queue settings, this only guarantees to represent queue 0. See
 * __rnpm_get_coalesce for more details.
 **/
static int rnpm_get_coalesce(struct net_device *netdev,
#ifdef HAVE_ETHTOOL_COALESCE_EXTACK
							 struct ethtool_coalesce *coal,
							 struct kernel_ethtool_coalesce __maybe_unused *kernel_coal,
							 struct netlink_ext_ack __maybe_unused *extack)
#else
							 struct ethtool_coalesce *coal)
#endif
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	coal->use_adaptive_tx_coalesce = adapter->adaptive_tx_coal;
	coal->tx_coalesce_usecs = adapter->tx_usecs;
	coal->tx_coalesce_usecs_irq = 0;
	coal->tx_max_coalesced_frames = adapter->tx_frames;
	coal->tx_max_coalesced_frames_irq = adapter->tx_work_limit;

	coal->use_adaptive_rx_coalesce = adapter->adaptive_rx_coal;
	coal->rx_coalesce_usecs_irq = 0;
	coal->rx_coalesce_usecs = adapter->rx_usecs;
	coal->rx_max_coalesced_frames = adapter->rx_frames;
	coal->rx_max_coalesced_frames_irq = adapter->napi_budge;

	/* this is not support */
	coal->pkt_rate_low = 0;
	coal->pkt_rate_high = 0;
	coal->rx_coalesce_usecs_low = 0;
	coal->rx_max_coalesced_frames_low = 0;
	coal->tx_coalesce_usecs_low = 0;
	coal->tx_max_coalesced_frames_low = 0;
	coal->rx_coalesce_usecs_high = 0;
	coal->rx_max_coalesced_frames_high = 0;
	coal->tx_coalesce_usecs_high = 0;
	coal->tx_max_coalesced_frames_high = 0;
	coal->rate_sample_interval = 0;

	return 0;
}

/**
 * rnpm_set_coalesce - set coalesce settings for every queue on the netdev
 * @netdev: the netdev to change
 * @ec: ethtool coalesce settings
 * @kec: kernel coalesce parameter
 * @extack: kernel extack parameter
 *
 * This will set each queue to the same coalesce settings.
 **/
static int rnpm_set_coalesce(struct net_device *netdev,
#ifdef HAVE_ETHTOOL_COALESCE_EXTACK
							 struct ethtool_coalesce *ec,
							 struct kernel_ethtool_coalesce __maybe_unused *kernel_coal,
							 struct netlink_ext_ack __maybe_unused *extack)
#else
							 struct ethtool_coalesce *ec)
#endif /* HAVE_ETHTOOL_COALESCE_EXTACK */
{
	int reset = 0;
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	u32 value;
	/* we don't support close tx and rx coalesce */
	if (!(ec->use_adaptive_tx_coalesce) || !(ec->use_adaptive_rx_coalesce))
		return -EINVAL;

	if (ec->tx_max_coalesced_frames_irq) {
		/* check coalesce frame irq */
		value = clamp_t(u32,
						ec->tx_max_coalesced_frames_irq,
						RNPM_MIN_TX_WORK,
						RNPM_MAX_TX_WORK);
		value = ALIGN(value, RNPM_WORK_ALIGN);

		if (adapter->tx_work_limit != value) {
			reset = 1;
			adapter->tx_work_limit = value;
		}
	}

	if (ec->tx_max_coalesced_frames) {
		/* check vlaue */
		value = clamp_t(u32,
						ec->tx_max_coalesced_frames,
						RNPM_MIN_TX_FRAME,
						RNPM_MAX_TX_FRAME);
		if (adapter->tx_frames != value) {
			reset = 1;
			adapter->tx_frames = value;
		}
	}

	if (ec->tx_coalesce_usecs) {
		/* check vlaue */
		value = clamp_t(
			u32, ec->tx_coalesce_usecs, RNPM_MIN_TX_USEC, RNPM_MAX_TX_USEC);
		value = value;
		if (adapter->tx_usecs != value) {
			reset = 1;
			adapter->tx_usecs = value;
		}
	}

	if (ec->rx_max_coalesced_frames_irq) {

		value = clamp_t(u32,
						ec->rx_max_coalesced_frames_irq,
						RNPM_MIN_RX_WORK,
						RNPM_MAX_RX_WORK);
		value = ALIGN(value, RNPM_WORK_ALIGN);

		if (adapter->napi_budge != value) {
			reset = 1;
			adapter->napi_budge = value;
		}
	}

	if (ec->rx_max_coalesced_frames) {
		value = clamp_t(u32,
						ec->rx_max_coalesced_frames,
						RNPM_MIN_RX_FRAME,
						RNPM_MAX_RX_FRAME);
		if (adapter->rx_frames != value) {
			reset = 1;
			adapter->rx_frames = value;
		}
	}

	if (ec->rx_coalesce_usecs) {
		/* check vlaue */
		value = clamp_t(
			u32, ec->rx_coalesce_usecs, RNPM_MIN_RX_USEC, RNPM_MAX_RX_USEC);
		value = value;

		if (adapter->rx_usecs != value) {
			reset = 1;
			adapter->rx_usecs = value;
		}
	}

	/* other setup is not supported */
	if ((ec->pkt_rate_low) || (ec->pkt_rate_high) ||
		(ec->rx_coalesce_usecs_low) || (ec->rx_max_coalesced_frames_low) ||
		(ec->tx_coalesce_usecs_low) || (ec->tx_max_coalesced_frames_low) ||
		(ec->rx_coalesce_usecs_high) || (ec->rx_max_coalesced_frames_high) ||
		(ec->tx_coalesce_usecs_high) || (ec->tx_max_coalesced_frames_high) ||
		(ec->rate_sample_interval) || (ec->tx_coalesce_usecs_irq) ||
		(ec->rx_coalesce_usecs_irq))
		return -EINVAL;

	if (reset)
		return rnpm_setup_tc(netdev, netdev_get_num_tc(netdev));

	return 0;
}

/**
 * rnpm_get_ethtool_stats - copy stat values into supplied buffer
 * @netdev: the netdev to collect stats for
 * @stats: ethtool stats command structure
 * @data: ethtool supplied buffer
 *
 * Copy the stats values for this netdev into the buffer. Expects data to be
 * pre-allocated to the size returned by i40e_get_stats_count.. Note that all
 * statistics must be copied in a static order, and the count must not change
 * for a given netdev. See i40e_get_stats_count for more details.
 *
 * If a statistic is not currently valid (such as a disabled queue), this
 * function reports its value as zero.
 **/
static void rnpm_get_ethtool_stats(struct net_device *netdev,
								   struct ethtool_stats *stats,
								   u64 *data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;

	struct net_device_stats *net_stats = &netdev->stats;
	struct rnpm_ring *ring;
	int i, j;
	char *p = NULL;

	rnpm_update_stats(adapter);

	for (i = 0; i < RNPM_GLOBAL_STATS_LEN; i++) {
		p = (char *)net_stats + rnpm_gstrings_net_stats[i].stat_offset;
		data[i] = (rnpm_gstrings_net_stats[i].sizeof_stat == sizeof(u64))
					  ? *(u64 *)p
					  : *(u32 *)p;
	}

	for (j = 0; j < RNPM_HWSTRINGS_STATS_LEN; j++, i++) {
		p = (char *)adapter + rnpm_hwstrings_stats[j].stat_offset;
		data[i] = (rnpm_hwstrings_stats[j].sizeof_stat == sizeof(u64))
					  ? *(u64 *)p
					  : *(u32 *)p;
	}

	BUG_ON(RNPM_NUM_TX_QUEUES != RNPM_NUM_RX_QUEUES);

	for (j = 0; j < RNPM_NUM_TX_QUEUES; j++) {
		/* tx-ring */
		ring = adapter->tx_ring[j];

		if (!ring) {
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;

			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;

			continue;
		}

		data[i++] = ring->stats.packets;
		data[i++] = ring->stats.bytes;
		data[i++] = ring->tx_stats.restart_queue;
		data[i++] = ring->tx_stats.tx_busy;
		data[i++] = ring->tx_stats.tx_done_old;
		data[i++] = ring->tx_stats.clean_desc;
		data[i++] = ring->tx_stats.poll_count;
		data[i++] = ring->tx_stats.irq_more_count;
		/* rnpm_tx_queue_ring_stat */
		data[i++] =
			rd32(hw, RNPM_DMA_REG_TX_DESC_BUF_HEAD(ring->rnpm_queue_idx));
		data[i++] =
			rd32(hw, RNPM_DMA_REG_TX_DESC_BUF_TAIL(ring->rnpm_queue_idx));
		data[i++] = ring->next_to_clean;
		data[i++] = ring->next_to_use;
		data[i++] = ring->tx_stats.send_bytes;
		data[i++] = ring->tx_stats.send_bytes_to_hw;
		data[i++] = ring->tx_stats.todo_update;
		data[i++] = ring->tx_stats.send_done_bytes;
		data[i++] = ring->tx_stats.vlan_add;
		if (ring->tx_stats.tx_next_to_clean == -1)
			data[i++] = ring->count;
		else
			data[i++] = ring->tx_stats.tx_next_to_clean;
		data[i++] = ring->tx_stats.tx_irq_miss;
		data[i++] = ring->tx_stats.tx_equal_count;

		/* rx-ring */
		ring = adapter->rx_ring[j];

		if (!ring) {
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			continue;
		}
		data[i++] = ring->stats.packets;
		data[i++] = ring->stats.bytes;
		data[i++] = ring->rx_stats.driver_drop_packets;
		data[i++] = ring->rx_stats.rsc_count;
		data[i++] = ring->rx_stats.rsc_flush;
		data[i++] = ring->rx_stats.non_eop_descs;
		data[i++] = ring->rx_stats.alloc_rx_page_failed;
		data[i++] = ring->rx_stats.alloc_rx_buff_failed;
		data[i++] = ring->rx_stats.csum_err;
		data[i++] = ring->rx_stats.csum_good;
		data[i++] = ring->rx_stats.poll_again_count;
		data[i++] = ring->rx_stats.vlan_remove;
		data[i++] = ring->rx_stats.alloc_rx_page;
		/* rnpm_rx_queue_ring_stat */
		data[i++] =
			rd32(hw, RNPM_DMA_REG_RX_DESC_BUF_HEAD(ring->rnpm_queue_idx));
		data[i++] =
			rd32(hw, RNPM_DMA_REG_RX_DESC_BUF_TAIL(ring->rnpm_queue_idx));
		data[i++] = ring->next_to_use;
		data[i++] = ring->next_to_clean;
		if (ring->rx_stats.rx_next_to_clean == -1)
			data[i++] = ring->count;
		else
			data[i++] = ring->rx_stats.rx_next_to_clean;
		data[i++] = ring->rx_stats.rx_irq_miss;
		data[i++] = ring->rx_stats.rx_equal_count;
	}
}

#ifndef HAVE_NDO_SET_FEATURES
static u32 rnpm_get_rx_csum(struct net_device *netdev)
{
	return !!(netdev->features & NETIF_F_RXCSUM);
}

static int rnpm_set_rx_csum(struct net_device *netdev, u32 data)
{
	// struct rnpm_adapter *adapter = netdev_priv(netdev);
	// bool need_reset = false;

	if (data)
		netdev->features |= NETIF_F_RXCSUM;
	else
		netdev->features &= ~NETIF_F_RXCSUM;

	return 0;
}

__maybe_unused static u32 rnpm_get_tx_csum(struct net_device *netdev)
{
	return (netdev->features & NETIF_F_IP_CSUM) != 0;
}

static int rnpm_set_tx_csum(struct net_device *netdev, u32 data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
#ifdef NETIF_F_IPV6_CSUM
	u32 feature_list = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
#else
	u32 feature_list = NETIF_F_IP_CSUM;
#endif

	switch (adapter->hw.rss_type) {
		case rnpm_rss_n10:
#ifdef HAVE_ENCAP_TSO_OFFLOAD
			if (data)
				netdev->hw_enc_features |= NETIF_F_GSO_UDP_TUNNEL;
			else
				netdev->hw_enc_features &= ~NETIF_F_GSO_UDP_TUNNEL;
			feature_list |= NETIF_F_GSO_UDP_TUNNEL;
#endif /* HAVE_ENCAP_TSO_OFFLOAD */
			feature_list |= NETIF_F_SCTP_CSUM;
			break;
		default:
			break;
	}

	if (data)
		netdev->features |= feature_list;
	else
		netdev->features &= ~feature_list;

	return 0;
}

#ifdef NETIF_F_TSO
static int rnpm_set_tso(struct net_device *netdev, u32 data)
{
#ifdef NETIF_F_TSO6
	u32 feature_list = NETIF_F_TSO | NETIF_F_TSO6;
#else
	u32 feature_list = NETIF_F_TSO;
#endif

	if (data)
		netdev->features |= feature_list;
	else
		netdev->features &= ~feature_list;

#ifndef HAVE_NETDEV_VLAN_FEATURES
	if (!data) {
		struct rnpm_adapter *adapter = netdev_priv(netdev);
		struct net_device *v_netdev;
		int i;

		/* disable TSO on all VLANs if they're present */
		if (!adapter->vlgrp)
			goto tso_out;

		for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
			v_netdev = vlan_group_get_device(adapter->vlgrp, i);
			if (!v_netdev)
				continue;

			v_netdev->features &= ~feature_list;
			vlan_group_set_device(adapter->vlgrp, i, v_netdev);
		}
	}

tso_out:

#endif /* HAVE_NETDEV_VLAN_FEATURES */
	return 0;
}
#endif
#endif

#ifdef ETHTOOL_GRXRINGS
/**
 * rnpm_get_rss_hash_opts - Get RSS hash Input Set for each flow type
 * @pf: pointer to the physical function struct
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the flow is supported, else Invalid Input.
 **/
static int rnpm_get_rss_hash_opts(struct rnpm_adapter *adapter,
								  struct ethtool_rxnfc *cmd)
{
	cmd->data = 0;

	/* Report default options for RSS on rnpm */
	switch (cmd->flow_type) {
		case TCP_V4_FLOW:
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
			/* fallthrough */
			fallthrough;
		case UDP_V4_FLOW:
		case SCTP_V4_FLOW:
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
			/* fallthrough */
			fallthrough;
		case AH_ESP_V4_FLOW:
		case AH_V4_FLOW:
		case ESP_V4_FLOW:
		case IPV4_FLOW:
			cmd->data |= RXH_IP_SRC | RXH_IP_DST;
			break;
		case TCP_V6_FLOW:
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
			/* fallthrough */
			fallthrough;
		case UDP_V6_FLOW:
		case SCTP_V6_FLOW:
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
			/* fallthrough */
			fallthrough;
		case AH_ESP_V6_FLOW:
		case AH_V6_FLOW:
		case ESP_V6_FLOW:
		case IPV6_FLOW:
			cmd->data |= RXH_IP_SRC | RXH_IP_DST;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

__maybe_unused static void dump_fsp(struct ethtool_rx_flow_spec *fsp)
{
	int i;

	dbg(" fsp cookie is %llx\n", fsp->ring_cookie);
	switch (fsp->flow_type & ~FLOW_EXT) {
		case ETHER_FLOW:
			for (i = 0; i < ETH_ALEN; i++) {
				dbg("src 0x%02x\n", fsp->h_u.ether_spec.h_source[i]);
				dbg("src mask 0x%02x\n", fsp->m_u.ether_spec.h_source[i]);
			}
			for (i = 0; i < ETH_ALEN; i++) {
				dbg("dst 0x%02x\n", fsp->h_u.ether_spec.h_dest[i]);
				dbg("dst mask 0x%02x\n", fsp->m_u.ether_spec.h_dest[i]);
			}
			dbg("proto type is %x\n", fsp->h_u.ether_spec.h_proto);
			break;
		default:
			dbg("flow type is %x\n", fsp->flow_type);
			dbg("l2 prot is %x\n", fsp->h_u.ether_spec.h_proto);
			dbg("ip4 src ip is %x\n", fsp->h_u.tcp_ip4_spec.ip4src);
			dbg("ip4 src ip mask is %x\n", fsp->m_u.tcp_ip4_spec.ip4src);
			dbg("ip4 dst ip is %x\n", fsp->h_u.tcp_ip4_spec.ip4dst);
			dbg("ip4 dst ip mask is %x\n", fsp->m_u.tcp_ip4_spec.ip4dst);
			dbg("ip4 src port is %x\n", fsp->h_u.tcp_ip4_spec.psrc);
			dbg("ip4 src port mask is %x\n", fsp->m_u.tcp_ip4_spec.psrc);
			dbg("ip4 dst port is %x\n", fsp->h_u.tcp_ip4_spec.pdst);
			dbg("ip4 dst port mask is %x\n", fsp->m_u.tcp_ip4_spec.pdst);
			dbg("proto is %x\n", fsp->h_u.usr_ip4_spec.proto);
			break;
	}
}

static int rnpm_get_ethtool_fdir_entry(struct rnpm_adapter *adapter,
									   struct ethtool_rxnfc *cmd)
{
	// union rnpm_atr_input *mask = &adapter->fdir_mask;
	struct ethtool_rx_flow_spec *fsp = (struct ethtool_rx_flow_spec *)&cmd->fs;
	struct hlist_node *node2;
	struct rnpm_fdir_filter *rule = NULL;

	/* report total rule count */
	cmd->data = adapter->fdir_pballoc;

	hlist_for_each_entry_safe(
		rule, node2, &adapter->fdir_filter_list, fdir_node)
	{
		if (fsp->location <= rule->sw_idx)
			break;
	}

	if (!rule || fsp->location != rule->sw_idx)
		return -EINVAL;

	/* fill out the flow spec entry */

	/* set flow type field */
	switch (rule->filter.formatted.flow_type) {
		case RNPM_ATR_FLOW_TYPE_TCPV4:
			fsp->flow_type = TCP_V4_FLOW;
			break;
		case RNPM_ATR_FLOW_TYPE_UDPV4:
			fsp->flow_type = UDP_V4_FLOW;
			break;
		case RNPM_ATR_FLOW_TYPE_SCTPV4:
			fsp->flow_type = SCTP_V4_FLOW;
			break;
		case RNPM_ATR_FLOW_TYPE_IPV4:
			fsp->flow_type = IP_USER_FLOW;
			fsp->h_u.usr_ip4_spec.ip_ver = ETH_RX_NFC_IP4;
			if (adapter->fdir_mode == fdir_mode_tuple5) {
				fsp->h_u.usr_ip4_spec.proto =
					rule->filter.formatted.inner_mac[0];
				fsp->m_u.usr_ip4_spec.proto = 0xff;

			} else {
				fsp->h_u.usr_ip4_spec.proto =
					rule->filter.formatted.inner_mac[0] &
					rule->filter.formatted.inner_mac_mask[0];
				fsp->m_u.usr_ip4_spec.proto =
					rule->filter.formatted.inner_mac_mask[0];
			}
			break;
		case RNPM_ATR_FLOW_TYPE_ETHER:
			fsp->flow_type = ETHER_FLOW;
			/* support proto and mask only in this mode */
			fsp->h_u.ether_spec.h_proto = rule->filter.layer2_formate.proto;
			fsp->m_u.ether_spec.h_proto = 0xffff;
			break;
		default:
			return -EINVAL;
	}
	if (rule->filter.formatted.flow_type != RNPM_ATR_FLOW_TYPE_ETHER) {
		// fsp->h_ext.vlan_tci = rule->filter.formatted.vlan_id;
		// fsp->h_ext.vlan_etype = rule->filter.formatted.flex_bytes;
		// fsp->h_ext.data[1] = htonl(rule->filter.formatted.vm_pool);
		/* not support mask in tuple 5 mode */
		if (adapter->fdir_mode == fdir_mode_tuple5) {
			fsp->h_u.tcp_ip4_spec.psrc = rule->filter.formatted.src_port;
			fsp->h_u.tcp_ip4_spec.pdst = rule->filter.formatted.dst_port;
			fsp->h_u.tcp_ip4_spec.ip4src = rule->filter.formatted.src_ip[0];
			fsp->h_u.tcp_ip4_spec.ip4dst = rule->filter.formatted.dst_ip[0];
			fsp->m_u.tcp_ip4_spec.psrc = 0xffff;
			fsp->m_u.tcp_ip4_spec.pdst = 0xffff;
			fsp->m_u.tcp_ip4_spec.ip4src = 0xffffffff;
			fsp->m_u.tcp_ip4_spec.ip4dst = 0xffffffff;
			// fsp->m_ext.vlan_tci = mask->formatted.vlan_id;
			// fsp->m_ext.vlan_etype = mask->formatted.flex_bytes;
			// fsp->m_ext.data[1] = htonl(mask->formatted.vm_pool);
		} else {
			fsp->h_u.tcp_ip4_spec.psrc = rule->filter.formatted.src_port &
										 rule->filter.formatted.src_port_mask;
			fsp->m_u.tcp_ip4_spec.psrc = rule->filter.formatted.src_port_mask;
			fsp->h_u.tcp_ip4_spec.pdst = rule->filter.formatted.dst_port &
										 rule->filter.formatted.dst_port_mask;
			fsp->m_u.tcp_ip4_spec.pdst = rule->filter.formatted.dst_port_mask;
			fsp->h_u.tcp_ip4_spec.ip4src =
				rule->filter.formatted.src_ip[0] &
				rule->filter.formatted.src_ip_mask[0];
			fsp->m_u.tcp_ip4_spec.ip4src =
				rule->filter.formatted.src_ip_mask[0];
			fsp->h_u.tcp_ip4_spec.ip4dst =
				rule->filter.formatted.dst_ip[0] &
				rule->filter.formatted.dst_ip_mask[0];
			fsp->m_u.tcp_ip4_spec.ip4dst =
				rule->filter.formatted.dst_ip_mask[0];
		}
	}
	// dump_fsp(fsp);
	// fsp->flow_type |= FLOW_EXT;

	/* record action */
	if (rule->action == RNPM_FDIR_DROP_QUEUE)
		fsp->ring_cookie = RX_CLS_FLOW_DISC;
	else {
		if (rule->vf_num != 0) {
			fsp->ring_cookie = ((u64)rule->vf_num << 32) |
							   (rule->action % PF_RING_CNT_WHEN_IOV_ENABLED);
		} else {
			fsp->ring_cookie = rule->action;
		}
	}

	return 0;
}

static int rnpm_get_ethtool_fdir_all(struct rnpm_adapter *adapter,
									 struct ethtool_rxnfc *cmd,
									 u32 *rule_locs)
{
	struct hlist_node *node2;
	struct rnpm_fdir_filter *rule;
	int cnt = 0;

	/* report total rule count */
	cmd->data = adapter->fdir_pballoc;

	hlist_for_each_entry_safe(
		rule, node2, &adapter->fdir_filter_list, fdir_node)
	{
		if (cnt == cmd->rule_cnt)
			return -EMSGSIZE;
		rule_locs[cnt] = rule->sw_idx;
		cnt++;
	}

	cmd->rule_cnt = cnt;

	return 0;
}

/**
 * rnpm_get_rxnfc - command to get RX flow classification rules
 * @netdev: network interface device structure
 * @cmd: ethtool rxnfc command
 * @rule_locs: pointer to store rule data
 *
 * Returns Success if the command is supported.
 **/
static int rnpm_get_rxnfc(struct net_device *dev,
						  struct ethtool_rxnfc *cmd,
#ifdef HAVE_ETHTOOL_GET_RXNFC_VOID_RULE_LOCS
						  void *rule_locs)
#else
						  u32 *rule_locs)
#endif
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
		case ETHTOOL_GRXRINGS:
			if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED) {
				/* we fix 2 when srio on */
				cmd->data = 2;
			} else {
				cmd->data = adapter->num_rx_queues;
			}
			ret = 0;
			break;
		case ETHTOOL_GRXCLSRLCNT:
			cmd->rule_cnt = adapter->fdir_filter_count;
			ret = 0;
			break;
		case ETHTOOL_GRXCLSRULE:
			ret = rnpm_get_ethtool_fdir_entry(adapter, cmd);
			break;
		case ETHTOOL_GRXCLSRLALL:
#ifdef HAVE_ETHTOOL_GET_RXNFC_VOID_RULE_LOCS
			ret = rnpm_get_ethtool_fdir_all(adapter, cmd, (u32 *)rule_locs);
#else
			ret = rnpm_get_ethtool_fdir_all(adapter, cmd, rule_locs);
#endif
			break;
		case ETHTOOL_GRXFH:
			ret = rnpm_get_rss_hash_opts(adapter, cmd);
			break;
		default:
			break;
	}

	return ret;
}
#define UDP_RSS_FLAGS \
	(RNPM_FLAG2_RSS_FIELD_IPV4_UDP | RNPM_FLAG2_RSS_FIELD_IPV6_UDP)
static int rnpm_set_rss_hash_opt(struct rnpm_adapter *adapter,
								 struct ethtool_rxnfc *nfc)
{
	// u32 flags2 = adapter->flags2;

	/*
	 * RSS does not support anything other than hashing
	 * to queues on src and dst IPs and ports
	 */
	if (nfc->data & ~(RXH_IP_SRC | RXH_IP_DST | RXH_L4_B_0_1 | RXH_L4_B_2_3))
		return -EINVAL;

	switch (nfc->flow_type) {
		case TCP_V4_FLOW:
		case TCP_V6_FLOW:
		case UDP_V4_FLOW:
		case UDP_V6_FLOW:
			if (!(nfc->data & RXH_IP_SRC) || !(nfc->data & RXH_IP_DST) ||
				!(nfc->data & RXH_L4_B_0_1) || !(nfc->data & RXH_L4_B_2_3))
				return -EINVAL;
			break;
		case AH_ESP_V4_FLOW:
		case AH_V4_FLOW:
		case ESP_V4_FLOW:
		case SCTP_V4_FLOW:
		case AH_ESP_V6_FLOW:
		case AH_V6_FLOW:
		case ESP_V6_FLOW:
		case SCTP_V6_FLOW:
			if (!(nfc->data & RXH_IP_SRC) || !(nfc->data & RXH_IP_DST) ||
				(nfc->data & RXH_L4_B_0_1) || (nfc->data & RXH_L4_B_2_3))
				return -EINVAL;
			break;
		default:
			return -EINVAL;
	}

	/* if we changed something we need to update flags */
	/*
	if (flags2 != adapter->flags2) {
		struct rnpm_hw *hw = &adapter->hw;
		u32 mrqc = RNPM_READ_REG(hw, RNPM_MRQC);

		if ((flags2 & UDP_RSS_FLAGS) &&
			!(adapter->flags2 & UDP_RSS_FLAGS))
			e_warn(drv, "enabling UDP RSS: fragmented packets"
				   " may arrive out of order to the stack above\n");

		adapter->flags2 = flags2;

		mrqc |= RNPM_MRQC_RSS_FIELD_IPV4
			  | RNPM_MRQC_RSS_FIELD_IPV4_TCP
			  | RNPM_MRQC_RSS_FIELD_IPV6
			  | RNPM_MRQC_RSS_FIELD_IPV6_TCP;

		mrqc &= ~(RNPM_MRQC_RSS_FIELD_IPV4_UDP |
			  RNPM_MRQC_RSS_FIELD_IPV6_UDP);

		if (flags2 & RNPM_FLAG2_RSS_FIELD_IPV4_UDP)
			mrqc |= RNPM_MRQC_RSS_FIELD_IPV4_UDP;

		if (flags2 & RNPM_FLAG2_RSS_FIELD_IPV6_UDP)
			mrqc |= RNPM_MRQC_RSS_FIELD_IPV6_UDP;

		RNPM_WRITE_REG(hw, RNPM_MRQC, mrqc);
	}
	*/

	return 0;
}


#ifdef ETHTOOL_GFLAGS
__maybe_unused static int rnpm_set_flags(struct net_device *netdev, u32 data)
{
	return 0;
}
#endif /* ETHTOOL_GFLAGS */


static int rnpm_flowspec_to_flow_type(struct rnpm_adapter *adapter,
									  struct ethtool_rx_flow_spec *fsp,
									  uint8_t *flow_type,
									  struct rnpm_fdir_filter *input)
{
	int i;
	int ret = 1;
	/* not support flow_ext */
	if (fsp->flow_type & FLOW_EXT)
		return 0;

	switch (fsp->flow_type & ~FLOW_EXT) {
		case TCP_V4_FLOW:
			*flow_type = RNPM_ATR_FLOW_TYPE_TCPV4;
			break;
		case UDP_V4_FLOW:
			*flow_type = RNPM_ATR_FLOW_TYPE_UDPV4;
			break;
		case SCTP_V4_FLOW:
			*flow_type = RNPM_ATR_FLOW_TYPE_SCTPV4;
			break;
		case ETHER_FLOW:
			/* layer 2 flow */
			*flow_type = RNPM_ATR_FLOW_TYPE_ETHER;
			input->filter.layer2_formate.proto = fsp->h_u.ether_spec.h_proto;
			break;
		case IP_USER_FLOW:
			switch (fsp->h_u.usr_ip4_spec.proto) {
				case IPPROTO_TCP:
					*flow_type = RNPM_ATR_FLOW_TYPE_TCPV4;
					break;
				case IPPROTO_UDP:
					*flow_type = RNPM_ATR_FLOW_TYPE_UDPV4;
					break;
				case IPPROTO_SCTP:
					*flow_type = RNPM_ATR_FLOW_TYPE_SCTPV4;
					break;
				case 0:
					/* if only ip4 no src no dst*/
					if (!(fsp->h_u.tcp_ip4_spec.ip4src) &&
						(!(fsp->h_u.tcp_ip4_spec.ip4dst))) {
						/* if have no l4 proto, use layer2 */
						*flow_type = RNPM_ATR_FLOW_TYPE_ETHER;
						input->filter.layer2_formate.proto = htons(0x0800);
					} else {
						/* may only src or dst input */
						*flow_type = RNPM_ATR_FLOW_TYPE_IPV4;
					}
					break;
				default:
					/* other unknown l4 proto ip */
					*flow_type = RNPM_ATR_FLOW_TYPE_IPV4;
			}
			break;
		default:
			return 0;
	}
	/* layer2 flow */
	if (*flow_type == RNPM_ATR_FLOW_TYPE_ETHER) {
		if (adapter->layer2_count >= adapter->layer2_count_max) {
			e_err(drv, "layer2 count full\n");
			ret = 0;
		}
		/* should check dst mac filter */
		/* should check src dst all zeros */
		for (i = 0; i < ETH_ALEN; i++) {
			if ((fsp->h_u.ether_spec.h_source[i] != 0) ||
				(fsp->h_u.ether_spec.h_dest[i] != 0) ||
				(fsp->m_u.ether_spec.h_source[i] != 0) ||
				(fsp->m_u.ether_spec.h_dest[i] != 0))
				ret = 0;
		}
	} else if (*flow_type == RNPM_ATR_FLOW_TYPE_IPV4) {
		if (adapter->fdir_mode == fdir_mode_tuple5) {
			if (adapter->tuple_5_count >= adapter->tuple_5_count_max) {
				e_err(drv, "tuple 5 count full\n");
				ret = 0;
			}
			if ((fsp->h_u.usr_ip4_spec.ip4src != 0) &&
				(fsp->m_u.usr_ip4_spec.ip4src != 0xffffffff)) {
				e_err(drv, "ip src mask error\n");
				ret = 0;
			}
			if ((fsp->h_u.usr_ip4_spec.ip4dst != 0) &&
				(fsp->m_u.usr_ip4_spec.ip4dst != 0xffffffff)) {
				e_err(drv, "ip dst mask error\n");
				ret = 0;
			}
			if ((fsp->h_u.usr_ip4_spec.proto != 0) &&
				(fsp->m_u.usr_ip4_spec.proto != 0xff)) {
				e_err(drv, "ip l4 proto mask error\n");
				ret = 0;
			}
		} else {
			if (adapter->tuple_5_count >= adapter->tuple_5_count_max) {
				e_err(drv, "tcam count full\n");
				ret = 0;
			}
			/* tcam mode can support mask */
		}
		/* not support l4_4_bytes */
		if ((fsp->h_u.usr_ip4_spec.l4_4_bytes != 0)) {
			e_err(drv, "ip l4_4_bytes error\n");
			ret = 0;
		}
	} else {
		if (adapter->fdir_mode == fdir_mode_tuple5) {
			/* should check mask all ff */
			if (adapter->tuple_5_count >= adapter->tuple_5_count_max) {
				e_err(drv, "tuple 5 count full\n");
				ret = 0;
			}
			if ((fsp->h_u.tcp_ip4_spec.ip4src != 0) &&
				(fsp->m_u.tcp_ip4_spec.ip4src != 0xffffffff)) {
				e_err(drv, "src mask error\n");
				ret = 0;
			}
			if ((fsp->h_u.tcp_ip4_spec.ip4dst != 0) &&
				(fsp->m_u.tcp_ip4_spec.ip4dst != 0xffffffff)) {
				e_err(drv, "dst mask error\n");
				ret = 0;
			}
			if ((fsp->h_u.tcp_ip4_spec.psrc != 0) &&
				(fsp->m_u.tcp_ip4_spec.psrc != 0xffff)) {
				e_err(drv, "src port mask error\n");
				ret = 0;
			}
			if ((fsp->h_u.tcp_ip4_spec.pdst != 0) &&
				(fsp->m_u.tcp_ip4_spec.pdst != 0xffff)) {
				e_err(drv, "src port mask error\n");
				ret = 0;
			}
		} else {
			if (adapter->tuple_5_count >= adapter->tuple_5_count_max) {
				e_err(drv, "tcam count full\n");
				ret = 0;
			}
		}
		/* l4 tos is not supported */
		if (fsp->h_u.tcp_ip4_spec.tos != 0) {
			e_err(drv, "tos error\n");
			ret = 0;
		}
	}

	return ret;
}

int rnpm_update_ethtool_fdir_entry(struct rnpm_adapter *adapter,
								   struct rnpm_fdir_filter *input,
								   u16 sw_idx)
{
	struct rnpm_hw *hw = &adapter->hw;
	struct hlist_node *node2;
	struct rnpm_fdir_filter *rule, *parent;
	bool deleted = false;
	s32 err;

	parent = NULL;
	rule = NULL;

	hlist_for_each_entry_safe(
		rule, node2, &adapter->fdir_filter_list, fdir_node)
	{
		/* hash found, or no matching entry */
		if (rule->sw_idx >= sw_idx)
			break;
		parent = rule;
	}

	/* if there is an old rule occupying our place remove it */
	if (rule && (rule->sw_idx == sw_idx)) {
		/* only clear hw enable bits */
		/* hardware filters are only configured when interface is up,
		 * and we should not issue filter commands while the interface
		 * is down
		 */
		if (netif_running(adapter->netdev) && (!input)) {
			err = rnpm_fdir_erase_perfect_filter(
				adapter->fdir_mode, hw, &rule->filter, rule->hw_idx);
			if (err)
				return -EINVAL;
		}
		adapter->fdir_filter_count--;
		if (rule->filter.formatted.flow_type == RNPM_ATR_FLOW_TYPE_ETHER) {
			/* used to determine hw reg offset */
			adapter->layer2_count--;
		} else {
			adapter->tuple_5_count--;
		}
		hlist_del(&rule->fdir_node);
		kfree(rule);
		deleted = true;
	}

	/* If we weren't given an input, then this was a request to delete a
	 * filter. We should return -EINVAL if the filter wasn't found, but
	 * return 0 if the rule was successfully deleted.
	 */
	if (!input)
		return deleted ? 0 : -EINVAL;

	/* initialize node and set software index */
	INIT_HLIST_NODE(&input->fdir_node);

	/* add filter to the list */
	if (parent)
		hlist_add_behind(&input->fdir_node, &parent->fdir_node);
	else
		hlist_add_head(&input->fdir_node, &adapter->fdir_filter_list);

	/* update counts */
	adapter->fdir_filter_count++;
	if (input->filter.formatted.flow_type == RNPM_ATR_FLOW_TYPE_ETHER) {
		/* used to determine hw reg offset */
		adapter->layer2_count++;
	} else {
		adapter->tuple_5_count++;
	}
	return 0;
}

static int rnpm_add_ethtool_fdir_entry(struct rnpm_adapter *adapter,
									   struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp = (struct ethtool_rx_flow_spec *)&cmd->fs;
	struct rnpm_hw *hw = &adapter->hw;
	struct rnpm_fdir_filter *input;
    	unsigned long flags;
	/* we don't support mask */
	// union rnpm_atr_input mask;
	int err;

	if (!(adapter->flags & RNPM_FLAG_FDIR_PERFECT_CAPABLE))
		return -EOPNOTSUPP;

	/*
	 * Don't allow programming if the action is a queue greater than
	 * the number of online Rx queues.
	 */
	/* is sriov is on, allow vf and queue */
	/* vf should smaller than num_vfs */
	if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED) {
		if ((fsp->ring_cookie != RX_CLS_FLOW_DISC) &&
			((((fsp->ring_cookie & 0xff00000000) >> 32) > adapter->num_vfs) ||
			 ((fsp->ring_cookie & 0xffffffff) >= PF_RING_CNT_WHEN_IOV_ENABLED)))
			return -EINVAL;

	} else {
		if ((fsp->ring_cookie != RX_CLS_FLOW_DISC) &&
			(fsp->ring_cookie >= adapter->num_rx_queues))
			return -EINVAL;
	}

	/* Don't allow indexes to exist outside of available space */
	if (fsp->location >= (adapter->fdir_pballoc)) {
		e_err(drv, "Location out of range\n");
		return -EINVAL;
	}

	input = kzalloc(sizeof(*input), GFP_ATOMIC);
	if (!input)
		return -ENOMEM;

	// memset(&mask, 0, sizeof(union rnpm_atr_input));
	/* set SW index */
	input->sw_idx = fsp->location;

	/* record flow type */
	if (!rnpm_flowspec_to_flow_type(
			adapter, fsp, &input->filter.formatted.flow_type, input)) {
		e_err(drv, "Unrecognized flow type\n");
		goto err_out;
	}

	if (input->filter.formatted.flow_type == RNPM_ATR_FLOW_TYPE_ETHER) {
		/* used to determine hw reg offset */
		input->hw_idx = adapter->layer2_count + adapter->layer2_offset;
	} else if (input->filter.formatted.flow_type == RNPM_ATR_FLOW_TYPE_IPV4) {
		input->hw_idx = adapter->tuple_5_count + adapter->tuple_5_offset;
		/* Copy input into formatted structures */
		input->filter.formatted.src_ip[0] = fsp->h_u.usr_ip4_spec.ip4src;
		input->filter.formatted.src_ip_mask[0] = fsp->m_u.usr_ip4_spec.ip4src;
		input->filter.formatted.dst_ip[0] = fsp->h_u.usr_ip4_spec.ip4dst;
		input->filter.formatted.dst_ip_mask[0] = fsp->m_u.usr_ip4_spec.ip4dst;
		input->filter.formatted.src_port = 0;
		input->filter.formatted.src_port_mask = 0xffff;
		input->filter.formatted.dst_port = 0;
		input->filter.formatted.dst_port_mask = 0xffff;
		input->filter.formatted.inner_mac[0] = fsp->h_u.usr_ip4_spec.proto;
		input->filter.formatted.inner_mac_mask[0] = fsp->m_u.usr_ip4_spec.proto;
	} else { /* tcp or udp or sctp*/
		input->hw_idx = adapter->tuple_5_count + adapter->tuple_5_offset;
		/* Copy input into formatted structures */
		input->filter.formatted.src_ip[0] = fsp->h_u.tcp_ip4_spec.ip4src;
		input->filter.formatted.src_ip_mask[0] = fsp->m_u.usr_ip4_spec.ip4src;
		input->filter.formatted.dst_ip[0] = fsp->h_u.tcp_ip4_spec.ip4dst;
		input->filter.formatted.dst_ip_mask[0] = fsp->m_u.usr_ip4_spec.ip4dst;
		input->filter.formatted.src_port = fsp->h_u.tcp_ip4_spec.psrc;
		input->filter.formatted.src_port_mask = fsp->m_u.tcp_ip4_spec.psrc;
		input->filter.formatted.dst_port = fsp->h_u.tcp_ip4_spec.pdst;
		input->filter.formatted.dst_port_mask = fsp->m_u.tcp_ip4_spec.pdst;
	}
	/* determine if we need to drop or route the packet */
	if (fsp->ring_cookie == RX_CLS_FLOW_DISC)
		input->action = RNPM_FDIR_DROP_QUEUE;
	else {
		input->vf_num = (fsp->ring_cookie >> 32) & 0xff;
		if (input->vf_num) {
			/* in vf mode input->action is the real queue nums */
			input->action = 2 * (((fsp->ring_cookie >> 32) & 0xff) - 1) +
							(fsp->ring_cookie & 0xffffffff);
		} else
			input->action = fsp->ring_cookie;
	}
	spin_lock_irqsave(&adapter->fdir_perfect_lock, flags);
	/* only program filters to hardware if the net device is running, as
	 * we store the filters in the Rx buffer which is not allocated when
	 * the device is down
	 */
	if (netif_running(adapter->netdev)) {
		if (!input->vf_num) {
			err = rnpm_fdir_write_perfect_filter(
				adapter->fdir_mode,
				hw,
				&input->filter,
				input->hw_idx,
				(input->action == RNPM_FDIR_DROP_QUEUE)
					? RNPM_FDIR_DROP_QUEUE
					: adapter->rx_ring[input->action]->rnpm_queue_idx);
			/* to check */
		} else {
			err = rnpm_fdir_write_perfect_filter(
				adapter->fdir_mode,
				hw,
				&input->filter,
				input->hw_idx,
				(input->action == RNPM_FDIR_DROP_QUEUE) ? RNPM_FDIR_DROP_QUEUE
														: input->action);
		}
		if (err)
			goto err_out_w_lock;
	}

	rnpm_update_ethtool_fdir_entry(adapter, input, input->sw_idx);
	spin_unlock_irqrestore(&adapter->fdir_perfect_lock,flags);
	return 0;
err_out_w_lock:
	spin_unlock_irqrestore(&adapter->fdir_perfect_lock, flags);
err_out:
	kfree(input);
	return -EINVAL;
}

static int rnpm_del_ethtool_fdir_entry(struct rnpm_adapter *adapter,
									   struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp = (struct ethtool_rx_flow_spec *)&cmd->fs;
	int err;
    	unsigned long flags;

	spin_lock_irqsave(&adapter->fdir_perfect_lock, flags);
	err = rnpm_update_ethtool_fdir_entry(adapter, NULL, fsp->location);
	spin_unlock_irqrestore(&adapter->fdir_perfect_lock, flags);

	return err;
}


/**
 * rnpm_set_rxnfc - command to set RX flow classification rules
 * @dev: network interface device structure
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the command is supported.
 **/
static int rnpm_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
		case ETHTOOL_SRXCLSRLINS:
			ret = rnpm_add_ethtool_fdir_entry(adapter, cmd);
			break;
		case ETHTOOL_SRXCLSRLDEL:
			ret = rnpm_del_ethtool_fdir_entry(adapter, cmd);
			break;
		case ETHTOOL_SRXFH:
			ret = rnpm_set_rss_hash_opt(adapter, cmd);
			break;
		default:
			break;
	}

	return ret;
}

#endif /* ETHTOOL_GRXRINGS */

#ifdef ETHTOOL_SRXNTUPLE
/* We need to keep this around for kernels 2.6.33 - 2.6.39 in order to avoid
 * a null pointer dereference as it was assumend if the NETIF_F_NTUPLE flag
 * was defined that this function was present.
 */
static int rnpm_set_rx_ntuple(struct net_device __always_unused *dev,
							  struct ethtool_rx_ntuple __always_unused *cmd)
{
	return -EOPNOTSUPP;
}

#endif /* ETHTOOL_SRXNTUPLE */

#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)

static uint32_t rnpm_rss_indir_size(struct net_device *netdev)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);

	return rnpm_rss_indir_tbl_entries(adapter);
}

static u32 rnpm_get_rxfh_key_size(struct net_device *netdev)
{
	return RNPM_RSS_KEY_SIZE;
}

static void rnpm_get_reta(struct rnpm_adapter *adapter, u32 *indir)
{
	int i, reta_size = rnpm_rss_indir_tbl_entries(adapter);
	u16 rss_m = adapter->ring_feature[RING_F_RSS].mask;

	if (adapter->flags & RNPM_FLAG_SRIOV_ENABLED)
		rss_m = adapter->ring_feature[RING_F_RSS].indices - 1;

	for (i = 0; i < reta_size; i++) {
		if (adapter->flags & RNPM_FLAG_RXHASH_DISABLE)
			indir[i] = 0;
		else
			indir[i] = adapter->rss_indir_tbl[i] & rss_m;
	}
}

#ifdef HAVE_RXFH_HASHFUNC
static int
rnpm_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key, u8 *hfunc)
#else
static int rnpm_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key)
#endif
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;

#ifdef HAVE_RXFH_HASHFUNC
	if (hfunc)
		*hfunc = ETH_RSS_HASH_TOP;
#endif

	if (indir)
		rnpm_get_reta(adapter, indir);

	if (key)
		memcpy(key, pf_adapter->rss_key, rnpm_get_rxfh_key_size(netdev));

	return 0;
}

enum {
	PART_FW,
	PART_CFG,
	PART_MACSN,
	PART_PCSPHY,
	PART_PXE,
};

#ifdef HAVE_DDP_PROFILE_UPLOAD_SUPPORT
static int rnpm_flash_firmware(struct rnpm_adapter *adapter,
							   int region,
							   const u8 *data,
							   int bytes)
{
	struct rnpm_hw *hw = &adapter->hw;
	switch (region) {
		case PART_FW: {
			if (*((u32 *)(data + 28)) != 0xA51BBEAF) {
				return -EINVAL;
			}
			break;
		}
		case PART_CFG: {
			if (*((u32 *)(data)) != 0x00010cf9) {
				return -EINVAL;
			}
			break;
		}
		case PART_MACSN: {
			break;
		}
		case PART_PCSPHY: {
			if (*((u16 *)(data)) != 0x081d) {
				return -EINVAL;
			}
			break;
		}
		case PART_PXE: {
			if (*((u16 *)(data)) != 0xaa55) {
				return -EINVAL;
			}
			break;
		}
		default: {
			return -EINVAL;
		}
	}

	return rnpm_fw_update(hw, region, data, bytes);
}

static int rnpm_flash_firmware_from_file(struct net_device *dev,
										 struct rnpm_adapter *adapter,
										 int region,
										 const char *filename)
{
	const struct firmware *fw;
	int rc;

	rc = request_firmware(&fw, filename, &dev->dev);
	if (rc != 0) {
		netdev_err(
			dev, "Error %d requesting firmware file: %s\n", rc, filename);
		return rc;
	}

	rc = rnpm_flash_firmware(adapter, region, fw->data, fw->size);

	release_firmware(fw);
	return rc;
}

static int rnpm_flash_device(struct net_device *dev,
							 struct ethtool_flash *flash)
{
	struct rnpm_adapter *adapter = netdev_priv(dev);

	if (IS_VF(adapter->hw.pfvfnum)) {
		netdev_err(dev, "flashdev not supported from a virtual function\n");
		return -EINVAL;
	}

	return rnpm_flash_firmware_from_file(
		dev, adapter, flash->region, flash->data);
}
#endif
static int rnpm_rss_indir_tbl_max(struct rnpm_adapter *adapter)
{
	if (adapter->hw.rss_type == rnpm_rss_uv3p)
		return 8;
	else if (adapter->hw.rss_type == rnpm_rss_uv440)
		return 128;
	else if (adapter->hw.rss_type == rnpm_rss_n10)
		return 128;
	else
		return 128;
}

/**
 * rnpm_set_rxfh - set the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 * @key: hash key
 * @hfunc: hash function to use
 *
 * Returns -EINVAL if the table specifies an invalid queue id, otherwise
 * returns 0 after programming the table.
 **/
#ifdef HAVE_RXFH_HASHFUNC
static int rnpm_set_rxfh(struct net_device *netdev,
						 const u32 *indir,
						 const u8 *key,
						 const u8 hfunc)
#else
#ifdef HAVE_RXFH_NONCONST
static int rnpm_set_rxfh(struct net_device *netdev, u32 *indir, u8 *key)
#else
static int
rnpm_set_rxfh(struct net_device *netdev, const u32 *indir, const u8 *key)
#endif /* HAVE_RXFH_NONCONST */
#endif /* HAVE_RXFH_HASHFUNC */
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_pf_adapter *pf_adapter = adapter->pf_adapter;
	u16 i;
    	unsigned long flags;
	u32 reta_entries = rnpm_rss_indir_tbl_entries(adapter);

#ifdef HAVE_RXFH_HASHFUNC
	if (hfunc)
		return -EOPNOTSUPP;
#endif

	/* Verify user input. */
	if (indir) {
		int max_queues =
			min_t(int, adapter->num_rx_queues, rnpm_rss_indir_tbl_max(adapter));

		/* in this mode ,do not change rss table */
		if (adapter->flags & RNPM_FLAG_RXHASH_DISABLE)
			return -EINVAL;
		/*Allow at least 2 queues w/ SR-IOV.*/
		if ((adapter->flags & RNPM_FLAG_SRIOV_ENABLED) && (max_queues < 2))
			max_queues = 2;

		/* Verify user input. */
		for (i = 0; i < reta_entries; i++)
			if (indir[i] >= max_queues)
				return -EINVAL;

		/* store rss tbl */
		for (i = 0; i < reta_entries; i++)
			adapter->rss_indir_tbl[i] = indir[i];

		rnpm_store_reta(adapter);
	}

	/* Fill out the rss hash key */
	if (key) {
		/* not support key setup in multiports */
		if (adapter->flags & RNPM_FLAG_MUTIPORT_ENABLED)
			return -EINVAL;
		spin_lock_irqsave(&pf_adapter->key_setup_lock,flags);
		memcpy(pf_adapter->rss_key, key, rnpm_get_rxfh_key_size(netdev));
		rnpm_store_key(pf_adapter);
		spin_unlock_irqrestore(&pf_adapter->key_setup_lock,flags);
	}

	return 0;
}

#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */

#ifdef HAVE_ETHTOOL_PHY_STATISTICS
void rnpm_get_phy_statistics(struct net_device *netdev,
							 struct ethtool_stats *stats,
							 u64 *data)
{
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	struct rnpm_hw *hw = &adapter->hw;
	struct phy_statistics ps;

	if (rnpm_mbx_get_phy_statistics(hw, (u8 *)&ps) != 0) {
		return;
	}

	*data++ = ps.yt.pkg_ib_valid;
	*data++ = ps.yt.pkg_ib_os_good;
	*data++ = ps.yt.pkg_ib_us_good;
	*data++ = ps.yt.pkg_ib_err;
	*data++ = ps.yt.pkg_ib_os_bad;
	*data++ = ps.yt.pkg_ib_frag;
	*data++ = ps.yt.pkg_ib_nosfd;
	*data++ = ps.yt.pkg_ob_valid;
	*data++ = ps.yt.pkg_ob_os_good;
	*data++ = ps.yt.pkg_ob_us_good;
	*data++ = ps.yt.pkg_ob_err;
	*data++ = ps.yt.pkg_ob_os_bad;
	*data++ = ps.yt.pkg_ob_frag;
	*data++ = ps.yt.pkg_ob_nosfd;
}
#endif

static 
int rnpm_nway_reset(struct net_device *netdev)
{
	/* restart autonegotiation */
	struct rnpm_adapter *adapter = netdev_priv(netdev);
	netdev_info(netdev, "NIC Link is Down\n");
	rnpm_down(adapter);
	msleep(10);
	rnpm_up(adapter);
	return 0;
}
static const struct ethtool_ops rnpm_ethtool_ops = {
#ifndef ETHTOOL_GLINKSETTINGS
	.get_settings = rnpm_get_settings,
	.set_settings = rnpm_set_settings,
#else
	.get_link_ksettings = rnpm_get_link_ksettings,
	.set_link_ksettings = rnpm_set_link_ksettings,
#endif /* ETHTOOL_GLINKSETTINGS */
	.get_drvinfo = rnpm_get_drvinfo,

	.get_regs_len = rnpm_get_regs_len,
	.get_regs = rnpm_get_regs,
	.get_wol = rnpm_get_wol,
	.set_wol = rnpm_set_wol,
	.nway_reset = rnpm_nway_reset,
	.get_link = ethtool_op_get_link,
	//.set_eeprom		= rnpm_set_eeprom,
	//.get_eeprom_len	= rnpm_get_eeprom_len,
	//.get_eeprom		= rnpm_get_eeprom,
	.get_ringparam = rnpm_get_ringparam,
	.set_ringparam = rnpm_set_ringparam,
	.get_pauseparam = rnpm_get_pauseparam,
	.set_pauseparam = rnpm_set_pauseparam,
	.get_msglevel = rnpm_get_msglevel,
	.set_msglevel = rnpm_set_msglevel,

#ifdef ETHTOOL_GFECPARAM
	.get_fecparam = rnpm_get_fecparam,
	.set_fecparam = rnpm_set_fecparam,
#endif /* ETHTOOL_GFECPARAM */

#ifndef CLOST_SELF_TEST
#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
// .self_test_count = rnpm_diag_test_count,
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */
	.self_test = rnpm_diag_test,
#endif
	.get_strings = rnpm_get_strings,

#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef ETHTOOL_GEEE
//.get_eee		= rnpm_get_eee,
#endif /* ETHTOOL_GEEE */
#ifdef ETHTOOL_SEEE
//.set_eee		= rnpm_set_eee,
#endif /* ETHTOOL_SEEE */
#ifdef HAVE_ETHTOOL_SET_PHYS_ID
	.set_phys_id = rnpm_set_phys_id,
#else
//.phys_id		= rnpm_phys_id,
#endif /* HAVE_ETHTOOL_SET_PHYS_ID */
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
	.get_stats_count = rnpm_get_stats_count,
#else  /* HAVE_ETHTOOL_GET_SSET_COUNT */
	.get_sset_count = rnpm_get_sset_count,
	.get_priv_flags = rnpm_get_priv_flags,
	.set_priv_flags = rnpm_set_priv_flags,
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */
	.get_ethtool_stats = rnpm_get_ethtool_stats,
#ifdef HAVE_ETHTOOL_GET_PERM_ADDR
	.get_perm_addr = ethtool_op_get_perm_addr,
#endif
	.get_coalesce = rnpm_get_coalesce,
	.set_coalesce = rnpm_set_coalesce,

#ifdef ETHTOOL_COALESCE_USECS
	.supported_coalesce_params = ETHTOOL_COALESCE_USECS,
#endif

#if 0
#ifdef HAVE_ETHTOOL_COALESCE_PARAMS_SUPPORT //?
	.supported_coalesce_params =
		ETHTOOL_COALESCE_USECS | ETHTOOL_COALESCE_MAX_FRAMES_IRQ |
		ETHTOOL_COALESCE_USE_ADAPTIVE | ETHTOOL_COALESCE_RX_USECS_HIGH |
		ETHTOOL_COALESCE_TX_USECS_HIGH,
#endif
#endif

#ifndef HAVE_NDO_SET_FEATURES
	.get_rx_csum = rnpm_get_rx_csum,
	.set_rx_csum = rnpm_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = rnpm_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = ethtool_op_get_tso,
	.set_tso = rnpm_set_tso,
#endif

#ifdef ETHTOOL_GFLAGS
	.get_flags = ethtool_op_get_flags,
//.set_flags		= rnpm_set_flags,
#endif
#endif /* HAVE_NDO_SET_FEATURES */
#ifdef ETHTOOL_GRXRINGS
	.get_rxnfc = rnpm_get_rxnfc,
	.set_rxnfc = rnpm_set_rxnfc,
#ifdef ETHTOOL_SRXNTUPLE
	.set_rx_ntuple = rnpm_set_rx_ntuple,
#endif
#endif

#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef ETHTOOL_GEEE
//.get_eee                = ixgbe_get_eee,
#endif /* ETHTOOL_GEEE */
#ifdef ETHTOOL_SEEE
//.set_eee                = ixgbe_set_eee,
#endif /* ETHTOOL_SEEE */
#ifdef ETHTOOL_SCHANNELS
	.get_channels = rnpm_get_channels,
	.set_channels = rnpm_set_channels,
#endif

#ifdef ETHTOOL_GMODULEINFO
	.get_module_info = rnpm_get_module_info,
	.get_module_eeprom = rnpm_get_module_eeprom,
#endif
#ifdef HAVE_ETHTOOL_GET_TS_INFO
	.get_ts_info = rnpm_get_ts_info,
#endif /* HAVE_ETHTOOL_GET_TS_INFO */

#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
	.get_rxfh_indir_size = rnpm_rss_indir_size,
	.get_rxfh_key_size = rnpm_get_rxfh_key_size,
	.get_rxfh = rnpm_get_rxfh,
	.set_rxfh = rnpm_set_rxfh,
#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */
	.get_dump_flag = rnpm_get_dump_flag,
	.get_dump_data = rnpm_get_dump_data,
	.set_dump = rnpm_set_dump,

#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
#ifdef HAVE_DDP_PROFILE_UPLOAD_SUPPORT
	.flash_device = rnpm_flash_device,
#endif /* HAVE_DDP_PROFILE_UPLOAD_SUPPORT */

#ifdef HAVE_ETHTOOL_PHY_STATISTICS
	.get_ethtool_phy_stats = rnpm_get_phy_statistics,
#endif

#ifdef ETHTOOL_PERQUEUE
//.get_per_queue_coalesce	= rnpm_get_per_queue_coalesce,
//.set_per_queue_coalesce	= rnpm_set_per_queue_coalesce,
#endif /* ETHTOOL_PERQUEUE */

};

#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
static const struct ethtool_ops_ext rnpm_ethtool_ops_ext = {
	.size = sizeof(struct ethtool_ops_ext),
	.get_ts_info = rnpm_get_ts_info,
	.set_phys_id = rnpm_set_phys_id,
	.get_channels = rnpm_get_channels,
	.set_channels = rnpm_set_channels,
#ifdef ETHTOOL_GMODULEINFO
	.get_module_info = rnpm_get_module_info,
	.get_module_eeprom = rnpm_get_module_eeprom,
#endif

#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
	.get_rxfh_indir_size = rnpm_rss_indir_size,
	.get_rxfh_key_size = rnpm_get_rxfh_key_size,
	.get_rxfh = rnpm_get_rxfh,
	.set_rxfh = rnpm_set_rxfh,
#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */
#ifdef ETHTOOL_GEEE
//.get_eee		= rnpm_get_eee,
#endif /* ETHTOOL_GEEE */
#ifdef ETHTOOL_SEEE
//.set_eee		= rnpm_set_eee,
#endif /* ETHTOOL_SEEE */
};
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */

void rnpm_set_ethtool_ops(struct net_device *netdev)
{
#ifndef ETHTOOL_OPS_COMPAT
	netdev->ethtool_ops = &rnpm_ethtool_ops;
#else
	SET_ETHTOOL_OPS(netdev, &rnpm_ethtool_ops);
#endif
#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
	set_ethtool_ops_ext(netdev, &rnpm_ethtool_ops_ext);
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
}
#endif /* SIOCETHTOOL */
