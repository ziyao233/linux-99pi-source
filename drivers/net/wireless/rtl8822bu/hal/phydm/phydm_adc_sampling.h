/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __INC_ADCSMP_H
#define __INC_ADCSMP_H

#define DYNAMIC_LA_MODE	"2.0"  /*2017.02.06  Dino */

#if (PHYDM_LA_MODE_SUPPORT == 1)

struct rt_adcsmp_string {
	u32		*octet;
	u32		length;
	u32		buffer_size;
	u32		start_pos;
};


enum rt_adcsmp_trig_sel {
	PHYDM_ADC_BB_TRIG	= 0,
	PHYDM_ADC_MAC_TRIG	= 1,
	PHYDM_ADC_RF0_TRIG	= 2,
	PHYDM_ADC_RF1_TRIG	= 3,
	PHYDM_MAC_TRIG		= 4
};


enum rt_adcsmp_trig_sig_sel {
	ADCSMP_TRIG_CRCOK	= 0,
	ADCSMP_TRIG_CRCFAIL	= 1,
	ADCSMP_TRIG_CCA		= 2,
	ADCSMP_TRIG_REG		= 3
};


enum rt_adcsmp_state {
	ADCSMP_STATE_IDLE		= 0,
	ADCSMP_STATE_SET		= 1,
	ADCSMP_STATE_QUERY	=	2
};


struct rt_adcsmp {
	struct rt_adcsmp_string		adc_smp_buf;
	enum rt_adcsmp_state		adc_smp_state;
	u8					la_trig_mode;
	u32					la_trig_sig_sel;
	u8					la_dma_type;
	u32					la_trigger_time;
	u32					la_mac_mask_or_hdr_sel; /*1.BB mode: for debug port header sel; 2.MAC mode: for reference mask*/
	u32					la_dbg_port;
	u8					la_trigger_edge;
	u8					la_smp_rate;
	u32					la_count;
	u8					is_bb_trigger;
	u8					la_work_item_index;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	RT_WORK_ITEM	adc_smp_work_item;
	RT_WORK_ITEM	adc_smp_work_item_1;
#endif
};

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
void adc_smp_work_item_callback(
	void	*context
);
#endif

void adc_smp_set(
	void	*dm_void,
	u8	trig_mode,
	u32	trig_sig_sel,
	u8	dma_data_sig_sel,
	u32	trigger_time,
	u16	polling_time
);

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
enum rt_status adc_smp_query(
	void	*dm_void,
	ULONG	information_buffer_length,
	void	*information_buffer,
	PULONG	bytes_written
);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
void adc_smp_query(
	void		*dm_void,
	void		*output,
	u32		out_len,
	u32		*pused
);

s32 adc_smp_get_sample_counts(
	void		*dm_void
);

s32 adc_smp_query_single_data(
	void		*dm_void,
	void		*output,
	u32		out_len,
	u32		index
);

#endif
void adc_smp_stop(
	void	*dm_void
);

void adc_smp_init(
	void	*dm_void
);

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
void adc_smp_de_init(
	void			*dm_void
);
#endif

void phydm_la_mode_bb_setting(
	void		*dm_void
);

void phydm_la_mode_set_trigger_time(
	void		*dm_void,
	u32		trigger_time_mu_sec
);

void phydm_lamode_trigger_setting(
	void		*dm_void,
	char			input[][16],
	u32		*_used,
	char			*output,
	u32		*_out_len,
	u32		input_num
);
#endif
#endif
