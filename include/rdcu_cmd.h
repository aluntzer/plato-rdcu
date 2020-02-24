/**
 * @file   rdcu_cmd.h
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2018
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @see FPGA Requirement Specification PLATO-IWF-PL-RS-005 Issue 0.6
 */
#ifndef _RDCU_CMD_H_
#define _RDCU_CMD_H_

#include <stdint.h>

/* readable RDCU register addresses (RDCU-FRS-FN-0284) */
#define FPGA_VERSION			0x10000000UL
#define RDCU_STATUS			0x10000004UL
#define LVDS_CORE_STATUS		0x10000008UL
#define SPW_LINK_STATUS			0x1000000CUL
#define SPW_ERR_CNTRS			0x10000010UL
#define RMAP_LAST_ERR			0x10000014UL
#define RMAP_NO_REPLY_ERR_CNTRS		0x10000018UL
#define RMAP_PCKT_ERR_CNTRS		0x1000001CUL
#define ADC_VALUES_1			0x10000020UL
#define ADC_VALUES_2			0x10000024UL
#define ADC_VALUES_3			0x10000028UL
#define ADC_VALUES_4			0x1000002CUL
#define ADC_STATUS			0x10000030UL
/* spare: 0x10000034UL */
#define COMPR_STATUS			0x10000038UL
/* spare: 0x1000003CUL */

/* writeable RDCU register addresses (RDCU-FRS-FN-0284) */
#define RDCU_RESET			0x10000040UL
#define SPW_LINK_CTRL			0x10000044UL
#define LVDS_CTRL			0x10000048UL
#define CORE_CTRL			0x1000004CUL
#define ADC_CTRL			0x10000050UL
/* spare: 0x10000054UL */
#define COMPR_CTRL			0x10000058UL
/* spare: 0x1000005CUL */



/* writeable Data Compressor register addresses (RDCU-FRS-FN-0288) */
#define COMPR_PARAM_1			0x11000000UL
#define COMPR_PARAM_2			0x11000004UL
#define ADAPTIVE_PARAM_1		0x11000008UL
#define ADAPTIVE_PARAM_2		0x1100000CUL
#define DATA_START_ADDR			0x11000010UL
#define MODEL_START_ADDR		0x11000014UL
#define NUM_SAMPLES			0x11000018UL
#define NEW_MODEL_START_ADDR		0x1100001CUL
#define COMPR_DATA_BUF_START_ADDR	0x11000020UL
#define COMPR_DATA_BUF_LEN		0x11000024UL
/* spare: 0x11000028UL */
/* spare: 0x1100002CUL */

/* readable Data Compressor register addresses (RDCU-FRS-FN-0288) */
#define USED_PARAM_1			0x11000030UL
#define USED_PARAM_2			0x11000034UL
#define COMPR_DATA_START_ADDR		0x11000038UL
#define COMPR_DATA_SIZE			0x1100003CUL
#define COMPR_DATA_ADAPTIVE_1_SIZE	0x11000040UL
#define COMPR_DATA_ADAPTIVE_2_SIZE	0x11000044UL
#define COMPR_ERROR			0x11000048UL
#define MODEL_INFO_START_ADDR		0x1100004CUL
#define MODEL_INFO_LEN			0x11000050UL
/* spare: 0x11000054UL */
/* spare: 0x11000058UL */
/* spare: 0x1100005CUL */


/* writeable SRAM EDAC register addresses (RDCU-FRS-FN-0292) */
#define SRAM_EDAC_CTRL			0x01000000UL
/* spare: 0x01000004UL */

/* readable SRAM EDAC register addresses (RDCU-FRS-FN-0292) */
#define SRAM_EDAC_STATUS		0x01000008UL
/* spare: 0x0100000CUL */

/* SRAM address range (RDCU-FRS-FN-0280) */
#define RDCU_SRAM_START			0x00000000UL
#define RDCU_SRAM_END			0x007FFFFFUL
#define RDCU_SRAM_SIZE			(RDCU_SRAM_END - RDCU_SRAM_START + 1UL)




int rdcu_read_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr);
int rdcu_write_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr);

int rdcu_write_cmd_data(uint16_t trans_id, uint8_t *cmd,
			uint32_t addr, uint32_t size);
int rdcu_read_cmd_data(uint16_t trans_id, uint8_t *cmd,
		       uint32_t addr, uint32_t size);


/* RDCU read accessors */
int rdcu_read_cmd_fpga_version(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_rdcu_status(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_lvds_core_status(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_spw_link_status(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_spw_err_cntrs(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_rmap_last_err(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_rmap_no_reply_err_cntrs(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_rmap_pckt_err_cntrs(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_adc_values_1(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_adc_values_2(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_adc_values_3(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_adc_values_4(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_adc_status(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_status(uint16_t trans_id, uint8_t *cmd);

/* RDCU read accessors */
int rdcu_write_cmd_rdcu_reset(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_spw_link_ctrl(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_lvds_ctrl(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_core_ctrl(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_adc_ctrl(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_compr_ctrl(uint16_t trans_id, uint8_t *cmd);

/* Data Compressor write accessors */
int rdcu_write_cmd_compressor_param1(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_compressor_param2(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_adaptive_param1(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_adaptive_param2(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_data_start_addr(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_model_start_addr(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_num_samples(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_new_model_start_addr(uint16_t trans_id, uint8_t *cmd);

int rdcu_write_cmd_compr_data_buf_start_addr(uint16_t trans_id, uint8_t *cmd);
int rdcu_write_cmd_compr_data_buf_len(uint16_t trans_id, uint8_t *cmd);

/* Data Compressor read accessors */
int rdcu_read_cmd_used_param1(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_used_param2(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_data_start_addr(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_data_size(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_data_adaptive_1_size(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_data_adaptive_2_size(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_compr_error(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_model_info_start_addr(uint16_t trans_id, uint8_t *cmd);
int rdcu_read_cmd_model_info_len(uint16_t trans_id, uint8_t *cmd);


/* SRAM EDAC read accessors */
int rdcu_read_cmd_sram_edac_status(uint16_t trans_id, uint8_t *cmd);

/* SRAM EDAC write accessors */
int rdcu_write_cmd_sram_edac_ctrl(uint16_t trans_id, uint8_t *cmd);


#endif /* _RDCU_CMD_H_ */
