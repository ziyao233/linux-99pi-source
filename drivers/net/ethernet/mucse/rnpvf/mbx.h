#ifndef _RNP_MBX_H_
#define _RNP_MBX_H_

#include "vf.h"

#define RNP_VFMAILBOX_SIZE 14 /* 16 32 bit words - 64 bytes */
#define RNP_ERR_MBX -100

struct mbx_shm {
	u32 stat;
#define MBX_ST_PF_ACK (1 << 0)
#define MBX_ST_PF_STS (1 << 1)
#define MBX_ST_PF_RST (1 << 2)

#define MBX_ST_VF_ACK (1 << 3)
#define MBX_ST_VF_REQ (1 << 4)
#define MBX_ST_VF_RST (1 << 5)

#define MBX_ST_CPU_ACK (1 << 6)
#define MBX_ST_CPU_REQ (1 << 7)

	u32 data[RNP_VFMAILBOX_SIZE];
} __aligned(4);

/* If it's a RNP_VF_* msg then it originates in the VF and is sent to the
 * PF.  The reverse is true if it is RNP_PF_*.
 * Message ACK's are the value or'd with 0xF0000000
 */
#define RNP_VT_MSGTYPE_ACK                                                     \
	0x80000000
/* Messages below or'd with
 * this are the ACK
 */
#define RNP_VT_MSGTYPE_NACK                                                    \
	0x40000000
/* Messages below or'd with
 * this are the NACK
 */
#define RNP_VT_MSGTYPE_CTS                                                     \
	0x20000000
/* Indicates that VF is still
 * clear to send requests
 */
#define RNP_VT_MSGINFO_SHIFT 14
/* bits 23:16 are used for exra info for certain messages */
#define RNP_VT_MSGINFO_MASK (0xFF << RNP_VT_MSGINFO_SHIFT)

/* mailbox API, legacy requests */
#define RNP_VF_RESET 0x01 /* VF requests reset */
#define RNP_VF_SET_MAC_ADDR 0x02 /* VF requests PF to set MAC addr */
#define RNP_VF_SET_MULTICAST 0x03 /* VF requests PF to set MC addr */
#define RNP_VF_SET_VLAN 0x04 /* VF requests PF to set VLAN */

/* mailbox API, version 1.0 VF requests */
#define RNP_VF_SET_LPE 0x05 /* VF requests PF to set VMOLR.LPE */
#define RNP_VF_SET_MACVLAN 0x06 /* VF requests PF for unicast filter */
#define RNP_VF_GET_MACVLAN 0x07 /* VF requests mac */
#define RNP_VF_API_NEGOTIATE 0x08 /* negotiate API version */

/* mailbox API, version 1.1 VF requests */
#define RNP_VF_GET_QUEUE 0x09 /* get queue configuration */
#define RNP_VF_SET_VLAN_STRIP 0x0a /* VF requests PF to set VLAN STRIP */
#define RNP_VF_REG_RD 0x0b /* vf read reg */
#define RNP_VF_GET_MTU 0x0c /* vf read reg */
#define RNP_VF_SET_MTU 0x0d /* vf read reg */
#define RNP_VF_GET_FW 0x0e /* vf read reg */
#define RNP_VF_RESET_PF 0x13 /* vf read reg */

#define RNP_PF_VFNUM_MASK (0x3f << 21)
#define RNP_PF_SET_FCS 0x10 /* PF set fcs status */
#define RNP_PF_SET_PAUSE 0x11 /* PF set pause status */
#define RNP_PF_SET_FT_PADDING 0x12 /* PF set ft padding status */
#define RNP_PF_SET_VLAN_FILTER 0x13
#define RNP_PF_SET_VLAN 0x14
#define RNP_PF_SET_LINK 0x15
#define RNP_PF_SET_MTU 0x16
#define RNP_PF_SET_RESET 0x17
#define RNP_PF_LINK_UP (1 << 31)

#define RNP_PF_REMOVE 0x0f
#define RNP_PF_GET_LINK 0x10
/* GET_QUEUES return data indices within the mailbox */
#define RNP_VF_TX_QUEUES 1 /* number of Tx queues supported */
#define RNP_VF_RX_QUEUES 2 /* number of Rx queues supported */
#define RNP_VF_TRANS_VLAN 3 /* Indication of port vlan */
#define RNP_VF_DEF_QUEUE 4 /* Default queue offset */

/* length of permanent address message returned from PF */
#define RNP_VF_PERMADDR_MSG_LEN 11
/* word in permanent address message with the current multicast type */
#define RNP_VF_MC_TYPE_WORD 3
#define RNP_VF_DMA_VERSION_WORD 4
#define RNP_VF_VLAN_WORD 5
#define RNP_VF_PHY_TYPE_WORD 6
#define RNP_VF_FW_VERSION_WORD 7
#define RNP_VF_LINK_STATUS_WORD 8
#define RNP_VF_AXI_MHZ 9
#define RNP_VF_FEATURE 10

#define RNP_PF_CONTROL_PRING_MSG 0x0100 /* PF control message */

#define RNP_VF_MBX_INIT_TIMEOUT 2000 /* number of retries on mailbox */
#define RNP_VF_MBX_INIT_DELAY 500 /* microseconds between retries */

/* forward declaration of the HW struct */
struct rnpvf_hw;

enum MBX_ID {
	MBX_VF0 = 0,
	MBX_VF1,
	//...
	MBX_VF63,
	MBX_CM3CPU,
	MBX_VFCNT
};

#endif /* _RNP_MBX_H_ */
