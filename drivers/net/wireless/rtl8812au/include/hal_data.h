/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __HAL_DATA_H__
#define __HAL_DATA_H__

#if 1//def  CONFIG_SINGLE_IMG

#include "../hal/OUTSRC/odm_precomp.h"

//
// <Roger_Notes> For RTL8723 WiFi/BT/GPS multi-function configuration. 2010.10.06.
//
typedef enum _RT_MULTI_FUNC {
	RT_MULTI_FUNC_NONE	= 0x00,
	RT_MULTI_FUNC_WIFI 	= 0x01,
	RT_MULTI_FUNC_BT 		= 0x02,
	RT_MULTI_FUNC_GPS 	= 0x04,
} RT_MULTI_FUNC, *PRT_MULTI_FUNC;
//
// <Roger_Notes> For RTL8723 WiFi PDn/GPIO polarity control configuration. 2010.10.08.
//
typedef enum _RT_POLARITY_CTL {
	RT_POLARITY_LOW_ACT 	= 0,
	RT_POLARITY_HIGH_ACT 	= 1,
} RT_POLARITY_CTL, *PRT_POLARITY_CTL;

// For RTL8723 regulator mode. by tynli. 2011.01.14.
typedef enum _RT_REGULATOR_MODE {
	RT_SWITCHING_REGULATOR 	= 0,
	RT_LDO_REGULATOR 			= 1,
} RT_REGULATOR_MODE, *PRT_REGULATOR_MODE;

//
// Interface type.
//
typedef	enum _INTERFACE_SELECT_PCIE {
	INTF_SEL0_SOLO_MINICARD			= 0,		// WiFi solo-mCard
	INTF_SEL1_BT_COMBO_MINICARD		= 1,		// WiFi+BT combo-mCard
	INTF_SEL2_PCIe						= 2,		// PCIe Card
} INTERFACE_SELECT_PCIE, *PINTERFACE_SELECT_PCIE;


typedef	enum _INTERFACE_SELECT_USB {
	INTF_SEL0_USB 				= 0,		// USB
	INTF_SEL1_USB_High_Power  	= 1,		// USB with high power PA
	INTF_SEL2_MINICARD		  	= 2,		// Minicard
	INTF_SEL3_USB_Solo 		= 3,		// USB solo-Slim module
	INTF_SEL4_USB_Combo		= 4,		// USB Combo-Slim module
	INTF_SEL5_USB_Combo_MF	= 5,		// USB WiFi+BT Multi-Function Combo, i.e., Proprietary layout(AS-VAU) which is the same as SDIO card
} INTERFACE_SELECT_USB, *PINTERFACE_SELECT_USB;

typedef enum _RT_AMPDU_BRUST_MODE {
	RT_AMPDU_BRUST_NONE 		= 0,
	RT_AMPDU_BRUST_92D 		= 1,
	RT_AMPDU_BRUST_88E 		= 2,
	RT_AMPDU_BRUST_8812_4 	= 3,
	RT_AMPDU_BRUST_8812_8 	= 4,
	RT_AMPDU_BRUST_8812_12 	= 5,
	RT_AMPDU_BRUST_8812_15	= 6,
	RT_AMPDU_BRUST_8723B	 	= 7,
} RT_AMPDU_BRUST, *PRT_AMPDU_BRUST_MODE;

#define CHANNEL_MAX_NUMBER			14+24+21	// 14 is the max channel number
#define CHANNEL_MAX_NUMBER_2G		14
#define CHANNEL_MAX_NUMBER_5G		54			// Please refer to "phy_GetChnlGroup8812A" and "Hal_ReadTxPowerInfo8812A"
#define CHANNEL_MAX_NUMBER_5G_80M	7
#define CHANNEL_GROUP_MAX				3+9	// ch1~3, ch4~9, ch10~14 total three groups
#define MAX_PG_GROUP					13

#define MAX_REGULATION_NUM						3
#define MAX_RF_PATH_NUM_IN_POWER_LIMIT_TABLE	4
#define MAX_2_4G_BANDWITH_NUM					2
#define MAX_2_4G_RATE_SECTION_NUM				3
#define MAX_2_4G_CHANNEL_NUM						5 // adopt channel group instead of individual channel 
#define MAX_5G_BANDWITH_NUM						4
#define MAX_5G_RATE_SECTION_NUM					4
#define MAX_5G_CHANNEL_NUM						14 // adopt channel group instead of individual channel 

#define MAX_BASE_NUM_IN_PHY_REG_PG_2_4G			4 //  CCK:1,OFDM:2, HT:2
#define MAX_BASE_NUM_IN_PHY_REG_PG_5G			5 // OFDM:1, HT:2, VHT:2


//###### duplicate code,will move to ODM #########
#define IQK_MAC_REG_NUM		4
#define IQK_ADDA_REG_NUM		16

#define IQK_BB_REG_NUM			10
#define IQK_BB_REG_NUM_92C	9
#define IQK_BB_REG_NUM_92D	10
#define IQK_BB_REG_NUM_test	6

#define IQK_Matrix_Settings_NUM_92D	1+24+21

#define HP_THERMAL_NUM		8
//###### duplicate code,will move to ODM #########

#ifdef CONFIG_RTL8192D
typedef enum _MACPHY_MODE_8192D {
	SINGLEMAC_SINGLEPHY,	//SMSP
	DUALMAC_DUALPHY,		//DMDP
	DUALMAC_SINGLEPHY,	//DMSP
} MACPHY_MODE_8192D, *PMACPHY_MODE_8192D;
#endif

#ifdef CONFIG_USB_RX_AGGREGATION
typedef enum _USB_RX_AGG_MODE {
	USB_RX_AGG_DISABLE,
	USB_RX_AGG_DMA,
	USB_RX_AGG_USB,
	USB_RX_AGG_MIX
} USB_RX_AGG_MODE;

//#define MAX_RX_DMA_BUFFER_SIZE	10240		// 10K for 8192C RX DMA buffer

#endif

struct dm_priv {
	u8	DM_Type;
	u8	DMFlag;
	u8	InitDMFlag;
	//u8   RSVD_1;

	u32	InitODMFlag;
	//* Upper and Lower Signal threshold for Rate Adaptive*/
	int	UndecoratedSmoothedPWDB;
	int	UndecoratedSmoothedCCK;
	int	EntryMinUndecoratedSmoothedPWDB;
	int	EntryMaxUndecoratedSmoothedPWDB;
	int	MinUndecoratedPWDBForDM;
	int	LastMinUndecoratedPWDBForDM;

	s32	UndecoratedSmoothedBeacon;
#ifdef CONFIG_BT_COEXIST
	s32 	BT_EntryMinUndecoratedSmoothedPWDB;
	s32 	BT_EntryMaxUndecoratedSmoothedPWDB;
#endif

//###### duplicate code,will move to ODM #########
	//for High Power
	u8 	bDynamicTxPowerEnable;
	u8 	LastDTPLvl;
	u8	DynamicTxHighPowerLvl;//Add by Jacken Tx Power Control for Near/Far Range 2008/03/06

	//for tx power tracking
	u8	bTXPowerTracking;
	u8	TXPowercount;
	u8	bTXPowerTrackingInit;
	u8	TxPowerTrackControl;	//for mp mode, turn off txpwrtracking as default
	u8	TM_Trigger;

	u8	ThermalMeter[2];				// ThermalMeter, index 0 for RFIC0, and 1 for RFIC1
	u8	ThermalValue;
	u8	ThermalValue_LCK;
	u8	ThermalValue_IQK;
	u8	ThermalValue_DPK;
	u8	bRfPiEnable;
	//u8   RSVD_2;

	//for APK
	u32	APKoutput[2][2];	//path A/B; output1_1a/output1_2a
	u8	bAPKdone;
	u8	bAPKThermalMeterIgnore;
	u8	bDPdone;
	u8	bDPPathAOK;
	u8	bDPPathBOK;
	//u8   RSVD_3;
	//u8   RSVD_4;
	//u8   RSVD_5;

	//for IQK
	u32	ADDA_backup[IQK_ADDA_REG_NUM];
	u32	IQK_MAC_backup[IQK_MAC_REG_NUM];
	u32	IQK_BB_backup_recover[9];
	u32	IQK_BB_backup[IQK_BB_REG_NUM];

	u8	PowerIndex_backup[6];
	u8	OFDM_index[2];

	u8	bCCKinCH14;
	u8	CCK_index;
	u8	bDoneTxpower;
	u8	CCK_index_HP;

	u8	OFDM_index_HP[2];
	u8	ThermalValue_HP[HP_THERMAL_NUM];
	u8	ThermalValue_HP_index;
	//u8   RSVD_6;

	//for TxPwrTracking2
	s32	RegE94;
	s32  RegE9C;
	s32	RegEB4;
	s32	RegEBC;

	u32	TXPowerTrackingCallbackCnt;	//cosa add for debug

	u32	prv_traffic_idx; // edca turbo
#ifdef CONFIG_RTL8192D
	u8	ThermalValue_AVG[AVG_THERMAL_NUM];
	u8	ThermalValue_AVG_index;
	u8	ThermalValue_RxGain;
	u8	ThermalValue_Crystal;
	u8	bReloadtxpowerindex;

	u32	RegD04_MP;

	u8	RegC04_MP;
	u8	Delta_IQK;
	u8	Delta_LCK;
	//u8   RSVD_7;

	BOOLEAN	bDPKdone[2];
	//u16 RSVD_8;

	u32	RegA24;
	u32	RegRF3C[2];	//pathA / pathB
#endif
//###### duplicate code,will move to ODM #########

	// Add for Reading Initial Data Rate SEL Register 0x484 during watchdog. Using for fill tx desc. 2011.3.21 by Thomas
	u8	INIDATA_RATE[32];
};


typedef struct hal_com_data {
	HAL_VERSION			VersionID;
	RT_MULTI_FUNC		MultiFunc; // For multi-function consideration.
	RT_POLARITY_CTL		PolarityCtl; // For Wifi PDn Polarity control.
	RT_REGULATOR_MODE	RegulatorMode; // switching regulator or LDO

	u16	FirmwareVersion;
	u16	FirmwareVersionRev;
	u16	FirmwareSubVersion;
	u16	FirmwareSignature;

	//current WIFI_PHY values
	WIRELESS_MODE		CurrentWirelessMode;
	CHANNEL_WIDTH	CurrentChannelBW;
	BAND_TYPE			CurrentBandType;	//0:2.4G, 1:5G
	BAND_TYPE			BandSet;
	u8	CurrentChannel;
	u8	CurrentCenterFrequencyIndex1;
	u8	nCur40MhzPrimeSC;// Control channel sub-carrier
	u8	nCur80MhzPrimeSC;   //used for primary 40MHz of 80MHz mode

	u16	CustomerID;
	u16	BasicRateSet;
	u16 ForcedDataRate;// Force Data Rate. 0: Auto, 0x02: 1M ~ 0x6C: 54M.
	u32	ReceiveConfig;

	//rf_ctrl
	u8	rf_chip;
	u8	rf_type;
	u8	NumTotalRFPath;

	u8	InterfaceSel;
	u8	framesync;
	u32	framesyncC34;
	u8	framesyncMonitor;
	u8	DefaultInitialGain[4];
	//
	// EEPROM setting.
	//
	u16	EEPROMVID;
	u16	EEPROMSVID;
#ifdef CONFIG_USB_HCI
	u16	EEPROMPID;
	u16	EEPROMSDID;
#endif
#ifdef CONFIG_PCI_HCI
	u16	EEPROMDID;
	u16	EEPROMSMID;
#endif

	u8	EEPROMCustomerID;
	u8	EEPROMSubCustomerID;
	u8	EEPROMVersion;
	u8	EEPROMRegulatory;
	u8	EEPROMThermalMeter;
	u8	EEPROMBluetoothCoexist;
	u8	EEPROMBluetoothType;
	u8	EEPROMBluetoothAntNum;
	u8	EEPROMBluetoothAntIsolation;
	u8	EEPROMBluetoothRadioShared;
	u8	bTXPowerDataReadFromEEPORM;
	u8	bAPKThermalMeterIgnore;

	BOOLEAN 		EepromOrEfuse;
	u8				EfuseUsedPercentage;
	u16				EfuseUsedBytes;
	//u8				EfuseMap[2][HWSET_MAX_SIZE_JAGUAR];
	EFUSE_HAL		EfuseHal;

	//---------------------------------------------------------------------------------//
	//3 [2.4G]
	u8	Index24G_CCK_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER];
	u8	Index24G_BW40_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER];
	//If only one tx, only BW20 and OFDM are used.
	s8	CCK_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	OFDM_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW20_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW40_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	//3 [5G]
	u8	Index5G_BW40_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER];
	u8	Index5G_BW80_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER_5G_80M];
	s8	OFDM_5G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW20_5G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW40_5G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW80_5G_Diff[MAX_RF_PATH][MAX_TX_COUNT];

	u8	Regulation2_4G;
	u8	Regulation5G;

	u8	TxPwrInPercentage;

	u8	TxPwrCalibrateRate;
	//
	// TX power by rate table at most 4RF path.
	// The register is
	//
	// VHT TX power by rate off setArray =
	// Band:-2G&5G = 0 / 1
	// RF: at most 4*4 = ABCD=0/1/2/3
	// CCK=0 OFDM=1/2 HT-MCS 0-15=3/4/56 VHT=7/8/9/10/11
	//
	u8	TxPwrByRateTable;
	u8	TxPwrByRateBand;
	u32	TxPwrByRateOffset[TX_PWR_BY_RATE_NUM_BAND]
	[TX_PWR_BY_RATE_NUM_RF]
	[TX_PWR_BY_RATE_NUM_SECTION];
	//---------------------------------------------------------------------------------//

	//2 Power Limit Table
	u8	TxPwrLevelCck[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];
	u8	TxPwrLevelHT40_1S[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];	// For HT 40MHZ pwr
	u8	TxPwrLevelHT40_2S[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];	// For HT 40MHZ pwr
	u8	TxPwrHt20Diff[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];// HT 20<->40 Pwr diff
	u8	TxPwrLegacyHtDiff[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];// For HT<->legacy pwr diff

	// Power Limit Table for 2.4G
	u8	TxPwrLimit_2_4G[MAX_REGULATION_NUM]
	[MAX_2_4G_BANDWITH_NUM]
	[MAX_2_4G_RATE_SECTION_NUM]
	[MAX_2_4G_CHANNEL_NUM]
	[MAX_RF_PATH_NUM];

	// Power Limit Table for 5G
	u8	TxPwrLimit_5G[MAX_REGULATION_NUM]
	[MAX_5G_BANDWITH_NUM]
	[MAX_5G_RATE_SECTION_NUM]
	[MAX_5G_CHANNEL_NUM]
	[MAX_RF_PATH_NUM];


	// Store the original power by rate value of the base of each rate section of rf path A & B
	u8	TxPwrByRateBase2_4G[MAX_RF_PATH_NUM_IN_POWER_LIMIT_TABLE]
	[MAX_BASE_NUM_IN_PHY_REG_PG_2_4G];
	u8	TxPwrByRateBase5G[MAX_RF_PATH_NUM_IN_POWER_LIMIT_TABLE]
	[MAX_BASE_NUM_IN_PHY_REG_PG_5G];

	// For power group
	u8	PwrGroupHT20[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];
	u8	PwrGroupHT40[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];




	u8	PGMaxGroup;
	u8	LegacyHTTxPowerDiff;// Legacy to HT rate power diff
	// The current Tx Power Level
	u8	CurrentCckTxPwrIdx;
	u8	CurrentOfdm24GTxPwrIdx;
	u8	CurrentBW2024GTxPwrIdx;
	u8	CurrentBW4024GTxPwrIdx;

	// Read/write are allow for following hardware information variables
	u8	pwrGroupCnt;
	u32	MCSTxPowerLevelOriginalOffset[MAX_PG_GROUP][16];
	u32	CCKTxPowerLevelOriginalOffset;

	u8	CrystalCap;
	u32	AntennaTxPath;					// Antenna path Tx
	u32	AntennaRxPath;					// Antenna path Rx

	u8	PAType_2G;
	u8	PAType_5G;
	u8	LNAType_2G;
	u8	LNAType_5G;
	u8	ExternalPA_2G;
	u8	ExternalLNA_2G;
	u8	ExternalPA_5G;
	u8	ExternalLNA_5G;
	u8	RFEType;
	u8	BoardType;
	u8	ExternalPA;
	u8	bIQKInitialized;
	BOOLEAN		bLCKInProgress;

	BOOLEAN		bSwChnl;
	BOOLEAN		bSetChnlBW;
	BOOLEAN		bChnlBWInitialzed;
	BOOLEAN		bNeedIQK;

	u8	bLedOpenDrain; // Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16.
	u8	TxPowerTrackControl; //for mp mode, turn off txpwrtracking as default
	u8	b1x1RecvCombine;	// for 1T1R receive combining

	u32	AcParam_BE; //Original parameter for BE, use for EDCA turbo.

	BB_REGISTER_DEFINITION_T	PHYRegDef[4];	//Radio A/B/C/D

	u32	RfRegChnlVal[2];

	//RDG enable
	BOOLEAN	 bRDGEnable;

	//for host message to fw
	u8	LastHMEBoxNum;

	u8	fw_ractrl;
	u8	RegTxPause;
	// Beacon function related global variable.
	u8	RegBcnCtrlVal;
	u8	RegFwHwTxQCtrl;
	u8	RegReg542;
	u8	RegCR_1;
	u8	Reg837;
	u8	RegRFPathS1;
	u16	RegRRSR;

	u8	CurAntenna;
	u8	AntDivCfg;
	u8	TRxAntDivType;

	u8	bDumpRxPkt;//for debug
	u8	bDumpTxPkt;//for debug
	u8	FwRsvdPageStartOffset; //2010.06.23. Added by tynli. Reserve page start offset except beacon in TxQ.

	// 2010/08/09 MH Add CU power down mode.
	BOOLEAN		pwrdown;

	// Add for dual MAC  0--Mac0 1--Mac1
	u32	interfaceIndex;

	u8	OutEpQueueSel;
	u8	OutEpNumber;

	// 2010/12/10 MH Add for USB aggreation mode dynamic shceme.
	BOOLEAN		UsbRxHighSpeedMode;

	// 2010/11/22 MH Add for slim combo debug mode selective.
	// This is used for fix the drawback of CU TSMC-A/UMC-A cut. HW auto suspend ability. Close BT clock.
	BOOLEAN		SlimComboDbg;

#ifdef CONFIG_P2P
	u8	p2p_ps_offload;
#endif

	u8	AMPDUDensity;

	// Auto FSM to Turn On, include clock, isolation, power control for MAC only
	u8	bMacPwrCtrlOn;

	RT_AMPDU_BRUST
	AMPDUBurstMode; //92C maybe not use, but for compile successfully

#ifdef CONFIG_SDIO_HCI
	//
	// For SDIO Interface HAL related
	//

	//
	// SDIO ISR Related
	//
//	u32			IntrMask[1];
//	u32			IntrMaskToSet[1];
//	LOG_INTERRUPT		InterruptLog;
	u32			sdio_himr;
	u32			sdio_hisr;

	//
	// SDIO Tx FIFO related.
	//
	// HIQ, MID, LOW, PUB free pages; padapter->xmitpriv.free_txpg
	u8			SdioTxFIFOFreePage[SDIO_TX_FREE_PG_QUEUE];
	_lock		SdioTxFIFOFreePageLock;
#ifndef CONFIG_SDIO_TX_TASKLET
	_thread_hdl_	SdioXmitThread;
	_sema		SdioXmitSema;
	_sema		SdioXmitTerminateSema;
#endif//CONFIG_SDIO_TX_TASKLET
	//
	// SDIO Rx FIFO related.
	//
	u8			SdioRxFIFOCnt;
	u16			SdioRxFIFOSize;
#endif //CONFIG_SDIO_HCI

#ifdef CONFIG_USB_HCI
	u32	UsbBulkOutSize;
	BOOLEAN		bSupportUSB3;

	// Interrupt relatd register information.
	u32	IntArray[3];//HISR0,HISR1,HSISR
	u32	IntrMask[3];
	u8	C2hArray[16];
#ifdef CONFIG_USB_TX_AGGREGATION
	u8	UsbTxAggMode;
	u8	UsbTxAggDescNum;
#endif // CONFIG_USB_TX_AGGREGATION

#ifdef CONFIG_USB_RX_AGGREGATION
	u16	HwRxPageSize;				// Hardware setting
	u32	MaxUsbRxAggBlock;

	USB_RX_AGG_MODE	UsbRxAggMode;
	u8	UsbRxAggBlockCount;			// USB Block count. Block size is 512-byte in hight speed and 64-byte in full speed
	u8	UsbRxAggBlockTimeout;
	u8	UsbRxAggPageCount;			// 8192C DMA page count
	u8	UsbRxAggPageTimeout;

	u8	RegAcUsbDmaSize;
	u8	RegAcUsbDmaTime;
#endif//CONFIG_USB_RX_AGGREGATION
#endif //CONFIG_USB_HCI


#ifdef CONFIG_PCI_HCI
	//
	// EEPROM setting.
	//
	u16	EEPROMChannelPlan;

	u8	EEPROMTSSI[2];
	u8	EEPROMBoardType;
	u32	TransmitConfig;

	u32	IntrMask[2];
	u32	IntrMaskToSet[2];

	u8	bDefaultAntenna;
	//u8	bIQKInitialized;

	u8	bInterruptMigration;
	u8	bDisableTxInt;
	u8	bGpioHwWpsPbc;
#endif //CONFIG_PCI_HCI

	struct dm_priv	dmpriv;
	DM_ODM_T 		odmpriv;

#ifdef DBG_CONFIG_ERROR_DETECT
	struct sreset_priv srestpriv;
#endif

#ifdef CONFIG_BT_COEXIST
	struct btcoexist_priv	bt_coexist;
#endif

#if defined(CONFIG_RTL8723A) || defined(CONFIG_RTL8723B)
	// Interrupt relatd register information.
	u32			SysIntrStatus;
	u32			SysIntrMask;
#endif //endif CONFIG_RTL8723A


#if defined(CONFIG_RTL8192C) ||defined(CONFIG_RTL8192D)

	u8	BluetoothCoexist;

	u8	EEPROMChnlAreaTxPwrCCK[2][3];
	u8	EEPROMChnlAreaTxPwrHT40_1S[2][3];
	u8	EEPROMChnlAreaTxPwrHT40_2SDiff[2][3];
	u8	EEPROMPwrLimitHT20[3];
	u8	EEPROMPwrLimitHT40[3];
#ifdef CONFIG_RTL8192D
	MACPHY_MODE_8192D	MacPhyMode92D;
	BAND_TYPE	CurrentBandType92D;	//0:2.4G, 1:5G
	BAND_TYPE	BandSet92D;
	BOOLEAN       bMasterOfDMSP;
	BOOLEAN       bSlaveOfDMSP;

	IQK_MATRIX_REGS_SETTING IQKMatrixRegSetting[IQK_Matrix_Settings_NUM_92D];
#ifdef CONFIG_DUALMAC_CONCURRENT
	BOOLEAN		bInModeSwitchProcess;
#endif
	u8	AutoLoadStatusFor8192D;
	u8	EEPROMC9;
	u8	EEPROMCC;
	u8	PAMode;
	u8	InternalPA5G[2];	//pathA / pathB
	BOOLEAN		bPhyValueInitReady;
	BOOLEAN		bLoadIMRandIQKSettingFor2G;// True if IMR or IQK  have done  for 2.4G in scan progress
	BOOLEAN		bNOPG;
	BOOLEAN		bIsVS;
	//Query RF by FW
	BOOLEAN		bReadRFbyFW;
	BOOLEAN		bEarlyModeEnable;
	BOOLEAN		bSupportRemoteWakeUp;
	BOOLEAN		bInSetPower;
	u8	RTSInitRate;	 // 2010.11.24.by tynli.
#endif //CONFIG_RTL8192D 

#endif //defined(CONFIG_RTL8192C) ||defined(CONFIG_RTL8192D)
} HAL_DATA_COMMON, *PHAL_DATA_COMMON;


typedef struct hal_com_data HAL_DATA_TYPE, *PHAL_DATA_TYPE;
#define GET_HAL_DATA(__pAdapter)	((HAL_DATA_TYPE *)((__pAdapter)->HalData))
#define GET_HAL_RFPATH_NUM(__pAdapter) (((HAL_DATA_TYPE *)((__pAdapter)->HalData))->NumTotalRFPath )
#define RT_GetInterfaceSelection(_Adapter) 	(GET_HAL_DATA(_Adapter)->InterfaceSel)
#endif


#endif //__HAL_DATA_H__

