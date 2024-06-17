#ifndef _RNPM_SRIOV_H_
#define _RNPM_SRIOV_H_

void rnpm_restore_vf_multicasts(struct rnpm_adapter *adapter);
void rnpm_msg_task(struct rnpm_pf_adapter *adapter);
int rnpm_vf_configuration(struct pci_dev *pdev, unsigned int event_mask);
void rnpm_disable_tx_rx(struct rnpm_adapter *adapter);
void rnpm_ping_all_vfs(struct rnpm_adapter *adapter);
int rnpm_ndo_set_vf_mac(struct net_device *netdev, int queue, u8 *mac);
void rnpm_msg_post_status(struct rnpm_adapter *adapter, enum PF_STATUS status);
#ifdef HAVE_NDO_SET_VF_MIN_MAX_TX_RATE
int rnpm_ndo_set_vf_bw(struct net_device *netdev, int vf,
                      int __always_unused min_tx_rate, int max_tx_rate);
#else
int rnpm_ndo_set_vf_bw(struct net_device *netdev, int vf, int max_tx_rate);
#endif /* HAVE_NDO_SET_VF_MIN_MAX_TX_RATE */

int rnpm_ndo_set_vf_spoofchk(struct net_device *netdev, int vf, bool setting);
int rnpm_ndo_get_vf_config(struct net_device *netdev,
			    int vf, struct ifla_vf_info *ivi);
void rnpm_check_vf_rate_limit(struct rnpm_adapter *adapter);
int rnpm_disable_sriov(struct rnpm_adapter *adapter);
#ifdef CONFIG_PCI_IOV
void rnpm_enable_sriov(struct rnpm_adapter *adapter);
#endif
int rnpm_pci_sriov_configure(struct pci_dev *dev, int num_vfs);

static inline void rnpm_set_vmvir(struct rnpm_adapter *adapter,
				   u16 vid, u16 qos, u32 vf)
{
	// struct rnpm_hw *hw = &adapter->hw;
}

#endif /* _RNPM_SRIOV_H_ */

