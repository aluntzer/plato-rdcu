/**
 * @file   rdcu_ctrl.c
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
 * @brief RMAP RDCU control library
 * @see FPGA Requirement Specification PLATO-IWF-PL-RS-005 Issue 0.7
 *
 * USAGE:
 *	All sync() calls respect the direction of the sync, i.e. read-only
 *	registers in the RDCU are synced to the local mirror
 *	and vice-versa for write-only register
 *	The only exception are SRAM (data) related syncs, they specifiy
 *	the direction by a directional suffix, which is either _icu_rdcu
 *	for ICU->RDCU, i.e. transfer local to remote, or _rdcu_icu for a
 *	read-back
 *
 *	Access to the local mirror is provided by _get or _set calls, so
 *	to configure a register in the RDCU, one would call the sequence:
 *
 *	set_register_xyz(someargument);
 *	sync_register_xyz();
 *
 *	while(rdcu_sync_status())
 *		wait_busy_or_do_something_else_your_choice();
 *
 *	[sync_comple]
 *
 *	and to read a register:
 *
 *	sync_register_uvw()
 *
 *	while(rdcu_sync_status())
 *		wait_busy_or_do_something_else_your_choice();
 *
 *	[sync_comple]
 *
 *	value = get_register_uvw();
 *
 *
 * WARNING: this has not been thoroughly tested and is in need of inspection
 *	    with regards to the specification, in order to locate any
 *	    register transcription errors or typos
 *	    In the PLATO-IWF-PL-RS-005 Issue 0.7 (at least the one I have),
 *	    register bits in tables and register figures are sometimes in
 *	    conflict. Only values from tables have been used here.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <byteorder.h>
#include <rmap.h>
#include <rdcu_cmd.h>
#include <rdcu_ctrl.h>
#include <rdcu_rmap.h>


static struct rdcu_mirror *rdcu;


/**
 * @brief get the 4 FPGA minor/major version digits
 * @see RDCU-FRS-FN-0522
 */

uint16_t rdcu_get_fpga_version(void)
{
	return (uint16_t) (rdcu->fpga_version & 0xFFFF);
}


/**
 * @brief get the RDCU board serial number
 * @see RDCU-FRS-FN-0532
 */

uint32_t rdcu_get_rdcu_status_board_serial_number(void)
{
	return (rdcu->rdcu_status >> 12) & 0xFUL;
}


/**
 * @brief get FPGA core power good bit
 * @see RDCU-FRS-FN-0532
 *
 * @returns 0 : FPGA core power supply failure
 *	    1 : FPGA core power supply (1.5V) is good
 */

uint32_t rdcu_get_rdcu_status_fpga_core_power_good(void)
{
	return (rdcu->rdcu_status >> 6) & 0x1UL;
}


/**
 * @brief get core power good bit
 * @see RDCU-FRS-FN-0532
 *
 * @returns 0 : FPGA core power supply failure
 *	    1 : FPGA core power supply (1.8V) is good
 */

uint32_t rdcu_get_rdcu_status_core_power_good(void)
{
	return (rdcu->rdcu_status >> 5) & 0x1UL;
}


/**
 * @brief get i/o power good bit
 * @see RDCU-FRS-FN-0532
 *
 * @returns 0 : FPGA core power supply failure
 *	    1 : FPGA core power supply (3.3V) is good
 */

uint32_t rdcu_get_rdcu_status_io_power_good(void)
{
	return (rdcu->rdcu_status >> 4) & 0x1UL;
}


/**
 * @brief get reset by register bit
 * @see RDCU-FRS-FN-0532
 *
 * @returns 0 : not the reset reason
 *	    1 : reset triggered by register bit
 */

uint32_t rdcu_get_rdcu_status_reset_by_register(void)
{
	return (rdcu->rdcu_status >> 1) & 0x1UL;
}


/**
 * @brief get power on reset bit
 * @see RDCU-FRS-FN-0532
 *
 * @returns 0 : not the reset reason
 *	    1 : reset triggered by power on
 */

uint32_t rdcu_get_rdcu_status_power_on_reset(void)
{
	return rdcu->rdcu_status & 0x1UL;
}


/**
 * @brief get RMAP Target logical address
 * @see RDCU-FRS-FN-0542
 */

uint8_t rdcu_get_rmap_target_logical_address(void)
{
	return (uint8_t) ((rdcu->lvds_core_status >> 24) & 0xFFUL);
}


/**
 * @brief get RMAP Target command key
 * @see RDCU-FRS-FN-0542
 */

uint8_t rdcu_get_rmap_target_cmd_key(void)
{
	return (uint8_t) ((rdcu->lvds_core_status >> 16) & 0xFFUL);
}


/**
 * @brief get LVDS link enabled bit
 *
 * @param link the link number (0-7)
 *
 * @see RDCU-FRS-FN-0542
 *
 * @returns 0 : LVDS link disabled
 *	    1 : LVDS link enabled
 *	    other: invalid link argument
 */

uint32_t rdcu_get_lvds_link_enabled(uint32_t link)
{
	if (link > 7)
		return -1;

	return (rdcu->lvds_core_status >> link) & 0x1UL;
}


/**
 * @brief get SpW empty packet count
 * @see RDCU-FRS-FN-0552
 */

uint16_t rdcu_get_spw_empty_pckt_cnt(void)
{
	return (uint16_t) ((rdcu->spw_link_status >> 16) & 0xFFUL);
}


/**
 * @brief get SpW run-state clock divisor value
 * @see RDCU-FRS-FN-0552
 */

uint8_t rdcu_get_spw_run_clk_div(void)
{
	return (uint8_t) ((rdcu->spw_link_status >> 8) & 0x3FUL);
}


/**
 * @brief get SpW link run state
 * @see RDCU-FRS-FN-0552
 *
 * @returns 0 : SpW link not established
 *	    1 : SpW link is running
 */

uint8_t rdcu_get_spw_lnk_run_state(void)
{
	return (uint8_t) ((rdcu->spw_link_status >> 8) & 0x1UL);
}


/**
 * @brief get SpW link credit errors
 * @see RDCU-FRS-FN-0562
 */

uint8_t rdcu_get_spw_lnk_credit_errs(void)
{
	return (uint8_t) ((rdcu->spw_link_status >> 24) & 0xFFUL);
}


/**
 * @brief get SpW link escape errors
 * @see RDCU-FRS-FN-0562
 */

uint8_t rdcu_get_spw_lnk_escape_errs(void)
{
	return (uint8_t) ((rdcu->spw_link_status >> 16) & 0xFFUL);
}


/**
 * @brief get SpW link parity errors
 * @see RDCU-FRS-FN-0562
 */

uint8_t rdcu_get_spw_lnk_parity_errs(void)
{
	return (uint8_t) ((rdcu->spw_link_status >> 8) & 0xFFUL);
}


/**
 * @brief get SpW link disconnect errors
 * @see RDCU-FRS-FN-0562
 */

uint8_t rdcu_get_spw_lnk_disconnect_errs(void)
{
	return (uint8_t) (rdcu->spw_link_status & 0xFFUL);
}


/**
 * @brief get RMAP last error user code
 * @see RDCU-FRS-FN-0572
 */

uint8_t rdcu_get_rmap_last_error_user_code(void)
{
	return (uint8_t) ((rdcu->rmap_last_err >> 8) & 0xFFUL);
}


/**
 * @brief get RMAP last error standard code
 * @see RDCU-FRS-FN-0572
 */

uint8_t rdcu_get_rmap_last_error_standard_code(void)
{
	return (uint8_t) (rdcu->rmap_last_err & 0xFFUL);
}


/**
 * @brief get RMAP incomplete header error counter
 * @see RDCU-FRS-FN-0582
 */

uint8_t rdcu_get_rmap_incomplete_hdrs(void)
{
	return (uint8_t) ((rdcu->rmap_no_reply_err_cntrs >> 24) & 0xFFUL);
}


/**
 * @brief get RMAP received reply packet counter
 * @see RDCU-FRS-FN-0582
 */

uint8_t rdcu_get_rmap_recv_reply_pckts(void)
{
	return (uint8_t) ((rdcu->rmap_no_reply_err_cntrs >> 8) & 0xFFUL);
}


/**
 * @brief get received non-RMAP packet counter
 * @see RDCU-FRS-FN-0582
 */

uint8_t rdcu_get_recv_non_rmap_pckts(void)
{
	return (uint8_t) (rdcu->rmap_no_reply_err_cntrs & 0xFFUL);
}


/**
 * @brief get RMAP packet with length or content error counter
 * @see RDCU-FRS-FN-0592
 */

uint8_t rdcu_get_rmap_pckt_errs(void)
{
	return (uint8_t) ((rdcu->rmap_pckt_err_cntrs >> 24) & 0xFFUL);
}


/**
 * @brief get RMAP operation error counter
 * @see RDCU-FRS-FN-0592
 */

uint8_t rdcu_get_rmap_oper_errs(void)
{
	return (uint8_t) ((rdcu->rmap_pckt_err_cntrs >> 16) & 0xFFUL);
}


/**
 * @brief get RMAP command authorization error counter
 * @see RDCU-FRS-FN-0592
 */

uint8_t rdcu_get_rmap_cmd_auth_errs(void)
{
	return (uint8_t) ((rdcu->rmap_pckt_err_cntrs >> 8) & 0xFFUL);
}


/**
 * @brief get RMAP header error counter
 * @see RDCU-FRS-FN-0592
 */

uint8_t rdcu_get_rmap_hdr_errs(void)
{
	return (uint8_t) (rdcu->rmap_pckt_err_cntrs & 0xFFUL);
}


/**
 * @brief get an ADC value
 * @param id the ADC value id to get (1-8)
 * @see RDCU-FRS-FN-0602
 *
 * @return adc value, if 0xFFFF, id may be invalid
 */

uint16_t rdcu_get_adc_value(int id)
{
	switch (id) {
	case 1:
		return (uint16_t)  (rdcu->adc_values_1        & 0xFFFFUL);
	case 2:
		return (uint16_t) ((rdcu->adc_values_1 >> 16) & 0xFFFFUL);
	case 3:
		return (uint16_t)  (rdcu->adc_values_2        & 0xFFFFUL);
	case 4:
		return (uint16_t) ((rdcu->adc_values_2 >> 16) & 0xFFFFUL);
	case 5:
		return (uint16_t)  (rdcu->adc_values_3        & 0xFFFFUL);
	case 6:
		return (uint16_t) ((rdcu->adc_values_3 >> 16) & 0xFFFFUL);
	case 7:
		return (uint16_t)  (rdcu->adc_values_4        & 0xFFFFUL);
	case 8:
		return (uint16_t) ((rdcu->adc_values_4 >> 16) & 0xFFFFUL);

	default:
		break;
	}

	return 0xFFFF;
}


/**
 * @brief get valid ADC values
 * @see RDCU-FRS-FN-0612
 *
 * @returns 0: no valid ADC values in the ADC value registers
 *	    1: the ADC value registers contain valid ADC results
 */

uint32_t rdcu_get_valid_adc_values(void)
{
	return ((rdcu->adc_status >> 4) & 0x1UL);
}


/**
 * @brief get ADC logic reset
 * @see RDCU-FRS-FN-0612
 *
 * @returns 0: normal ADC operation
 *	    1: ADC logic is in reset state
 */

uint32_t rdcu_get_adc_logic_reset(void)
{
	return ((rdcu->adc_status >> 1) & 0x1UL);
}


/**
 * @brief get ADC logic enabled
 * @see RDCU-FRS-FN-0612
 *
 * @returns 0: ADC logic disabled
 *	    1: ADC logic enabled (normal operation)
 */

uint32_t rdcu_get_adc_logic_enabled(void)
{
	return (rdcu->adc_status & 0x1UL);
}


/**
 * @brief get RDCU Interrupt status
 * @see RDCU-FRS-FN-0632
 *
 * @returns 0: Interrupt is disabled
 *	    1: Interrupt is enabled
 */

uint32_t rdcu_get_rdcu_interrupt_enabled(void)
{
	return ((rdcu->compr_status >> 8) & 0x1UL);
}


/**
 * @brief get compressor status valid
 * @see RDCU-FRS-FN-0632
 *
 * @returns 0: Data is invalid
 *	    1: Data is valid
 */

uint32_t rdcu_get_compr_status_valid(void)
{
	return ((rdcu->compr_status >> 5) & 0x1UL);
}


/**
 * @brief get data compressor ready
 * @see RDCU-FRS-FN-0632
 *
 * @returns 0: Compressor is busy
 *	    1: Compressor is ready
 */

uint32_t rdcu_get_data_compr_ready(void)
{
	return ((rdcu->compr_status >> 4) & 0x1UL);
}


/**
 * @brief get data compressor interrupted
 * @see RDCU-FRS-FN-0632
 *
 * @returns 0: No compressor interruption
 *	    1: Compressor was interrupted
 */

uint32_t rdcu_get_data_compr_interrupted(void)
{
	return ((rdcu->compr_status >> 1) & 0x1UL);
}


/**
 * @brief get data compressor activce
 * @see RDCU-FRS-FN-0632
 *
 * @returns 0: Compressor is on hold
 *	    1: Compressor is active
 */

uint32_t rdcu_get_data_compr_active(void)
{
	return (rdcu->compr_status & 0x1UL);
}


/**
 * @brief set RDCU Board Reset Keyword
 * @see RDCU-FRS-FN-0662
 *
 * @note the valid key is 0x9A
 */

void rdcu_set_rdcu_board_reset_keyword(uint8_t key)
{
	/* clear and set */
	rdcu->rdcu_reset &= ~(0xFUL << 24);
	rdcu->rdcu_reset |= ((uint32_t) key << 24);
}


/**
 * @brief set RDCU Internal Bus Reset bit
 * @see RDCU-FRS-FN-0662
 *
 * @note The bit will auto-clear in the FPGA, to clear the local mirror,
 *	 make sure to rdcu_clear_rdcu_bus_reset() so the FPGA internal bus
 *	 is not reset every time rdcu_sync_rdcu_reset() is called, as
 *	 write-only registers are note synced back from the RDCU
 */

void rdcu_set_rdcu_bus_reset(void)
{
	rdcu->rdcu_reset |= (0x1UL << 12);
}


/**
 * @brief clear RDCU Internal Bus Reset bit
 * @see RDCU-FRS-FN-0662
 */

void rdcu_clear_rdcu_bus_reset(void)
{
	rdcu->rdcu_reset &= ~(0x1UL << 12);
}


/**
 * @brief set RDCU RMAP error counter Reset bit
 * @see RDCU-FRS-FN-0662
 *
 * @note The bit will auto-clear in the FPGA, to clear the local mirror,
 *	 make sure to rdcu_clear_rdcu_rmap_error_cntr_reset() so the FPGA
 *	 does not reset the RMAP error counter every time rdcu_sync_rdcu_reset()
 *	 is called, as write-only registers are not synced back from the RDCU.
 */

void rdcu_set_rdcu_rmap_error_cntr_reset(void)
{
	rdcu->rdcu_reset |= (0x1UL << 9);
}


/**
 * @brief clear RDCU RMAP error counter Reset bit
 * @see RDCU-FRS-FN-0662
 */

void rdcu_clear_rdcu_rmap_error_cntr_reset(void)
{
	rdcu->rdcu_reset &= ~(0x1UL << 9);
}


/**
 * @brief set RDCU SpaceWire error counter Reset bit
 * @see RDCU-FRS-FN-0662
 *
 * @note The bit will auto-clear in the FPGA, to clear the local mirror,
 *	 make sure to rdcu_clear_rdcu_spw_error_cntr_reset() so the FPGA
 *	 does not reset the SpW error counter every time rdcu_sync_rdcu_reset()
 *	 is called, as write-only registers are not synced back from the RDCU.
 */

void rdcu_set_rdcu_spw_error_cntr_reset(void)
{
	rdcu->rdcu_reset |= (0x1UL << 8);
}


/**
 * @brief clear RDCU SpaceWire error counter Reset bit
 * @see RDCU-FRS-FN-0662
 */

void rdcu_clear_rdcu_spw_error_cntr_reset(void)
{
	rdcu->rdcu_reset &= ~(0x1UL << 8);
}


/**
 * @brief set RDCU Board Reset bit
 * @see RDCU-FRS-FN-0662
 *
 * @note The bit will auto-clear in the FPGA, to clear the local mirror,
 *	 make sure to rdcu_clear_rdcu_board_reset() so the FPGA
 *	 does not reset the SpW error counter every time rdcu_sync_rdcu_reset()
 *	 is called, as write-only registers are not synced back from the RDCU.
 */

void rdcu_set_rdcu_board_reset(void)
{
	rdcu->rdcu_reset |= (0x1UL << 1);
}


/**
 * @brief clear RDCU SpaceWire error counter Reset bit
 * @see RDCU-FRS-FN-0662
 */

void rdcu_clear_rdcu_board_reset(void)
{
	rdcu->rdcu_reset &= ~(0x1UL << 1);
}


/**
 * @brief set SpW Link Control Run-State Clock Divisor
 * @see RDCU-FRS-FN-0672
 *
 * @note value is scaling factor minus 1
 */

int rdcu_set_spw_link_run_clkdiv(uint8_t div)
{
	if (div > 49)
		return -1;

	/* clear and set */
	rdcu->spw_link_ctrl &= ~(0x3FUL << 8);
	rdcu->spw_link_ctrl |= ((uint32_t) div << 8);

	return 0;
}


/**
 * @brief set LVDS link enabled
 *
 * @param link the link number (0-7)
 *
 * @see RDCU-FRS-FN-0682
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_lvds_link_enabled(uint32_t link)
{
	if (link > 7)
		return -1;

	rdcu->lvds_ctrl |= (0x1UL << link);

	return 0;
}


/**
 * @brief set LVDS link disabled
 *
 * @param link the link number (0-7)
 *
 * @see RDCU-FRS-FN-0682
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_lvds_link_disabled(uint32_t link)
{
	if (link > 7)
		return -1;

	rdcu->lvds_ctrl &= ~((0x1UL << link));

	return 0;
}


/**
 * @brief set RMAP Target logical address
 * @see RDCU-FRS-FN-0692
 */

void rdcu_set_rmap_target_logical_address(uint8_t addr)
{
	rdcu->core_ctrl &= ~(0xFFUL << 24);
	rdcu->core_ctrl |= ((uint32_t) addr) << 24;
}


/**
 * @brief set RMAP Target command key
 * @see RDCU-FRS-FN-0692
 */

void rdcu_set_rmap_target_cmd_key(uint8_t key)
{
	rdcu->core_ctrl &= ~(0xFFUL << 16);
	rdcu->core_ctrl |= ((uint32_t) key) << 16;
}


/**
 * @brief set the ADC logic reset bit
 * @see RDCU-FRS-FN-0712
 *
 * @note use rdcu_clear_adc_logic_reset(), rdcu_sync_adc_ctrl() sequence
 *	 to clear and start normal operation
 */

void rdcu_set_adc_logic_reset(void)
{
	rdcu->adc_ctrl |= (0x1UL << 1);
}


/**
 * @brief clear the ADC logic reset bit
 * @see RDCU-FRS-FN-0712
 */

void rdcu_clear_adc_logic_reset(void)
{
	rdcu->adc_ctrl &= ~(0x1UL << 1);
}


/**
 * @brief set the ADC logic enabled
 * @see RDCU-FRS-FN-0712
 */

void rdcu_set_adc_logic_enabled(void)
{
	rdcu->adc_ctrl |= 0x1UL;
}


/**
 * @brief set the ADC logic disabled
 * @see RDCU-FRS-FN-0712
 */

void rdcu_set_adc_logic_disabled(void)
{
	rdcu->adc_ctrl &= ~0x1UL;
}


/**
 * @brief enable RDCU interrupt signal to the ICU
 * @see RDCU-FRS-FN-0732
 */

void rdcu_set_rdcu_interrupt(void)
{
	rdcu->compr_ctrl |= (0x1UL << 8);
}


/**
 * @brief disable RDCU interrupt signal to the ICU
 * @see RDCU-FRS-FN-0732
 */

void rdcu_clear_rdcu_interrupt(void)
{
	rdcu->compr_ctrl &= ~(0x1UL << 8);
}


/**
 * @brief set data compressor interrupt
 * @see RDCU-FRS-FN-0732
 *
 * @note The bit will auto-clear in the FPGA once compression is complete.
 *	 To clear the local mirror, make sure to
 *	 rdcu_clear_data_compr_interrupt() so the FPGA does not interrupt
 *	 data compression unexepectedly when rdcu_sync_compr_ctrl()
 *	 is called, as write-only registers are not synced back from the RDCU.
 *
 */

void rdcu_set_data_compr_interrupt(void)
{
	rdcu->compr_ctrl |= (0x1UL << 1);
}


/**
 * @brief clear data compressor interrupt
 * @see RDCU-FRS-FN-0732
 */

void rdcu_clear_data_compr_interrupt(void)
{
	rdcu->compr_ctrl &= ~(0x1UL << 1);
}


/**
 * @brief set data compressor start bit
 * @see RDCU-FRS-FN-0732
 *
 * @note The bit will auto-clear in the FPGA once compression is complete.
 *	 To clear the local mirror, make sure to
 *	 rdcu_clear_data_compr_start() so the FPGA does not start
 *	 data compression unexepectedly when rdcu_sync_compr_ctrl()
 *	 is called, as write-only registers are not synced back from the RDCU.
 */

void rdcu_set_data_compr_start(void)
{
	rdcu->compr_ctrl |= 0x1UL;
}


/**
 * @brief clear data compressor start bit
 * @see RDCU-FRS-FN-0732
 */

void rdcu_clear_data_compr_start(void)
{
	rdcu->compr_ctrl &= ~0x1UL;
}


/**
 * @brief set number of noise bits to be rounded
 * @see RDCU-FRS-FN-0772
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_noise_bits_rounded(uint32_t rpar)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (rpar > 3)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->compressor_param1 &= ~(0x3UL << 16);
	rdcu->compressor_param1 |=  (rpar << 16);

	return 0;
}


/**
 * @brief set weighting parameter
 * @see RDCU-FRS-FN-0772
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_weighting_param(uint32_t mval)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (mval > 16)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->compressor_param1 &= ~(0x1FUL << 8);
	rdcu->compressor_param1 |=  (mval << 8);

	return 0;
}


/**
 * @brief set compression mode
 * @see RDCU-FRS-FN-0772
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_compression_mode(uint32_t cmode)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (cmode > 4)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->compressor_param1 &= ~0xFFUL;
	rdcu->compressor_param1 |= cmode;

	return 0;
}


/**
 * @brief set spillover threshold for encoding outliers
 * @see RDCU-FRS-FN-0782
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_spillover_threshold(uint32_t spill)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (spill < 2)
		return -1;

	if (spill > 16383)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->compressor_param2 &= ~(0x3FFFUL << 8);
	rdcu->compressor_param2 |=  (spill    << 8);

	return 0;
}


/**
 * @brief set Golomb parameter for dictionary selection
 * @see RDCU-FRS-FN-0782
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_golomb_param(uint32_t gpar)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (!gpar)
		return -1;

	if (gpar > 63)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->compressor_param2 &= ~0x3FUL;
	rdcu->compressor_param2 |= gpar;

	return 0;
}


/**
 * @brief set adaptive 1 spillover threshold for encoding outliers
 * @see RDCU-FRS-FN-0792
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_adaptive_1_spillover_threshold(uint32_t spill)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (spill < 2)
		return -1;

	if (spill > 16383)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->adaptive_param1 &= ~(0x3FFFUL << 8);
	rdcu->adaptive_param1 |=  (spill    << 8);

	return 0;
}


/**
 * @brief set adaptive 1 Golomb parameter for dictionary selection
 * @see RDCU-FRS-FN-0792
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_adaptive_1_golomb_param(uint32_t gpar)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (!gpar)
		return -1;

	if (gpar > 63)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->adaptive_param1 &= ~0x3FUL;
	rdcu->adaptive_param1 |= gpar;

	return 0;
}



/**
 * @brief set adaptive 2 spillover threshold for encoding outliers
 * @see RDCU-FRS-FN-0802
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_adaptive_2_spillover_threshold(uint32_t spill)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (spill < 2)
		return -1;

	if (spill > 16383)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->adaptive_param2 &= ~(0x3FFFUL << 8);
	rdcu->adaptive_param2 |=  (spill    << 8);

	return 0;
}


/**
 * @brief set adaptive 2 Golomb parameter for dictionary selection
 * @see RDCU-FRS-FN-0802
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_adaptive_2_golomb_param(uint32_t gpar)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (!gpar)
		return -1;

	if (gpar > 63)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->adaptive_param2 &= ~0x3FUL;
	rdcu->adaptive_param2 |= gpar;

	return 0;
}


/**
 * @brief set data start address
 * @see RDCU-FRS-FN-0812
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_data_start_addr(uint32_t addr)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (addr > 0x00FFFFFFUL)
		return -1;

	if (addr & 0x3)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->data_start_addr &= ~0x00FFFFFFUL;
	rdcu->data_start_addr |= addr;

	return 0;
}


/**
 * @brief set model start address
 * @see RDCU-FRS-FN-0822
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_model_start_addr(uint32_t addr)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (addr > 0x00FFFFFFUL)
		return -1;

	if (addr & 0x3)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->model_start_addr &= ~0x00FFFFFFUL;
	rdcu->model_start_addr |= addr;

	return 0;
}


/**
 * @brief set number of data samples (16 bit values) to compress
 * @see RDCU-FRS-FN-0832
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_num_samples(uint32_t samples)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (samples > 0x00FFFFFFUL)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->num_samples &= ~0x00FFFFFFUL;
	rdcu->num_samples |= samples;

	return 0;
}


/**
 * @brief set updated_model/new model start address
 * @see RDCU-FRS-FN-0842
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_new_model_start_addr(uint32_t addr)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (addr > 0x00FFFFFFUL)
		return -1;

	if (addr & 0x3)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/

	/* clear and set */
	rdcu->new_model_start_addr &= ~0x00FFFFFFUL;
	rdcu->new_model_start_addr |= addr;

	return 0;
}


/**
 * @brief set compressed data buffer start address
 * @see RDCU-FRS-FN-0850
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_compr_data_buf_start_addr(uint32_t addr)
{
#ifndef SKIP_CMP_PAR_CHECK
	if (addr > 0x00FFFFFFUL)
		return -1;

	if (addr & 0x3)
		return -1;
#endif /*SKIP_CMP_PAR_CHECK*/
	/* clear and set */
	rdcu->compr_data_buf_start_addr &= ~0x00FFFFFFUL;
	rdcu->compr_data_buf_start_addr |= addr;

	return 0;
}


/**
 * @brief set compressed data buffer length (in 16 bit values)
 * @see RDCU-FRS-FN-0862
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_set_compr_data_buf_len(uint32_t samples)
{
	if (samples > 0x00FFFFFFUL)
		return -1;

	/* clear and set */
	rdcu->compr_data_buf_len &= ~0x00FFFFFFUL;
	rdcu->compr_data_buf_len |= samples;

	return 0;
}


/**
 * @brief get compression mode
 * @see RDCU-FRS-FN-0892
 *
 * @returns the CMODE
 */

uint32_t rdcu_get_compression_mode(void)
{
	return rdcu->used_param1 & 0xFFUL;
}


/**
 * @brief get number of noise bits to be rounded
 * @see RDCU-FRS-FN-0892
 *
 * @returns the RPAR
 */

uint32_t rdcu_get_noise_bits_rounded(void)
{
	return (rdcu->used_param1 >> 16) & 0x3UL;
}


/**
 * @brief get weighting parameter
 * @see RDCU-FRS-FN-0892
 *
 * @returns the RPAR
 */

uint32_t rdcu_get_weighting_param(void)
{
	return (rdcu->used_param1 >> 8) & 0x1FUL;
}


/**
 * @brief get spillover threshold for encoding outliers
 * @see RDCU-FRS-FN-0902
 *
 * @returns the SPILL
 */

uint32_t rdcu_get_spillover_threshold(void)
{
	return (rdcu->used_param2 >> 8) & 0x3FFFUL;
}


/**
 * @brief get spillover threshold for encoding outliers
 * @see RDCU-FRS-FN-0902
 *
 * @returns the GPAR
 */

uint32_t rdcu_get_golomb_param(void)
{
	return rdcu->used_param2 & 0x3FUL;
}


/**
 * @brief get compressed data start address
 * @see RDCU-FRS-FN-0912
 *
 * @returns the output SRAM address
 */

uint32_t rdcu_get_compr_data_start_addr(void)
{
	return rdcu->compr_data_start_addr & 0x00FFFFFFUL;
}


/**
 * @brief get the need bytes for the given bits
 *
 * @param cmp_size_bit compressed data size, measured in bits
 *
 * @returns the size in bytes to store the compressed data
 * @note we round up the result to multiples of 4 bytes
 */

static uint32_t rdcu_bit_to_4byte(unsigned int cmp_size_bit)
{
	return (((cmp_size_bit + 7) / 8) + 3) & ~0x3UL;
}


/**
 * @brief get compressed data size in bits
 * @see RDCU-FRS-FN-0922
 *
 * @returns the compressed data size in bits
 */

uint32_t rdcu_get_compr_data_size_bit(void)
{
	return rdcu->compr_data_size;
}


/**
 * @brief get compressed data size in bytes
 * @see RDCU-FRS-FN-0922
 *
 * @returns the compressed data size in bytes
 */

uint32_t rdcu_get_compr_data_size_byte(void)
{
	return rdcu_bit_to_4byte(rdcu_get_compr_data_size_bit());
}


/**
 * @brief get compressed data adaptive 1 size in bits
 * @see RDCU-FRS-FN-0932
 *
 * @returns the adaptive 1 compressed data size in bits
 */

uint32_t rdcu_get_compr_data_adaptive_1_size_bit(void)
{
	return rdcu->compr_data_adaptive_1_size;
}


/**
 * @brief get compressed data adaptive 1 size in bytes
 * @see RDCU-FRS-FN-0932
 *
 * @returns the adaptive 1 compressed data size in bytes
 */

uint32_t rdcu_get_compr_data_adaptive_1_size_byte(void)
{
	return rdcu_bit_to_4byte(rdcu_get_compr_data_adaptive_1_size_bit());
}


/**
 * @brief get compressed data adaptive 2 size in bits
 * @see RDCU-FRS-FN-0942
 *
 * @returns the adaptive 2 compressed data size in bits
 */

uint32_t rdcu_get_compr_data_adaptive_2_size_bit(void)
{
	return rdcu->compr_data_adaptive_2_size;
}


/**
 * @brief get compressed data adaptive 2 size in bytes
 * @see RDCU-FRS-FN-0942
 *
 * @returns the adaptive 2 compressed data size in bytes
 */

uint32_t rdcu_get_compr_data_adaptive_2_size_byte(void)
{
	return rdcu_bit_to_4byte(rdcu_get_compr_data_adaptive_2_size_bit());
}


/**
 * @brief get compression error code
 * @see RDCU-FRS-FN-0954
 *
 * @returns the compression error code
 */

uint16_t rdcu_get_compr_error(void)
{
	return (uint16_t) (rdcu->compr_error & 0x3FFUL);
}


/**
 * @brief get model info start address
 * @see RDCU-FRS-FN-0960
 *
 */

uint32_t rdcu_get_new_model_addr_used(void)
{
	return rdcu->new_model_addr_used & 0x00FFFFFFUL;
}


/**
 * @brief get model info length
 * @see RDCU-FRS-FN-0972
 *
 * @returns the number of 16-bit samples in the model
 */

uint32_t rdcu_get_samples_used(void)
{
	return rdcu->samples_used & 0x00FFFFFFUL;
}


/**
 * @brief set EDAC sub chip die address
 * @see RDCU-FRS-FN-1012
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_edac_set_sub_chip_die_addr(uint32_t ca)
{
	if (ca > 0xFUL)
		return -1;

	rdcu->sram_edac_ctrl &= ~(0xFUL << 12);
	rdcu->sram_edac_ctrl |=  (ca    << 12);

	return 0;
}


/**
 * @brief set EDAC control register read operation
 * @see RDCU-FRS-FN-1012
 */

void rdcu_edac_set_ctrl_reg_read_op(void)
{
	rdcu->sram_edac_ctrl |= (0x1UL << 9);
}


/**
 * @brief set EDAC control register write operation
 * @see RDCU-FRS-FN-1012
 */

void rdcu_edac_set_ctrl_reg_write_op(void)
{
	rdcu->sram_edac_ctrl &= ~(0x1UL << 9);
}


/**
 * @brief set EDAC to bypass
 * @see RDCU-FRS-FN-1012
 */

void rdcu_edac_set_bypass(void)
{
	rdcu->sram_edac_ctrl |= (0x1UL << 8);
}


/**
 * @brief set EDAC to normal operation
 * @see RDCU-FRS-FN-1012
 */

void rdcu_edac_clear_bypass(void)
{
	rdcu->sram_edac_ctrl &= ~(0x1UL << 8);
}


/**
 * @brief set EDAC SRAM scrubbing information
 * @see RDCU-FRS-FN-1012
 */

void rdcu_edac_set_scrub_info(uint8_t nfo)
{
	rdcu->sram_edac_ctrl &= ~0xFFUL;
	rdcu->sram_edac_ctrl |= (uint32_t) nfo & 0xFFUL;
}


/**
 * @brief get EDAC sub chip die address
 * @see RDCU-FRS-FN-1032
 */

uint32_t rdcu_edac_get_sub_chip_die_addr(void)
{
	return (rdcu->sram_edac_status >> 12) & 0xFUL;
}


/**
 * @brief get EDAC bypass status
 * @see RDCU-FRS-FN-1032
 *
 * @returns 0: normal EDAC operation
 *	    1: EDAC function will be bypassed
 */

uint32_t rdcu_edac_get_bypass_status(void)
{
	return (rdcu->sram_edac_status >> 8) & 0x1UL;
}


/**
 * @brief get EDAC SRAM scrubbing information
 * @see RDCU-FRS-FN-1032
 */

uint8_t rdcu_edac_get_scrub_info(void)
{
	return (uint8_t) (rdcu->sram_edac_ctrl & 0xFFUL);
}


/**
 * @brief read data from the local SRAM mirror
 *
 * @param buf the buffer to read to (if NULL, the required size is returned)
 *
 * @param addr an address within the RDCU SRAM
 * @param size the number of bytes read
 *
 * @returns the number of bytes read, < 0 on error
 */

int rdcu_read_sram(void *buf, uint32_t addr, uint32_t size)
{
	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if (addr + size > RDCU_SRAM_END)
		return -1;

	if (buf)
		memcpy(buf, &rdcu->sram[addr], size);

	return (int)size; /* lol */
}


/**
 * @brief write arbitrary big-endian data to the local SRAM mirror
 *
 * @param buf the buffer to read from
 *
 * @param addr an address within the RDCU SRAM
 * @param size the number of bytes read
 *
 * @returns the number of bytes written, < 0 on error
 */

int rdcu_write_sram(void *buf, uint32_t addr, uint32_t size)
{
	if (!buf)
		return 0;

	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if (addr + size > RDCU_SRAM_END)
		return -1;

	if (buf)
		memcpy(&rdcu->sram[addr], buf, size);

	return (int)size; /* lol */
}


/**
 * @brief write uint8_t formatted data to the local SRAM mirror. (This function
 *	is endian-safe.)
 *
 * @param buf the buffer to read from
 *
 * @param addr an address within the RDCU SRAM
 * @param size the number of bytes read
 *
 * @returns the number of bytes written, < 0 on error
 */

int rdcu_write_sram_8(uint8_t *buf, uint32_t addr, uint32_t size)
{
	return rdcu_write_sram(buf, addr, size);
}


/**
 * @brief write uint16_t formatted data to the local SRAM mirror. This function
 *	is endian-safe.
 *
 * @param buf the buffer to read from
 *
 * @param addr an address within the RDCU SRAM
 * @param size the number of bytes read
 *
 * @returns the number of bytes written, < 0 on error
 */

int rdcu_write_sram_16(uint16_t *buf, uint32_t addr, uint32_t size)
{
	if (!buf)
		return 0;

	if (size & 0x1)
		return -1;

	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if (addr + size > RDCU_SRAM_END)
		return -1;

#if !(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	return rdcu_write_sram(buf, addr, size);
#else
	{
		uint32_t i;

		for (i = 0; i < size/sizeof(uint16_t); i++) {
			uint16_t *sram_buf = (uint16_t *)&rdcu->sram[addr];

			sram_buf[i] = cpu_to_be16(buf[i]);
		}
	}
	return (int)size; /* lol */
#endif /* __BYTE_ORDER__ */
}


/**
 * @brief write uint32_t formatted data to the local SRAM mirror. This function
 *	is endian-safe.
 *
 * @param buf the buffer to read from
 *
 * @param addr an address within the RDCU SRAM
 * @param size the number of bytes read
 *
 * @returns the number of bytes written, < 0 on error
 */

int rdcu_write_sram_32(uint32_t *buf, uint32_t addr, uint32_t size)
{
	if (!buf)
		return 0;

	if (size & 0x3)
		return -1;

	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if (addr + size > RDCU_SRAM_END)
		return -1;

#if !(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	return rdcu_write_sram(buf, addr, size);
#else
	{
		uint32_t i;

		for (i = 0; i < size/sizeof(uint32_t); i++) {
			uint32_t *sram_buf = (uint32_t *)&rdcu->sram[addr];

			sram_buf[i] = cpu_to_be32(buf[i]);
		}
	}
	return (int)size; /* lol */
#endif /* __BYTE_ORDER__ */
}


/**
 * @brief sync the FPGA version (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_fpga_version(void)
{
	return rdcu_sync(rdcu_read_cmd_fpga_version, &rdcu->fpga_version, 0);
}


/**
 * @brief sync the RDCU status register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_rdcu_status(void)
{
	return rdcu_sync(rdcu_read_cmd_rdcu_status, &rdcu->rdcu_status, 0);
}


/**
 * @brief sync the LVDS core status register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_lvds_core_status(void)
{
	return rdcu_sync(rdcu_read_cmd_lvds_core_status,
			 &rdcu->lvds_core_status, 0);
}


/**
 * @brief sync the SpW link status register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_spw_link_status(void)
{
	return rdcu_sync(rdcu_read_cmd_spw_link_status,
			 &rdcu->spw_link_status, 0);
}


/**
 * @brief sync the SpW Error Counter register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_spw_err_cntrs(void)
{
	return rdcu_sync(rdcu_read_cmd_spw_err_cntrs,
			 &rdcu->spw_err_cntrs, 0);
}


/**
 * @brief sync the RMAP Last Error register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_rmap_last_err(void)
{
	return rdcu_sync(rdcu_read_cmd_rmap_last_err,
			 &rdcu->rmap_last_err, 0);
}


/**
 * @brief sync the RMAP No-Reply Error Counter register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_rmap_no_reply_err_cntrs(void)
{
	return rdcu_sync(rdcu_read_cmd_rmap_no_reply_err_cntrs,
			 &rdcu->rmap_no_reply_err_cntrs, 0);
}


/**
 * @brief sync the RMAP Packet Error Counter register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_rmap_pckt_err_cntrs(void)
{
	return rdcu_sync(rdcu_read_cmd_rmap_pckt_err_cntrs,
			 &rdcu->rmap_pckt_err_cntrs, 0);
}



/**
 * @brief sync an ADC values register (read only)
 *
 * @param id the ADC value register id (1-4)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_adc_values(int id)
{
	switch (id) {
	case '1':
		return rdcu_sync(rdcu_read_cmd_adc_values_1,
				 &rdcu->adc_values_1, 0);
	case '2':
		return rdcu_sync(rdcu_read_cmd_adc_values_2,
				 &rdcu->adc_values_2, 0);
	case '3':
		return rdcu_sync(rdcu_read_cmd_adc_values_3,
				 &rdcu->adc_values_3, 0);
	case '4':
		return rdcu_sync(rdcu_read_cmd_adc_values_4,
				 &rdcu->adc_values_4, 0);
	default:
		break;
	}

	return -1;
}


/**
 * @brief sync the ADC Status register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_adc_status(void)
{
	return rdcu_sync(rdcu_read_cmd_adc_status,
			 &rdcu->adc_status, 0);
}


/**
 * @brief sync the Compressor Status register (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_status(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_status,
			 &rdcu->compr_status, 0);
}


/**
 * @brief sync the RDCU Reset register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_rdcu_reset(void)
{
	return rdcu_sync(rdcu_write_cmd_rdcu_reset,
			 &rdcu->rdcu_reset, 4);
}


/**
 * @brief sync the SpW Link Control register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_spw_link_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_spw_link_ctrl,
			 &rdcu->spw_link_ctrl, 4);
}

/**
 * @brief sync the LVDS Control register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_lvds_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_lvds_ctrl,
			 &rdcu->lvds_ctrl, 4);
}


/**
 * @brief sync the Core Control register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_core_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_core_ctrl,
			 &rdcu->core_ctrl, 4);
}


/**
 * @brief sync the ADC Control register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_adc_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_adc_ctrl,
			 &rdcu->adc_ctrl, 4);
}


/**
 * @brief sync the Compressor Control register (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_compr_ctrl,
			 &rdcu->compr_ctrl, 4);
}


/**
 * @brief sync the Compressor Parameter 1 (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compressor_param1(void)
{
	return rdcu_sync(rdcu_write_cmd_compressor_param1,
			 &rdcu->compressor_param1, 4);
}


/**
 * @brief sync the Compressor Parameter 2 (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compressor_param2(void)
{
	return rdcu_sync(rdcu_write_cmd_compressor_param2,
			 &rdcu->compressor_param2, 4);
}


/**
 * @brief sync the Adaptive Parameter 1 (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_adaptive_param1(void)
{
	return rdcu_sync(rdcu_write_cmd_adaptive_param1,
			 &rdcu->adaptive_param1, 4);
}


/**
 * @brief sync the Adaptive Parameter 2 (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_adaptive_param2(void)
{
	return rdcu_sync(rdcu_write_cmd_adaptive_param2,
			 &rdcu->adaptive_param2, 4);
}


/**
 * @brief sync the Data Start Address (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_data_start_addr(void)
{
	return rdcu_sync(rdcu_write_cmd_data_start_addr,
			 &rdcu->data_start_addr, 4);
}


/**
 * @brief sync the Model Start Address (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_model_start_addr(void)
{
	return rdcu_sync(rdcu_write_cmd_model_start_addr,
			 &rdcu->model_start_addr, 4);
}


/**
 * @brief sync the Number of Samples (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_num_samples(void)
{
	return rdcu_sync(rdcu_write_cmd_num_samples,
			 &rdcu->num_samples, 4);
}


/**
 * @brief sync the Model Start Address (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_new_model_start_addr(void)
{
	return rdcu_sync(rdcu_write_cmd_new_model_start_addr,
			 &rdcu->new_model_start_addr, 4);
}


/**
 * @brief sync the Compressed Data Buffer Start Address (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_buf_start_addr(void)
{
	return rdcu_sync(rdcu_write_cmd_compr_data_buf_start_addr,
			 &rdcu->compr_data_buf_start_addr, 4);
}


/**
 * @brief sync the Compressed Data Buffer Length (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_buf_len(void)
{
	return rdcu_sync(rdcu_write_cmd_compr_data_buf_len,
			 &rdcu->compr_data_buf_len, 4);
}


/**
 * @brief sync the Used Parameter 1 (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_used_param1(void)
{
	return rdcu_sync(rdcu_read_cmd_used_param1, &rdcu->used_param1, 0);
}


/**
 * @brief sync the Used Parameter 2 (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_used_param2(void)
{
	return rdcu_sync(rdcu_read_cmd_used_param2, &rdcu->used_param2, 0);
}


/**
 * @brief sync the Compressed Data Start Address (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_start_addr(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_data_start_addr,
			 &rdcu->compr_data_start_addr, 0);
}


/**
 * @brief sync the Compressed Data Start Size (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_size(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_data_size,
			 &rdcu->compr_data_size, 0);
}


/**
 * @brief sync the Compressed Data Adaptive 1 Size (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_adaptive_1_size(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_data_adaptive_1_size,
			 &rdcu->compr_data_adaptive_1_size, 0);
}


/**
 * @brief sync the Compressed Data Adaptive 2 Size (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_data_adaptive_2_size(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_data_adaptive_2_size,
			 &rdcu->compr_data_adaptive_2_size, 0);
}


/**
 * @brief sync the Compression Error (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_compr_error(void)
{
	return rdcu_sync(rdcu_read_cmd_compr_error,
			 &rdcu->compr_error, 0);
}


/**
 * @brief sync the Model Info Start Address (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_new_model_addr_used(void)
{
	return rdcu_sync(rdcu_read_cmd_new_model_addr_used,
			 &rdcu->new_model_addr_used, 0);
}


/**
 * @brief sync the Model Info Length (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_samples_used(void)
{
	return rdcu_sync(rdcu_read_cmd_samples_used,
			 &rdcu->samples_used, 0);
}


/**
 * @brief sync the SRAM EDAC Control (write only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_sram_edac_ctrl(void)
{
	return rdcu_sync(rdcu_write_cmd_sram_edac_ctrl,
			 &rdcu->sram_edac_ctrl, 4);
}


/**
 * @brief sync the SRAM EDAC Status (read only)
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_sram_edac_status(void)
{
	return rdcu_sync(rdcu_read_cmd_sram_edac_status,
			 &rdcu->sram_edac_status, 0);
}


/**
 * @brief sync a range of 32 bit words of the local mirror to the remote SRAM
 *
 * @param addr and address within the remote SRAM
 * @param size the number of bytes to sync
 * @param mtu the maximum transport unit per RMAP packet; choose wisely
 *
 * @note due to restrictions, the number of bytes and mtu must be a multiple
 *	 of 4; the address must be aligned to 32-bits as well
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_mirror_to_sram(uint32_t addr, uint32_t size, uint32_t mtu)
{
	int ret;

	uint32_t sent = 0;
	uint32_t tx_bytes;


	if (mtu & 0x3)
		return -1;

	if (addr & 0x3)
		return -1;

	if (size & 0x3)
		return -1;

	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if ((addr + size) > (RDCU_SRAM_END + 1))
		return -1;


	tx_bytes = size;

	while (tx_bytes >= mtu) {

		ret = rdcu_sync_data(rdcu_write_cmd_data, addr + sent,
				     &rdcu->sram[addr + sent], mtu, 0);
		if (ret > 0)
			continue;

		if (ret < 0)
			return -1;


		sent     += mtu;
		tx_bytes -= mtu;
	}

	while (tx_bytes) {
		ret = rdcu_sync_data(rdcu_write_cmd_data, addr + sent,
				     &rdcu->sram[addr + sent], tx_bytes, 0);
		if (ret > 0)
			continue;

		if (ret < 0)
			return -1;

		tx_bytes = 0;
	}


	return 0;
}


/**
 * @brief sync a range of 32 bit words of the remote SRAM to the local mirror
 *
 * @param addr an address within the remote SRAM
 * @param size the number of bytes to sync
 * @param mtu the maximum transport unit per RMAP packet; choose wisely
 *
 * @note due to restrictions, the number of bytes and mtu must be a multiple
 *	 of 4; the address must be aligned to 32-bits as well
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_sync_sram_to_mirror(uint32_t addr, uint32_t size, uint32_t mtu)
{
	int ret;

	uint32_t recv = 0;
	uint32_t rx_bytes;


	if (mtu & 0x3)
		return -1;

	if (addr & 0x3)
		return -1;

	if (size & 0x3)
		return -1;

	if (addr > RDCU_SRAM_END)
		return -1;

	if (size > RDCU_SRAM_SIZE)
		return -1;

	if ((addr + size) > (RDCU_SRAM_END + 1))
		return -1;


	rx_bytes = size;

	while (rx_bytes >= mtu) {

		ret = rdcu_sync_data(rdcu_read_cmd_data, addr + recv,
				     &rdcu->sram[addr + recv], mtu, 1);

#if 1
		while (rdcu_rmap_sync_status() > 3)
			;
#endif

		if (ret > 0)
			continue;

		if (ret < 0)
			return -1;

		recv     += mtu;
		rx_bytes -= mtu;
	}

	while (rx_bytes) {
		ret = rdcu_sync_data(rdcu_read_cmd_data, addr + recv,
				     &rdcu->sram[addr + recv], rx_bytes, 1);
		if (ret > 0)
			continue;

		if (ret < 0)
			return -1;

		rx_bytes = 0;
	}


	return 0;
}








/**
 * @brief initialise the rdcu control library
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_ctrl_init(void)
{
	rdcu = (struct rdcu_mirror *) calloc(1, sizeof(struct rdcu_mirror));
	if (!rdcu) {
		printf("Error allocating memory for the RDCU mirror\n");
		return -1;
	}

#if (__sparc__)
	rdcu->sram =  (uint8_t *) 0x60000000;
#else /* assume PC */
	rdcu->sram = (uint8_t *) malloc(RDCU_SRAM_SIZE);
	if (!rdcu->sram) {
		printf("Error allocating memory for the RDCU SRAM mirror\n");
		return -1;
	}
#endif

	memset(rdcu->sram, 0, RDCU_SRAM_SIZE);  /* clear sram buffer */

	return 0;
}
