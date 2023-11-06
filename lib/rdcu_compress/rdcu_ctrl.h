/**
 * @file   rdcu_ctrl.h
 * @author Armin Luntzer (armin.luntzer@univie.ac.at)
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
 * @brief RMAP RDCU control library header file
 * @see FPGA Requirement Specification PLATO-IWF-PL-RS-005 Issue 0.6
 */

#ifndef RDCU_CTRL_H
#define RDCU_CTRL_H

#include <stdint.h>


/**
 * @brief local mirror of the RDCU registers
 */

struct rdcu_mirror {
	/* RDCU registers */
	uint32_t fpga_version;			/* RDCU-FRS-FN-0522 */
	uint32_t rdcu_status;			/* RDCU-FRS-FN-0532 */
	uint32_t lvds_core_status;		/* RDCU-FRS-FN-0542 */
	uint32_t spw_link_status;		/* RDCU-FRS-FN-0552 */
	uint32_t spw_err_cntrs;			/* RDCU-FRS-FN-0562 */
	uint32_t rmap_last_err;			/* RDCU-FRS-FN-0572 */
	uint32_t rmap_no_reply_err_cntrs;	/* RDCU-FRS-FN-0582 */
	uint32_t rmap_pckt_err_cntrs;		/* RDCU-FRS-FN-0592 */
	uint32_t adc_values_1;			/* RDCU-FRS-FN-0602 */
	uint32_t adc_values_2;
	uint32_t adc_values_3;
	uint32_t adc_values_4;
	uint32_t adc_status;			/* RDCU-FRS-FN-0610 */
	uint32_t compr_status;			/* RDCU-FRS-FN-0632 */
	uint32_t rdcu_reset;			/* RDCU-FRS-FN-0662 */
	uint32_t spw_link_ctrl;			/* RDCU-FRS-FN-0672 */
	uint32_t lvds_ctrl;			/* RDCU-FRS-FN-0682 */
	uint32_t core_ctrl;			/* RDCU-FRS-FN-0692 */
	uint32_t adc_ctrl;			/* RDCU-FRS-FN-0712 */
	uint32_t compr_ctrl;			/* RDCU-FRS-FN-0732 */

	/* Data Compressor registers */
	uint32_t compressor_param1;		/* RDCU-FRS-FN-0772 */
	uint32_t compressor_param2;		/* RDCU-FRS-FN-0782 */
	uint32_t adaptive_param1;		/* RDCU-FRS-FN-0792 */
	uint32_t adaptive_param2;		/* RDCU-FRS-FN-0802 */
	uint32_t data_start_addr;		/* RDCU-FRS-FN-0812 */
	uint32_t model_start_addr;		/* RDCU-FRS-FN-0822 */
	uint32_t num_samples;			/* RDCU-FRS-FN-0832 */
	uint32_t new_model_start_addr;		/* RDCU-FRS-FN-0842 */
	uint32_t compr_data_buf_start_addr;	/* RDCU-FRS-FN-0852 */
	uint32_t compr_data_buf_len;		/* RDCU-FRS-FN-0862 */

	uint32_t used_param1;			/* RDCU-FRS-FN-0892 */
	uint32_t used_param2;			/* RDCU-FRS-FN-0902 */
	uint32_t compr_data_start_addr;		/* RDCU-FRS-FN-0912 */
	uint32_t compr_data_size;		/* RDCU-FRS-FN-0922 */
	uint32_t compr_data_adaptive_1_size;	/* RDCU-FRS-FN-0932 */
	uint32_t compr_data_adaptive_2_size;	/* RDCU-FRS-FN-0942 */
	uint32_t compr_error;			/* RDCU-FRS-FN-0952 */
	uint32_t new_model_addr_used;		/* RDCU-FRS-FN-0962 */
	uint32_t samples_used;			/* RDCU-FRS-FN-0970 */


	/* SRAM EDAC registers */
	uint32_t sram_edac_ctrl;		/* RDCU-FRS-FN-1012 */
	uint32_t sram_edac_status;		/* RDCU-FRS-FN-1032 */

	uint8_t *sram;				/* RDCU-FRS-FN-0280 */
};


/* RDCU registers */
int rdcu_sync_fpga_version(void);
int rdcu_sync_rdcu_status(void);
int rdcu_sync_lvds_core_status(void);
int rdcu_sync_spw_link_status(void);
int rdcu_sync_spw_err_cntrs(void);
int rdcu_sync_rmap_last_err(void);
int rdcu_sync_rmap_no_reply_err_cntrs(void);
int rdcu_sync_rmap_pckt_err_cntrs(void);
int rdcu_sync_adc_values(int id);
int rdcu_sync_adc_status(void);
int rdcu_sync_compr_status(void);
int rdcu_sync_rdcu_reset(void);
int rdcu_sync_spw_link_ctrl(void);
int rdcu_sync_lvds_ctrl(void);
int rdcu_sync_core_ctrl(void);
int rdcu_sync_adc_ctrl(void);
int rdcu_sync_compr_ctrl(void);


/* Data Compressor registers */
int rdcu_sync_compressor_param1(void);
int rdcu_sync_compressor_param2(void);
int rdcu_sync_adaptive_param1(void);
int rdcu_sync_adaptive_param2(void);
int rdcu_sync_data_start_addr(void);
int rdcu_sync_model_start_addr(void);
int rdcu_sync_num_samples(void);
int rdcu_sync_new_model_start_addr(void);
int rdcu_sync_compr_data_buf_start_addr(void);
int rdcu_sync_compr_data_buf_len(void);

int rdcu_sync_used_param1(void);
int rdcu_sync_used_param2(void);
int rdcu_sync_compr_data_start_addr(void);
int rdcu_sync_compr_data_size(void);
int rdcu_sync_compr_data_adaptive_1_size(void);
int rdcu_sync_compr_data_adaptive_2_size(void);
int rdcu_sync_compr_error(void);
int rdcu_sync_new_model_addr_used(void);
int rdcu_sync_samples_used(void);

/* SRAM EDAC registers */
int rdcu_sync_sram_edac_ctrl(void);
int rdcu_sync_sram_edac_status(void);

/* SRAM */
int rdcu_sync_mirror_to_sram(uint32_t addr, uint32_t size, uint32_t mtu);
int rdcu_sync_sram_to_mirror(uint32_t addr, uint32_t size, uint32_t mtu);



/* RDCU registers */
uint16_t rdcu_get_fpga_version(void);

uint32_t rdcu_get_rdcu_status_board_serial_number(void);
uint32_t rdcu_get_rdcu_status_fpga_core_power_good(void);
uint32_t rdcu_get_rdcu_status_core_power_good(void);
uint32_t rdcu_get_rdcu_status_io_power_good(void);
uint32_t rdcu_get_rdcu_status_reset_by_register(void);
uint32_t rdcu_get_rdcu_status_power_on_reset(void);

uint8_t rdcu_get_rmap_target_logical_address(void);
uint8_t rdcu_get_rmap_target_cmd_key(void);
uint32_t rdcu_get_lvds_link_enabled(uint32_t link);

uint16_t rdcu_get_spw_empty_pckt_cnt(void);
uint8_t  rdcu_get_spw_run_clk_div(void);
uint8_t  rdcu_get_spw_lnk_run_state(void);

uint8_t rdcu_get_spw_lnk_credit_errs(void);
uint8_t rdcu_get_spw_lnk_escape_errs(void);
uint8_t rdcu_get_spw_lnk_parity_errs(void);
uint8_t rdcu_get_spw_lnk_disconnect_errs(void);

uint8_t rdcu_get_rmap_last_error_user_code(void);
uint8_t rdcu_get_rmap_last_error_standard_code(void);

uint8_t rdcu_get_rmap_incomplete_hdrs(void);
uint8_t rdcu_get_rmap_recv_reply_pckts(void);
uint8_t rdcu_get_recv_non_rmap_pckts(void);

uint8_t rdcu_get_rmap_pckt_errs(void);
uint8_t rdcu_get_rmap_oper_errs(void);
uint8_t rdcu_get_rmap_cmd_auth_errs(void);
uint8_t rdcu_get_rmap_hdr_errs(void);

uint16_t rdcu_get_adc_value(int id);

uint32_t rdcu_get_valid_adc_values(void);
uint32_t rdcu_get_adc_logic_reset(void);
uint32_t rdcu_get_adc_logic_enabled(void);


uint32_t rdcu_get_rdcu_interrupt_enabled(void);
uint32_t rdcu_get_compr_status_valid(void);
uint32_t rdcu_get_data_compr_ready(void);
uint32_t rdcu_get_data_compr_interrupted(void);
uint32_t rdcu_get_data_compr_active(void);


void rdcu_set_rdcu_board_reset_keyword(uint8_t key);
void rdcu_set_rdcu_bus_reset(void);
void rdcu_clear_rdcu_bus_reset(void);
void rdcu_set_rdcu_rmap_error_cntr_reset(void);
void rdcu_clear_rdcu_rmap_error_cntr_reset(void);
void rdcu_set_rdcu_spw_error_cntr_reset(void);
void rdcu_clear_rdcu_spw_error_cntr_reset(void);
void rdcu_set_rdcu_board_reset(void);
void rdcu_clear_rdcu_board_reset(void);

int rdcu_set_spw_link_run_clkdiv(uint8_t divisor);

int rdcu_set_lvds_link_enabled(uint32_t link);
int rdcu_set_lvds_link_disabled(uint32_t link);


void rdcu_set_rmap_target_logical_address(uint8_t addr);
void rdcu_set_rmap_target_cmd_key(uint8_t key);

void rdcu_set_adc_logic_reset(void);
void rdcu_clear_adc_logic_reset(void);
void rdcu_set_adc_logic_enabled(void);
void rdcu_set_adc_logic_disabled(void);

void rdcu_set_rdcu_interrupt(void);
void rdcu_clear_rdcu_interrupt(void);
void rdcu_set_data_compr_interrupt(void);
void rdcu_clear_data_compr_interrupt(void);
void rdcu_set_data_compr_start(void);
void rdcu_clear_data_compr_start(void);



/* Data Compressor registers */
int rdcu_set_noise_bits_rounded(uint32_t rpar);
int rdcu_set_weighting_param(uint32_t mval);
int rdcu_set_compression_mode(uint32_t cmode);

int rdcu_set_spillover_threshold(uint32_t spill);
int rdcu_set_golomb_param(uint32_t gpar);

int rdcu_set_adaptive_1_spillover_threshold(uint32_t spill);
int rdcu_set_adaptive_1_golomb_param(uint32_t gpar);

int rdcu_set_adaptive_2_spillover_threshold(uint32_t spill);
int rdcu_set_adaptive_2_golomb_param(uint32_t gpar);

int rdcu_set_data_start_addr(uint32_t addr);

int rdcu_set_model_start_addr(uint32_t addr);

int rdcu_set_num_samples(uint32_t samples);

int rdcu_set_new_model_start_addr(uint32_t addr);

int rdcu_set_compr_data_buf_start_addr(uint32_t addr);

int rdcu_set_compr_data_buf_len(uint32_t samples);


uint32_t rdcu_get_compression_mode(void);
uint32_t rdcu_get_noise_bits_rounded(void);
uint32_t rdcu_get_weighting_param(void);

uint32_t rdcu_get_spillover_threshold(void);
uint32_t rdcu_get_golomb_param(void);

uint32_t rdcu_get_compr_data_start_addr(void);

uint32_t rdcu_get_compr_data_size_bit(void);
uint32_t rdcu_get_compr_data_size_byte(void);

uint32_t rdcu_get_compr_data_adaptive_1_size_bit(void);
uint32_t rdcu_get_compr_data_adaptive_1_size_byte(void);

uint32_t rdcu_get_compr_data_adaptive_2_size_bit(void);
uint32_t rdcu_get_compr_data_adaptive_2_size_byte(void);

uint16_t rdcu_get_compr_error(void);

uint32_t rdcu_get_new_model_addr_used(void);

uint32_t rdcu_get_samples_used(void);

/* SRAM EDAC registers */
int rdcu_edac_set_sub_chip_die_addr(uint32_t ca);
void rdcu_edac_set_ctrl_reg_read_op(void);
void rdcu_edac_set_ctrl_reg_write_op(void);
void rdcu_edac_set_bypass(void);
void rdcu_edac_clear_bypass(void);
void rdcu_edac_set_scrub_info(uint8_t nfo);


uint32_t rdcu_edac_get_sub_chip_die_addr(void);
uint32_t rdcu_edac_get_bypass_status(void);
uint8_t rdcu_edac_get_scrub_info(void);

/* SRAM */
int rdcu_read_sram(void *buf, uint32_t addr, uint32_t size);
int rdcu_write_sram(void *buf, uint32_t addr, uint32_t size);
int rdcu_write_sram_8(uint8_t *buf, uint32_t addr, uint32_t size);
int rdcu_write_sram_16(uint16_t *buf, uint32_t addr, uint32_t size);
int rdcu_write_sram_32(uint32_t *buf, uint32_t addr, uint32_t size);


int rdcu_ctrl_init(void);

#endif /* RDCU_CTRL_H */
