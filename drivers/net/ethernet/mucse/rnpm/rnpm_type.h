#ifndef _RNPM_TYPE_H_
#define _RNPM_TYPE_H_

#include <linux/types.h>
#include <linux/mdio.h>
#include <linux/netdevice.h>

#if defined(CONFIG_MXGBEM_N10) && !defined(N10)
#define N10
#endif

#if defined(CONFIG_MXGBEM_FIX_MAC_PADDIN) && !defined(FIX_MAC_PADDIN)
#define FIX_MAC_PADDIN
#endif

#if defined(CONFIG_MXGBEM_MSIX_COUNT)
#define RNPM_N10M_MSIX_VECTORS CONFIG_MXGBEM_MSIX_COUNT
#endif

#include "rnpm_regs.h"
#include "rnpm_compat.h"

/* Device IDs */
#define PCI_VENDOR_ID_MUCSE 0x8848
#define RNPM_DEV_ID_N10_PF0  0x7001
#define RNPM_DEV_ID_N10_PF1  0x7002

#define RNPM_DEV_ID_N10_PF0_N  0x1000
#define RNPM_DEV_ID_N10_PF1_N  0x1001


/* Wake Up Control */
#define RNPM_WUC_PME_EN	0x00000002 /* PME Enable */
#define RNPM_WUC_PME_STATUS	0x00000004 /* PME Status */
#define RNPM_WUC_WKEN		0x00000010 /* Enable PE_WAKE_N pin assertion  */

/* Wake Up Filter Control */
#define RNPM_WUFC_LNKC	0x00000001 /* Link Status Change Wakeup Enable */
#define RNPM_WUFC_MAG	0x00000002 /* Magic Packet Wakeup Enable */
#define RNPM_WUFC_EX	0x00000004 /* Directed Exact Wakeup Enable */
#define RNPM_WUFC_MC	0x00000008 /* Directed Multicast Wakeup Enable */
#define RNPM_WUFC_BC	0x00000010 /* Broadcast Wakeup Enable */
#define RNPM_WUFC_ARP	0x00000020 /* ARP Request Packet Wakeup Enable */
#define RNPM_WUFC_IPV4	0x00000040 /* Directed IPv4 Packet Wakeup Enable */
#define RNPM_WUFC_IPV6	0x00000080 /* Directed IPv6 Packet Wakeup Enable */
#define RNPM_WUFC_MNG	0x00000100 /* Directed Mgmt Packet Wakeup Enable */

#define RNPM_WUFC_IGNORE_TCO	0x00008000 /* Ignore WakeOn TCO packets */
#define RNPM_WUFC_FLX0	0x00010000 /* Flexible Filter 0 Enable */
#define RNPM_WUFC_FLX1	0x00020000 /* Flexible Filter 1 Enable */
#define RNPM_WUFC_FLX2	0x00040000 /* Flexible Filter 2 Enable */
#define RNPM_WUFC_FLX3	0x00080000 /* Flexible Filter 3 Enable */
#define RNPM_WUFC_FLX4	0x00100000 /* Flexible Filter 4 Enable */
#define RNPM_WUFC_FLX5	0x00200000 /* Flexible Filter 5 Enable */
#define RNPM_WUFC_FLX_FILTERS		0x000F0000 /* Mask for 4 flex filters */
#define RNPM_WUFC_FLX_FILTERS_6	0x003F0000 /* Mask for 6 flex filters */
#define RNPM_WUFC_FLX_FILTERS_8	0x00FF0000 /* Mask for 8 flex filters */
#define RNPM_WUFC_FW_RST_WK	0x80000000 /* Ena wake on FW reset assertion */
/* Mask for Ext. flex filters */
#define RNPM_WUFC_EXT_FLX_FILTERS	0x00300000
#define RNPM_WUFC_ALL_FILTERS		0x000F00FF /* Mask all 4 flex filters */
#define RNPM_WUFC_ALL_FILTERS_6	0x003F00FF /* Mask all 6 flex filters */
#define RNPM_WUFC_ALL_FILTERS_8	0x00FF00FF /* Mask all 8 flex filters */
#define RNPM_WUFC_FLX_OFFSET	16 /* Offset to the Flexible Filters bits */

/* Wake Up Status */
#define RNPM_WUS_LNKC		RNPM_WUFC_LNKC
#define RNPM_WUS_MAG		RNPM_WUFC_MAG
#define RNPM_WUS_EX		RNPM_WUFC_EX
#define RNPM_WUS_MC		RNPM_WUFC_MC
#define RNPM_WUS_BC		RNPM_WUFC_BC
#define RNPM_WUS_ARP		RNPM_WUFC_ARP
#define RNPM_WUS_IPV4		RNPM_WUFC_IPV4
#define RNPM_WUS_IPV6		RNPM_WUFC_IPV6
#define RNPM_WUS_MNG		RNPM_WUFC_MNG
#define RNPM_WUS_FLX0		RNPM_WUFC_FLX0
#define RNPM_WUS_FLX1		RNPM_WUFC_FLX1
#define RNPM_WUS_FLX2		RNPM_WUFC_FLX2
#define RNPM_WUS_FLX3		RNPM_WUFC_FLX3
#define RNPM_WUS_FLX4		RNPM_WUFC_FLX4
#define RNPM_WUS_FLX5		RNPM_WUFC_FLX5
#define RNPM_WUS_FLX_FILTERS	RNPM_WUFC_FLX_FILTERS
#define RNPM_WUS_FW_RST_WK	RNPM_WUFC_FW_RST_WK
/* Proxy Status */
#define RNPM_PROXYS_EX		0x00000004 /* Exact packet received */
#define RNPM_PROXYS_ARP_DIR	0x00000020 /* ARP w/filter match received */
#define RNPM_PROXYS_NS		0x00000200 /* IPV6 NS received */
#define RNPM_PROXYS_NS_DIR	0x00000400 /* IPV6 NS w/DA match received */
#define RNPM_PROXYS_ARP	0x00000800 /* ARP request packet received */
#define RNPM_PROXYS_MLD	0x00001000 /* IPv6 MLD packet received */

/* Proxying Filter Control */
#define RNPM_PROXYFC_ENABLE	0x00000001 /* Port Proxying Enable */
#define RNPM_PROXYFC_EX	0x00000004 /* Directed Exact Proxy Enable */
#define RNPM_PROXYFC_ARP_DIR	0x00000020 /* Directed ARP Proxy Enable */
#define RNPM_PROXYFC_NS	0x00000200 /* IPv6 Neighbor Solicitation */
#define RNPM_PROXYFC_ARP	0x00000800 /* ARP Request Proxy Enable */
#define RNPM_PROXYFC_MLD	0x00000800 /* IPv6 MLD Proxy Enable */
#define RNPM_PROXYFC_NO_TCO	0x00008000 /* Ignore TCO packets */

#define RNPM_WUPL_LENGTH_MASK	0xFFFF





#define RNPM_MAX_TRAFFIC_CLASS        4
#define TSRN10_TX_DEFAULT_BURST 8

#ifndef TSRN10_RX_DEFAULT_LINE
#define TSRN10_RX_DEFAULT_LINE 64
#endif
#ifndef TSRN10_RX_DEFAULT_BURST
#define TSRN10_RX_DEFAULT_BURST 16
#endif
#define RNPM_TX_PKT_POLL_BUDGET		  128

#ifndef RNPM_RX_PKT_POLL_BUDGET
#define RNPM_RX_PKT_POLL_BUDGET  64
#endif

#ifndef RNPM_PKT_TIMEOUT
#define RNPM_PKT_TIMEOUT 30
#endif

#ifndef RNPM_PKT_TIMEOUT_TX
#define RNPM_PKT_TIMEOUT_TX 100
#endif

/* VF Device IDs */
#define RNPM_DEV_ID_N10_PF0_VF   0x8001
#define RNPM_DEV_ID_N10_PF1_VF   0x8002

#define RNPM_DEV_ID_N10_PF0_VF_N   0x1010
#define RNPM_DEV_ID_N10_PF1_VF_N  0x1011


/* Transmit Descriptor - Advanced */
struct rnpm_tx_desc {
	union {
		__le64 pkt_addr; /* Packet buffer address */
		struct {
			__le32 adr_lo;
			__le32 adr_hi;
		};
	};
	__le32 blen_mac_ip_len;
	__le32 vlan_cmd;
#define RNPM_TXD_FLAGS_VLAN_PRIO_MASK 0xe000
#define RNPM_TX_FLAGS_VLAN_PRIO_SHIFT 13
#define RNPM_TX_FLAGS_VLAN_CFI_SHIFT  12


#define RNPM_TXD_VLAN_VALID	(0x80000000)
#define RNPM_TXD_VLAN_CTRL_NOP	(0x00 << 13)
#define RNPM_TXD_VLAN_CTRL_RM_VLAN	(0x20000000)
#define RNPM_TXD_VLAN_CTRL_INSERT_VLAN	(0x40000000)

#define RNPM_TXD_L4_CSUM		(0x10000000) /* udp tcp sctp csum */
#define RNPM_TXD_IP_CSUM		(0x8000000)
#define RNPM_TXD_TUNNEL_VXLAN	(0x1000000)
#define RNPM_TXD_TUNNEL_NVGRE	(0x2000000)
#define RNPM_TXD_L4_TYPE_UDP	(0xc00000)
#define RNPM_TXD_L4_TYPE_TCP	(0x400000)
#define RNPM_TXD_L4_TYPE_SCTP	(0x800000)
#define RNPM_TXD_FLAG_IPv4		(0)
#define RNPM_TXD_FLAG_IPv6		(0x200000)
#define RNPM_TXD_FLAG_TSO		(0x100000)
#define RNPM_TXD_FLAG_PTP		(0x4000000)
#define RNPM_TXD_CMD_RS			(0x040000)
#define RNPM_TXD_CMD_INNER_VLAN (0x08000000)
#define RNPM_TXD_STAT_DD		(0x020000)
#define RNPM_TXD_CMD_EOP		(0x010000)
} __attribute__((packed));

struct rnpm_tx_ctx_desc {
	__le32 mss_len_vf_num;
	__le32 inner_vlan_tunnel_len;
#define VF_VEB_MARK (1 << 24) // bit 56
	__le32 resv;
	__le32 resv_cmd;
#define RNPM_TXD_FLAG_TO_RPU				(0x80000000)
#define RNPM_TXD_SMAC_CTRL_NOP				(0x00 << 12)
#define RNPM_TXD_SMAC_CTRL_REPLACE_MACADDR0 (0x02 << 12)
#define RNPM_TXD_SMAC_CTRL_REPLACE_MACADDR1 (0x06 << 12)
#define RNPM_TXD_CTX_VLAN_CTRL_NOP			(0x00 << 10)
#define RNPM_TXD_CTX_VLAN_CTRL_RM_VLAN		(0x01 << 10)
#define RNPM_TXD_CTX_VLAN_CTRL_INSERT_VLAN	(0x02 << 10)
#define RNPM_TXD_MTI_CRC_PAD_CTRL			(0x01000000)
#define RNPM_TXD_CTX_CTRL_DESC				(0x080000)
#define RNPM_TXD_CMD_RS						(0x040000)
#define RNPM_TXD_STAT_DD					(0x020000)
} __attribute__((packed));

/* Receive Descriptor - Advanced */
union rnpm_rx_desc {
	struct {
		union {
			__le64 pkt_addr; /* Packet buffer address */
			struct {
				__le32 addr_lo;
				__le32 addr_hi;
			};
		};
		__le64 resv_cmd;
#define RNPM_RXD_FLAG_RS (0)
	};

	struct {
		__le32 rss_hash;
		__le16 mark;
#define VEB_VF_PKG (1 << 15)
		__le16 rev1;
		__le16 len;
		__le16 padding_len;
		__le16 vlan;
		__le16 cmd;
#define RNPM_RXD_STAT_VLAN_VALID     (1 << 15)
#define RNPM_RXD_STAT_TUNNEL_NVGRE   (0x02 << 13)
#define RNPM_RXD_STAT_TUNNEL_VXLAN   (0x01 << 13)
#define RNPM_RXD_STAT_TUNNEL_MASK    (0x03 << 13)
#define RNPM_RXD_STAT_ERR_MASK       (0x1f << 8)
#define RNPM_RXD_STAT_SCTP_MASK      (0x04 << 8)
#define RNPM_RXD_STAT_L4_SCTP        (0x02 << 6)
#define RNPM_RXD_STAT_L4_TCP         (0x01 << 6)
#define RNPM_RXD_STAT_L4_UDP         (0x03 << 6)
#define RNPM_RXD_STAT_IPV6           (1 << 5)
#define RNPM_RXD_STAT_IPV4           (0 << 5)
#define RNPM_RXD_STAT_PTP            (1 << 4)
#define RNPM_RXD_STAT_DD             (1 << 1)
#define RNPM_RXD_STAT_EOP            (1 << 0)
	} wb;
} __attribute__((packed));

/* Host Interface Command Structures */
struct rnpm_hic_hdr {
	u8 cmd;
	u8 buf_len;
	union {
		u8 cmd_resv;
		u8 ret_status;
	} cmd_or_resp;
	u8 checksum;
};

struct rnpm_hic_drv_info {
	struct rnpm_hic_hdr hdr;
	u8 port_num;
	u8 ver_sub;
	u8 ver_build;
	u8 ver_min;
	u8 ver_maj;
	u8 pad; /* end spacing to ensure length is mult. of dword */
	u16 pad2; /* end spacing to ensure length is mult. of dword2 */
};


/* Context descriptors */
struct rnpm_adv_tx_context_desc {
	__le32 vlan_macip_lens;
	__le32 seqnum_seed;
	__le32 type_tucmd_mlhl;
	__le32 mss_l4len_idx;
};

/* RAH */
#define RNPM_RAH_VIND_MASK     0x003C0000
#define RNPM_RAH_VIND_SHIFT    18
#define RNPM_RAH_AV            0x80000000
#define RNPM_CLEAR_VMDQ_ALL    0xFFFFFFFF

/* Autonegotiation advertised speeds */
typedef u32 rnpm_autoneg_advertised;
/* Link speed */
typedef u32 rnpm_link_speed;
#define RNPM_LINK_SPEED_UNKNOWN	   0
#define RNPM_LINK_SPEED_10_FULL	   BIT(2)
#define RNPM_LINK_SPEED_100_FULL   BIT(3)
#define RNPM_LINK_SPEED_1GB_FULL   BIT(4)
#define RNPM_LINK_SPEED_10GB_FULL  BIT(5)
#define RNPM_LINK_SPEED_40GB_FULL  BIT(6)
#define RNPM_LINK_SPEED_25GB_FULL  BIT(7)
#define RNPM_LINK_SPEED_50GB_FULL  BIT(8)
#define RNPM_LINK_SPEED_100GB_FULL BIT(9)
#define RNPM_LINK_SPEED_10_HALF	   BIT(10)
#define RNPM_LINK_SPEED_100_HALF   BIT(11)
#define RNPM_LINK_SPEED_1GB_HALF   BIT(12)
#define RNPM_SFP_MODE_10G_LR	   BIT(13)
#define RNPM_SFP_MODE_10G_SR	   BIT(14)
#define RNPM_SFP_MODE_10G_LRM	   BIT(15)
#define RNPM_SFP_MODE_1G_T		   BIT(16)
#define RNPM_SFP_MODE_1G_KX		   BIT(17)
#define RNPM_SFP_MODE_1G_SX		   BIT(18)
#define RNPM_SFP_MODE_1G_LX		   BIT(19)
#define RNPM_SFP_MODE_40G_SR4	   BIT(20)
#define RNPM_SFP_MODE_40G_CR4	   BIT(21)
#define RNPM_SFP_MODE_40G_LR4	   BIT(22)
#define RNPM_SFP_MODE_1G_CX		   BIT(23)
#define RNPM_SFP_MODE_10G_BASE_T   BIT(24)
#define RNPM_SFP_MODE_FIBER_CHANNEL_SPEED BIT(25) // sfp-a0-10 != 0
#define RNPM_SFP_CONNECTOR_DAC			  BIT(26)
#define RNPM_SFP_TO_SGMII			BIT(27)

#define RNPM_MODULE_QSFP_MAX_LEN 640

/* PHY ID */
#define RNPM_YT8631_PHY_ID 0x4f51e91a

#define RNPM_LINK_SPEED_n10_AUTONEG                       \
	(RNPM_LINK_SPEED_10_FULL | RNPM_LINK_SPEED_100_FULL | \
	 RNPM_LINK_SPEED_1GB_FULL | RNPM_LINK_SPEED_10GB_FULL)

/* Flow Control Data Sheet defined values
 * Calculation and defines taken from 802.1bb Annex O
 */

enum rnpm_atr_flow_type {
	RNPM_ATR_FLOW_TYPE_IPV4   = 0x0,
	RNPM_ATR_FLOW_TYPE_UDPV4  = 0x1,
	RNPM_ATR_FLOW_TYPE_TCPV4  = 0x2,
	RNPM_ATR_FLOW_TYPE_SCTPV4 = 0x3,
	RNPM_ATR_FLOW_TYPE_IPV6   = 0x4,
	RNPM_ATR_FLOW_TYPE_UDPV6  = 0x5,
	RNPM_ATR_FLOW_TYPE_TCPV6  = 0x6,
	RNPM_ATR_FLOW_TYPE_SCTPV6 = 0x7,
	RNPM_ATR_FLOW_TYPE_TUNNELED_IPV4        = 0x10,
	RNPM_ATR_FLOW_TYPE_TUNNELED_UDPV4       = 0x11,
	RNPM_ATR_FLOW_TYPE_TUNNELED_TCPV4       = 0x12,
	RNPM_ATR_FLOW_TYPE_TUNNELED_SCTPV4      = 0x13,
	RNPM_ATR_FLOW_TYPE_TUNNELED_IPV6        = 0x14,
	RNPM_ATR_FLOW_TYPE_TUNNELED_UDPV6       = 0x15,
	RNPM_ATR_FLOW_TYPE_TUNNELED_TCPV6       = 0x16,
	RNPM_ATR_FLOW_TYPE_TUNNELED_SCTPV6      = 0x17,
	RNPM_ATR_FLOW_TYPE_ETHER            = 0x18,
	RNPM_ATR_FLOW_TYPE_USERDEF          = 0x19,
};

#define RNPM_FDIR_DROP_QUEUE (200)


enum {
	fdir_mode_tcam = 0,
	fdir_mode_tuple5,
};
/* Flow Director ATR input struct. */
union rnpm_atr_input {
	/*
	 * Byte layout in order, all values with MSB first:
	 *
	 * vm_pool      - 1 byte
	 * flow_type    - 1 byte
	 * vlan_id      - 2 bytes
	 * src_ip       - 16 bytes
	 * inner_mac    - 6 bytes
	 * cloud_mode   - 2 bytes
	 * tni_vni      - 4 bytes
	 * dst_ip       - 16 bytes
	 * src_port     - 2 bytes
	 * dst_port     - 2 bytes
	 * flex_bytes   - 2 bytes
	 * bkt_hash     - 2 bytes
	 */
	struct {
		u8 vm_pool;
		u8 flow_type;
		__be16 vlan_id;
		__be32 dst_ip[4];
		__be32 dst_ip_mask[4];
		__be32 src_ip[4];
		__be32 src_ip_mask[4];
		u8 inner_mac[6];
		u8 inner_mac_mask[6];
		__be16 tunnel_type;
		__be32 tni_vni;
		__be16 src_port;
		__be16 src_port_mask;
		__be16 dst_port;
		__be16 dst_port_mask;
		__be16 flex_bytes;
		__be16 bkt_hash;
	} formatted;
	struct {
		u8 vm_poll;
		u8 flow_type;
		u16 vlan_id;
		__be16 proto;
		__be16 resv;
		__be32 nouse[12];
	} layer2_formate;
	__be32 dword_stream[14];

};


/* BitTimes (BT) conversion */
#define RNPM_BT2KB(BT) ((BT + (8 * 1024 - 1)) / (8 * 1024))
#define RNPM_B2BT(BT) (BT * 8)

/* Calculate Delay to respond to PFC */
#define RNPM_PFC_D	672

/* Calculate Cable Delay */
#define RNPM_CABLE_DC	5556 /* Delay Copper */
#define RNPM_CABLE_DO	5000 /* Delay Optical */

/* Calculate Interface Delay X540 */
#define RNPM_PHY_DC	25600	/* Delay 10G BASET */
#define RNPM_MAC_DC	8192	/* Delay Copper XAUI interface */
#define RNPM_XAUI_DC	(2 * 2048) /* Delay Copper Phy */

#define RNPM_ID_X540	(RNPM_MAC_DC + RNPM_XAUI_DC + RNPM_PHY_DC)

/* Calculate Interface Delay 82598, n10 */
#define RNPM_PHY_D	12800
#define RNPM_MAC_D	4096
#define RNPM_XAUI_D	(2 * 1024)

/* PHY MDI STANDARD CONFIG */
#define RNPM_MDI_PHY_ID1_OFFSET                2
#define RNPM_MDI_PHY_ID2_OFFSET                3
#define RNPM_MDI_PHY_ID_MASK                   0xFFFFFC00U
#define RNPM_MDI_PHY_SPEED_SELECT1             0x0040
#define RNPM_MDI_PHY_DUPLEX                    0x0100
#define RNPM_MDI_PHY_RESTART_AN                0x0200
#define RNPM_MDI_PHY_ANE                       0x1000
#define RNPM_MDI_PHY_SPEED_SELECT0             0x2000
#define RNPM_MDI_PHY_RESET                     0x8000

#define NGBE_PHY_RST_WAIT_PERIOD               50

#define RNPM_ID	(RNPM_MAC_D + RNPM_XAUI_D + RNPM_PHY_D)

/* Calculate Delay incurred from higher layer */
#define RNPM_HD	6144

/* Calculate PCI Bus delay for low thresholds */
#define RNPM_PCI_DELAY	10000

/* Flow Director compressed ATR hash input struct */
union rnpm_atr_hash_dword {
	struct {
		u8 vm_pool;
		u8 flow_type;
		__be16 vlan_id;
	} formatted;
	__be32 ip;
	struct {
		__be16 src;
		__be16 dst;
	} port;
	__be16 flex_bytes;
	__be32 dword;
};

enum rnpm_eeprom_type {
	rnpm_eeprom_uninitialized = 0,
	rnpm_eeprom_spi,
	rnpm_flash,
	rnpm_eeprom_none /* No NVM support */
};

enum rnpm_mac_type {
	rnp_mac_unknown = -1,
	rnpm_mac_4lane_40G,
	rnpm_mac_1lane_10_1G,
	rnpm_mac_2lanes,
	rnpm_mac_4lanes,
	rnpm_num_macs,
};
enum rnpm_rss_type {
	rnpm_rss_uv440 = 0,
	rnpm_rss_uv3p,
	rnpm_rss_n10,
};
enum rnpm_phy_type {
	rnpm_phy_unknown = 0,
	rnpm_phy_none,
	rnpm_phy_sfp,
	rnpm_phy_sfp_unsupported,
	rnpm_phy_sfp_unknown,
	rnpm_phy_generic
};

enum rnpm_sfp_type {
	rnpm_sfp_type_da_cu = 0,
	rnpm_sfp_type_sr = 1,
	rnpm_sfp_type_lr = 2,
	rnpm_sfp_type_da_cu_core0 = 3,
	rnpm_sfp_type_da_cu_core1 = 4,
	rnpm_sfp_type_srlr_core0 = 5,
	rnpm_sfp_type_srlr_core1 = 6,
	rnpm_sfp_type_da_act_lmt_core0 = 7,
	rnpm_sfp_type_da_act_lmt_core1 = 8,
	rnpm_sfp_type_1g_cu_core0 = 9,
	rnpm_sfp_type_1g_cu_core1 = 10,
	rnpm_sfp_type_1g_sx_core0 = 11,
	rnpm_sfp_type_1g_sx_core1 = 12,
	rnpm_sfp_type_1g_lx_core0 = 13,
	rnpm_sfp_type_1g_lx_core1 = 14,
	rnpm_sfp_type_not_present = 0xFFFE,
	rnpm_sfp_type_unknown = 0xFFFF
};

enum rnpm_media_type {
	rnpm_media_type_unknown = 0,
	rnpm_media_type_fiber,
	rnpm_media_type_copper,
	rnpm_media_type_backplane,
	rnpm_media_type_cx4,
	rnpm_media_type_da,
	rnpm_media_type_virtual
};

/* Flow Control Settings */
enum rnpm_fc_mode {
	rnpm_fc_none = 0,
	rnpm_fc_rx_pause,
	rnpm_fc_tx_pause,
	rnpm_fc_full,
	rnpm_fc_default
};

struct rnpm_addr_filter_info {
	u32 num_mc_addrs;
	u32 rar_used_count;
	u32 mta_in_use;
	u32 overflow_promisc;
	bool uc_set_promisc;
	bool user_set_promisc;
};

/* Bus parameters */
struct rnpm_bus_info {
	u16 func;
	u16 lan_id;
};

/* Flow control parameters */
struct rnpm_fc_info {
	u32 high_water[RNPM_MAX_TRAFFIC_CLASS]; /* Flow Control High-water */
	u32 low_water[RNPM_MAX_TRAFFIC_CLASS]; /* Flow Control Low-water */
	u16 pause_time; /* Flow Control Pause timer */
	bool send_xon; /* Flow control send XON */
	bool strict_ieee; /* Strict IEEE mode */
	bool disable_fc_autoneg; /* Do not autonegotiate FC */
	bool fc_was_autonegged; /* Is current_mode the result of autonegging? */
	enum rnpm_fc_mode current_mode; /* FC mode in effect */
	enum rnpm_fc_mode requested_mode; /* FC mode requested by caller */
};

/* Statistics counters collected by the MAC */
struct rnpm_hw_stats {
	u64 dma_to_eth;
	u64 dma_to_switch;
	u64 mac_to_mac;
	u64 switch_to_switch;
	u64 mac_to_dma;
	u64 switch_to_dma;
	u64 vlan_add_cnt;
	u64 vlan_strip_cnt;
	//=== error
	u64 invalid_droped_packets;
	u64 filter_dropped_packets;
	//== drop ==
	u64 host_l2_match_drop;
	u64 redir_input_match_drop;
	u64 redir_etype_match_drop;
	u64 redir_tcp_syn_match_drop;
	u64 redir_tuple5_match_drop;
	u64 redir_tcam_match_drop;

	u64 bmc_dropped_packets;
	u64 switch_dropped_packets;
	//=== rx
	u64 dma_to_host;
	//=== dma-tx ==
	u64 port0_tx_packets;
	u64 port1_tx_packets;
	u64 port2_tx_packets;
	u64 port3_tx_packets;
	//=== emac 1to4 tx ==
	u64 in0_tx_pkts;
	u64 in1_tx_pkts;
	u64 in2_tx_pkts;
	u64 in3_tx_pkts;
	//=== phy tx ==
	u64 port0_to_phy_pkts;
	u64 port1_to_phy_pkts;
	u64 port2_to_phy_pkts;
	u64 port3_to_phy_pkts;
	//=== mac rx ===
	u64 mac_rx_broadcast;
	u64 mac_rx_multicast;
};

/* forward declaration */
struct rnpm_hw;

/* iterator type for walking multicast address lists */
typedef u8* (*rnpm_mc_addr_itr) (struct rnpm_hw *hw, u8 **mc_addr_ptr, u32 *vmdq);

/* Function pointer table */
struct rnpm_eeprom_operations {
	s32 (*init_params)(struct rnpm_hw *);
	s32 (*read)(struct rnpm_hw *, u16, u16 *);
	s32 (*read_buffer)(struct rnpm_hw *, u16, u16, u16 *);
	s32 (*write)(struct rnpm_hw *, u16, u16);
	s32 (*write_buffer)(struct rnpm_hw *, u16, u16, u16 *);
	s32 (*validate_checksum)(struct rnpm_hw *, u16 *);
	s32 (*update_checksum)(struct rnpm_hw *);
	u16 (*calc_checksum)(struct rnpm_hw *);
};

struct rnpm_mac_operations {
	s32 (*init_hw)(struct rnpm_hw *);
	s32 (*reset_hw)(struct rnpm_hw *);
	s32 (*start_hw)(struct rnpm_hw *);
	s32 (*clear_hw_cntrs)(struct rnpm_hw *);
	enum rnpm_media_type (*get_media_type)(struct rnpm_hw *);
	u32 (*get_supported_physical_layer)(struct rnpm_hw *);
	s32 (*get_mac_addr)(struct rnpm_hw *, u8 *);
	s32 (*get_device_caps)(struct rnpm_hw *, u16 *);
	s32 (*get_wwn_prefix)(struct rnpm_hw *, u16 *, u16 *);
	s32 (*stop_adapter)(struct rnpm_hw *);
	s32 (*get_bus_info)(struct rnpm_hw *);
	void (*set_lan_id)(struct rnpm_hw *);
	s32 (*read_analog_reg8)(struct rnpm_hw *, u32, u8 *);
	s32 (*write_analog_reg8)(struct rnpm_hw *, u32, u8);
	s32 (*setup_sfp)(struct rnpm_hw *);
	s32 (*disable_rx_buff)(struct rnpm_hw *);
	s32 (*enable_rx_buff)(struct rnpm_hw *);
	s32 (*enable_rx_dma)(struct rnpm_hw *, u32);
	s32 (*acquire_swfw_sync)(struct rnpm_hw *, u16);
	void (*release_swfw_sync)(struct rnpm_hw *, u16);

	/* Link */
	void (*disable_tx_laser)(struct rnpm_hw *);
	void (*enable_tx_laser)(struct rnpm_hw *);
	void (*flap_tx_laser)(struct rnpm_hw *);
	s32 (*setup_link)(struct rnpm_hw *, rnpm_link_speed, bool);
	s32 (*check_link)(struct rnpm_hw *, rnpm_link_speed *, bool *, bool);
	s32 (*get_link_capabilities)(struct rnpm_hw *, rnpm_link_speed *,
			bool *, u32 *);

	/* Packet Buffer Manipulation */
	void (*set_rxpba)(struct rnpm_hw *, int, u32, int);

	/* LED */
	s32 (*led_on)(struct rnpm_hw *, u32);
	s32 (*led_off)(struct rnpm_hw *, u32);
	s32 (*blink_led_start)(struct rnpm_hw *, u32);
	s32 (*blink_led_stop)(struct rnpm_hw *, u32);

	/* RAR, Multicast, VLAN */
	s32 (*set_rar)(struct rnpm_hw *, u32, u8 *, u32, u32);
	s32 (*set_rar_mac)(struct rnpm_hw *, u32, u8 *, u32, u32);
	s32 (*clear_rar)(struct rnpm_hw *, u32);
	s32 (*clear_rar_mac)(struct rnpm_hw *, u32, u32);
	s32 (*set_vmdq)(struct rnpm_hw *, u32, u32);
	s32 (*clear_vmdq)(struct rnpm_hw *, u32, u32);
	s32 (*init_rx_addrs)(struct rnpm_hw *);
	s32 (*update_mc_addr_list)(struct rnpm_hw *, struct net_device *);
	s32 (*enable_mc)(struct rnpm_hw *);
	s32 (*disable_mc)(struct rnpm_hw *);
	s32 (*clear_vfta)(struct rnpm_hw *);
	s32 (*set_vfta)(struct rnpm_hw *, u32, u32, bool);
	s32 (*set_vfta_mac)(struct rnpm_hw *, u32, u32, bool);
	s32 (*init_uta_tables)(struct rnpm_hw *);
	void (*set_mac_anti_spoofing)(struct rnpm_hw *, bool, int);
	void (*set_vlan_anti_spoofing)(struct rnpm_hw *, bool, int);

	/* Flow Control */
	s32 (*fc_enable)(struct rnpm_hw *);

	/* Manageability interface */
	s32 (*set_fw_drv_ver)(struct rnpm_hw *, u8, u8, u8, u8);
	s32 (*get_thermal_sensor_data)(struct rnpm_hw *);
	s32 (*init_thermal_sensor_thresh)(struct rnpm_hw *hw);
	bool (*mng_fw_enabled)(struct rnpm_hw *hw);
};

struct rnpm_phy_operations {
	s32 (*identify)(struct rnpm_hw *);
	s32 (*identify_sfp)(struct rnpm_hw *);
	s32 (*init)(struct rnpm_hw *);
	s32 (*reset)(struct rnpm_hw *);
	s32 (*read_reg)(struct rnpm_hw *, u32, u32, u16 *);
	s32 (*write_reg)(struct rnpm_hw *, u32, u32, u16);
	s32 (*setup_link)(struct rnpm_hw *);
	s32 (*setup_link_speed)(struct rnpm_hw *, rnpm_link_speed, bool);
	s32 (*check_link)(struct rnpm_hw *, rnpm_link_speed *, bool *);
	s32 (*get_firmware_version)(struct rnpm_hw *, u16 *);
	s32 (*read_i2c_byte)(struct rnpm_hw *, u8, u8, u8 *);
	s32 (*write_i2c_byte)(struct rnpm_hw *, u8, u8, u8);
	s32 (*read_i2c_sff8472)(struct rnpm_hw *, u8, u8 *);
	s32 (*read_i2c_eeprom)(struct rnpm_hw *, u8, u8 *);
	s32 (*write_i2c_eeprom)(struct rnpm_hw *, u8, u8);
	s32 (*check_overtemp)(struct rnpm_hw *);
};

struct rnpm_eeprom_info {
	struct rnpm_eeprom_operations  ops;
	enum rnpm_eeprom_type          type;
	u32                             semaphore_delay;
	u16                             word_size;
	u16                             address_bits;
	u16                             word_page_size;
};

#define RNPM_FLAGS_DOUBLE_RESET_REQUIRED	0x01
#define RNPM_FLAGS_INIT_MAC_ADDRESS      0x02

enum mc_filter_type{
	rnpm_mc_filter_type0, /* nic hash table mode 0 */
	rnpm_mc_filter_type1, /* nic hash table mode 1 */
	rnpm_mc_filter_type2, /* nic hash table mode 2 */
	rnpm_mc_filter_type3, /* nic hash table mode 3 */
	rnpm_mc_filter_type4, /* mac hash table mode */
};

enum mc_location_type {
	rnpm_mc_location_nic,
	rnpm_mc_location_mac,
};
enum vlan_location_type {
	rnpm_vlan_location_nic,
	rnpm_vlan_location_mac,
};
struct rnpm_mac_info {
	struct rnpm_mac_operations     ops;
	// enum rnpm_mac_type             type;
	u8              addr[ETH_ALEN];
	u8              perm_addr[ETH_ALEN];
	/* prefix for World Wide Node Name (WWNN) */
	u16             wwnn_prefix;
	/* prefix for World Wide Port Name (WWPN) */
	u16             wwpn_prefix;
	u16				max_msix_vectors;
#define RNPM_MAX_MTA			128
	u32				mta_shadow[RNPM_MAX_MTA];
	s32             mc_filter_type;
	u32 		mc_location;
	u32             mcft_size;
	u32             vft_size;
	u32             vlan_location;
	u32             num_rar_entries;
	u32             rar_highwater;
	u32		rx_pb_size;
	u32             max_tx_queues;
	u32             max_rx_queues;
	u32             reg_off;
	u32             orig_autoc;
	u32             cached_autoc;
	u32             orig_autoc2;
	bool            orig_link_settings_stored;
	bool            autotry_restart;
	u8              mac_flags;
	bool            autoneg;
	bool duplex;
};

struct rnpm_phy_info {
	struct rnpm_phy_operations     ops;
	struct mdio_if_info		mdio;
	enum rnpm_phy_type             type;
	u32                             id;
	u32                            phy_addr;
	enum rnpm_sfp_type             sfp_type;
	bool                            sfp_setup_needed;
	u32                             revision;
	enum rnpm_media_type           media_type;
	bool                            reset_disable;
	rnpm_autoneg_advertised        autoneg_advertised;
	bool                            smart_speed_active;
	bool                            multispeed_fiber;
	bool                            reset_if_overtemp;
};

#include "rnpm_mbx.h"

struct rnpm_pcs_operations {
        u32 (*read)(struct rnpm_hw *hw, int num, u32 addr);
        void (*write)(struct rnpm_hw *hw, int num, u32 addr, u32 value);
};

struct rnpm_mbx_operations {
	s32 (*init_params)(struct rnpm_hw *hw);
	s32 (*read)(struct rnpm_hw *, u32 *, u16,  enum MBX_ID);
	s32 (*write)(struct rnpm_hw *, u32 *, u16, enum MBX_ID);
	s32 (*read_posted)(struct rnpm_hw *, u32 *, u16,  enum MBX_ID);
	s32 (*write_posted)(struct rnpm_hw *, u32 *, u16, enum MBX_ID);
	s32 (*check_for_msg)(struct rnpm_hw *, enum MBX_ID);
	s32 (*check_for_ack)(struct rnpm_hw *, enum MBX_ID);
//	s32 (*check_for_rst)(struct rnpm_hw *, enum MBX_ID);
	s32 (*configure)(struct rnpm_hw *hw, int nr_vec, bool enable);
};

struct rnpm_mbx_stats {
	u32 msgs_tx;
	u32 msgs_rx;

	u32 acks;
	u32 reqs;
	u32 rsts;
};

struct rnpm_pcs_info {
        struct rnpm_pcs_operations ops;
        int pcs_count;
};

struct rnpm_err_pkts_init_info {
	u64 wdt[4];
	u64 code[4];
	u64 crc[4];
	u64 slen[4];
	u64 glen[4];
	u64 iph[4];
	u64 len[4];
	u64 cut[4];
};

struct rnpm_mbx_info {
	struct rnpm_mbx_operations ops;
	struct rnpm_mbx_stats stats;
	u32 timeout;
	u32 usec_delay;
	u32 v2p_mailbox;
	u16 size;

	u16 vf_req[64];
	u16 vf_ack[64];
	u16 cpu_req;
	u16 cpu_ack;

	void *reply_dma;
	dma_addr_t reply_dma_phy;
	int reply_dma_size;

	struct mutex *lock;

	bool irq_enabled;
};

struct rnpm_hw {
	void *back;
	u8 __iomem *hw_addr;
	u8 __iomem *ring_msix_base;

	u8 pfvfnum; // fun
	u8 num;
	u8 nr_lane;
	int speed;
	u8 link; // up/down

	u8 pci_gen;
	u8 pci_lanes;
	struct pci_dev *pdev;

	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	char lane_mask;
	// u16 mac_type;
	u16 phy_type;

	u32 fw_version;
	u32 axi_mhz;
	u32 fw_uid;
	union {
		u8 port_id[4];
		u32 port_ids;
	};
	u8 is_backplane		   : 1;
	u8 is_sgmii			   : 1;
	u8 duplex			   : 1;
	u32 supported_link;

	u32 dma_version;
	enum rnpm_rss_type rss_type;
	struct rnpm_mac_info mac;
	struct rnpm_addr_filter_info addr_ctrl;
	struct rnpm_fc_info fc;
	struct rnpm_phy_info phy;
	struct rnpm_eeprom_info eeprom;
	struct rnpm_bus_info bus;
	struct rnpm_mbx_info mbx;
	struct rnpm_pcs_info pcs;
	struct rnpm_err_pkts_init_info err_pkts_init;
	bool adapter_stopped;
	bool force_full_reset;
	bool mng_fw_enabled;
	bool wol_enabled;

	int mode;
	int default_rx_queue;
	int usecstocount;

#define RNPM_NET_FEATURE_SG				  (u32)(1 << 0)
#define RNPM_NET_FEATURE_TX_CHECKSUM	  (u32)(1 << 1)
#define RNPM_NET_FEATURE_RX_CHECKSUM	  (u32)(1 << 2)
#define RNPM_NET_FEATURE_TSO			  (u32)(1 << 3)
#define RNPM_NET_FEATURE_TX_UDP_TUNNEL	  (1 << 4)
#define RNPM_NET_FEATURE_VLAN_FILTER	  (1 << 5)
#define RNPM_NET_FEATURE_VLAN_OFFLOAD	  (1 << 6)
#define RNPM_NET_FEATURE_RX_NTUPLE_FILTER (1 << 7)
#define RNPM_NET_FEATURE_TCAM			  (1 << 8)
#define RNPM_NET_FEATURE_RX_HASH		  (1 << 9)
#define RNPM_NET_FEATURE_RX_FCS			  (1 << 10)
	u32 feature_flags;

	struct {
		int version;
		int len;
		int flag;
	} dump;
};

struct rnpm_info {
	// enum rnpm_mac_type		mac;
	enum rnpm_rss_type               rss_type;
	s32				(*get_invariants)(struct rnpm_hw *);
	struct rnpm_mac_operations	*mac_ops;
	struct rnpm_eeprom_operations	*eeprom_ops;
	struct rnpm_phy_operations	*phy_ops;
	struct rnpm_mbx_operations	*mbx_ops;
	struct rnpm_pcs_operations *pcs_ops;

	bool one_pf_with_two_dma;
	int reg_off;
	int adapter_cnt;
	int hi_dma;
	int total_queue_pair_cnts;
	int queue_depth;
	int total_msix_table;
	int total_layer2_count;
	int total_tuple5_count;

	int dma2_in_1pf;
	char *hw_addr;
	struct {
		u16 tx_work_limit;
		u32 rx_usecs;
		u32 rx_frames;
		u32 tx_usecs;
		u32 tx_frames;
	} coalesce;
};


/* Error Codes */
#define RNPM_ERR_EEPROM                        -1
#define RNPM_ERR_EEPROM_CHECKSUM               -2
#define RNPM_ERR_PHY                           -3
#define RNPM_ERR_CONFIG                        -4
#define RNPM_ERR_PARAM                         -5
#define RNPM_ERR_MAC_TYPE                      -6
#define RNPM_ERR_UNKNOWN_PHY                   -7
#define RNPM_ERR_LINK_SETUP                    -8
#define RNPM_ERR_ADAPTER_STOPPED               -9
#define RNPM_ERR_INVALID_MAC_ADDR              -10
#define RNPM_ERR_DEVICE_NOT_SUPPORTED          -11
#define RNPM_ERR_MASTER_REQUESTS_PENDING       -12
#define RNPM_ERR_INVALID_LINK_SETTINGS         -13
#define RNPM_ERR_AUTONEG_NOT_COMPLETE          -14
#define RNPM_ERR_RESET_FAILED                  -15
#define RNPM_ERR_SWFW_SYNC                     -16
#define RNPM_ERR_PHY_ADDR_INVALID              -17
#define RNPM_ERR_I2C                           -18
#define RNPM_ERR_SFP_NOT_SUPPORTED             -19
#define RNPM_ERR_SFP_NOT_PRESENT               -20
#define RNPM_ERR_SFP_NO_INIT_SEQ_PRESENT       -21
#define RNPM_ERR_FDIR_REINIT_FAILED            -23
#define RNPM_ERR_EEPROM_VERSION                -24
#define RNPM_ERR_NO_SPACE                      -25
#define RNPM_ERR_OVERTEMP                      -26
#define RNPM_ERR_FC_NOT_NEGOTIATED             -27
#define RNPM_ERR_FC_NOT_SUPPORTED              -28
#define RNPM_ERR_SFP_SETUP_NOT_COMPLETE        -30
#define RNPM_ERR_PBA_SECTION                   -31
#define RNPM_ERR_INVALID_ARGUMENT              -32
#define RNPM_ERR_HOST_INTERFACE_COMMAND        -33
#define RNPM_NOT_IMPLEMENTED                   0x7FFFFFFF

#define RNPM_RAH_AV                           0x80000000
/* eth fix code */
#define RNPM_FCTRL_BPE                            BIT(10)
#define RNPM_FCTRL_UPE                            BIT(9)
#define RNPM_FCTRL_MPE                            BIT(8)

#define RNPM_MCSTCTRL_MTA                         BIT(2)
#define RNPM_MCSTCTRL_UTA                         BIT(3)

#define RNPM_MAX_LAYER2_FILTERS (16)
#define RNPM_MAX_TUPLE5_FILTERS (128)
#define RNPM_MAX_TCAM_FILTERS (4096)

#define RNPM_SRC_IP_MASK  BIT(0)
#define RNPM_DST_IP_MASK  BIT(1)
#define RNPM_SRC_PORT_MASK BIT(2)
#define RNPM_DST_PORT_MASK BIT(3)
#define RNPM_L4_PROTO_MASK BIT(4)
#endif /* _RNPM_TYPE_H_ */
