/**
 * @file   rdcu_cmd.c
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
 * @brief RMAP RDCU RMAP command library
 * @see FPGA Requirement Specification PLATO-IWF-PL-RS-005 Issue 0.7
 */


#include <stdlib.h>

#include <rmap.h>
#include <rdcu_cmd.h>
#include <rdcu_rmap.h>


/**
 * @brief generate a read command for an arbitrary register (internal)
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the register address
 *
 * @note this will configure a multi-address, 4 byte wide read command, because
 *	 the IWF RMAP core does not support single address read commands
 */

static int rdcu_read_cmd_register_internal(uint16_t trans_id, uint8_t *cmd,
					   uint32_t addr)
{
	return rdcu_gen_cmd(trans_id, cmd, RMAP_READ_ADDR_INC, addr, 4);
}


/**
 * @brief generate a write command for an arbitrary register (internal)
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the register address
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a multi-address, 4 byte wide write command, because
 *	 the IWF RMAP core does not support single address write commands with
 *	 reply and CRC check enabled
 */

static int rdcu_write_cmd_register_internal(uint16_t trans_id, uint8_t *cmd,
					    uint32_t addr)
{
	return rdcu_gen_cmd(trans_id, cmd, RMAP_WRITE_ADDR_INC_VERIFY_REPLY,
			    addr, 4);
}


/**
 * @brief create a command to write arbitrary data to the RDCU (internal)
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the address to write to
 * @param size the number of bytes to write
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a multi-address write command with reply enabled
 */

static int rdcu_write_cmd_data_internal(uint16_t trans_id, uint8_t *cmd,
					uint32_t addr, uint32_t size)
{
	return rdcu_gen_cmd(trans_id, cmd, RMAP_WRITE_ADDR_INC_REPLY,
			    addr, size);
}


/**
 * @brief create a command to read arbitrary data to the RDCU (internal)
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the address to read from
 * @param size the number of bytes to read
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a multi-address read command
 *
 */

static int rdcu_read_cmd_data_internal(uint16_t trans_id, uint8_t *cmd,
				       uint32_t addr, uint32_t size)
{
	return rdcu_gen_cmd(trans_id, cmd,
			    RMAP_READ_ADDR_INC, addr, size);
}


/**
 * @brief create a command to write arbitrary data to the RDCU
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the address to write to
 * @param size the number of bytes to write
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a multi-address write command with reply enabled
 */

int rdcu_write_cmd_data(uint16_t trans_id, uint8_t *cmd,
			uint32_t addr, uint32_t size)
{
	return rdcu_write_cmd_data_internal(trans_id, cmd, addr, size);
}


/**
 *
 * @brief create a command to read arbitrary data to the RDCU
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the address to read from
 * @param size the number of bytes to read
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a multi-address read command
 *
 */

int rdcu_read_cmd_data(uint16_t trans_id, uint8_t *cmd,
		       uint32_t addr, uint32_t size)
{
	return rdcu_read_cmd_data_internal(trans_id, cmd, addr, size);
}




/**
 * @brief generate a read command for an arbitrary register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the register address
 *
 * @note this will configure a single address, 4 byte wide read command
 */

int rdcu_read_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, addr);
}


/**
 * @brief generate a write command for an arbitrary register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param addr the register address
 *
 * @returns the size of the command data buffer or 0 on error
 *
 * @note this will configure a single address, 4 byte wide write command with
 *	 reply and CRC check enabled
 */

int rdcu_write_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, addr);
}




/**
 * @brief create a command to read the RDCU FPGA version register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_fpga_version(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, FPGA_VERSION);
}


/**
 * @brief create a command to read the RDCU status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_rdcu_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, RDCU_STATUS);
}


/**
 * @brief create a command to read the RDCU LVDS core status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_lvds_core_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, LVDS_CORE_STATUS);
}


/**
 * @brief create a command to read the RDCU SPW link status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_spw_link_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, SPW_LINK_STATUS);
}


/**
 * @brief create a command to read the RDCU SPW error counters register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_spw_err_cntrs(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, SPW_ERR_CNTRS);
}


/**
 * @brief create a command to read the RDCU RMAP last error register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_rmap_last_err(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, RMAP_LAST_ERR);
}


/**
 * @brief create a command to read the RDCU RMAP no reply error counter register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_rmap_no_reply_err_cntrs(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       RMAP_NO_REPLY_ERR_CNTRS);
}


/**
 * @brief create a command to read the RDCU RMAP packet error counter register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_rmap_pckt_err_cntrs(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       RMAP_PCKT_ERR_CNTRS);
}


/**
 * @brief create a command to read the RDCU ADC values 1 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_adc_values_1(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, ADC_VALUES_1);
}


/**
 * @brief create a command to read the RDCU ADC values 2 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_adc_values_2(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, ADC_VALUES_2);
}


/**
 * @brief create a command to read the RDCU ADC values 3 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_adc_values_3(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, ADC_VALUES_3);
}


/**
 * @brief create a command to read the RDCU ADC values 4 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_adc_values_4(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, ADC_VALUES_4);
}


/**
 * @brief create a command to read the RDCU ADC status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_adc_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, ADC_STATUS);
}


/**
 * @brief create a command to read the RDCU compressor status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, COMPR_STATUS);
}



/**
 * @brief create a command to write the RDCU reset register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_rdcu_reset(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, RDCU_RESET);
}


/**
 * @brief create a command to write the RDCU SPW link control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_spw_link_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, SPW_LINK_CTRL);
}


/**
 * @brief create a command to write the RDCU LVDS control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_lvds_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, LVDS_CTRL);
}


/**
 *
 * @brief create a command to write the RDCU core control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_core_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, CORE_CTRL);
}


/**
 *
 * @brief create a command to write the RDCU ADC control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_adc_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, ADC_CTRL);
}


/**
 *
 * @brief create a command to write the RDCU compressor control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_compr_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, COMPR_CTRL);
}


/**
 * @brief create a command to write the RDCU compressor parameter 1 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_compressor_param1(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, COMPR_PARAM_1);
}


/**
 * @brief create a command to write the RDCU compressor parameter 2 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_compressor_param2(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, COMPR_PARAM_2);
}


/**
 * @brief create a command to write the RDCU adaptive parameter 1 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_adaptive_param1(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd,
						ADAPTIVE_PARAM_1);
}


/**
 * @brief create a command to write the RDCU adaptive parameter 2 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_adaptive_param2(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd,
						ADAPTIVE_PARAM_2);
}


/**
 * @brief create a command to write the RDCU data start address register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_data_start_addr(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, DATA_START_ADDR);
}


/**
 * @brief create a command to write the RDCU model start address register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_model_start_addr(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, MODEL_START_ADDR);
}


/**
 * @brief create a command to write the RDCU number of samples register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_num_samples(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, NUM_SAMPLES);
}


/**
 * @brief create a command to write the RDCU updated/new model start address register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_new_model_start_addr(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd,
						UPDATED_MODEL_START_ADDR);
}


/**
 * @brief create a command to write the RDCU compressed data buffer start
 *	  address register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_compr_data_buf_start_addr(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd,
						COMPR_DATA_BUF_START_ADDR);
}


/**
 * @brief create a command to write the RDCU compressed data buffer length
 *	  register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_compr_data_buf_len(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd,
						COMPR_DATA_BUF_LEN);
}



/**
 * @brief create a command to read the RDCU used paramter 1 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_used_param1(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       USED_COMPR_PARAM_1);
}


/**
 * @brief create a command to read the RDCU unused paramter 2 register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_used_param2(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       USED_COMPR_PARAM_2);
}


/**
 * @brief create a command to read the RDCU compressed data start address
 *	  register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_data_start_addr(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       COMPR_DATA_START_ADDR);
}


/**
 * @brief create a command to read the RDCU compressed data size register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_data_size(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, COMPR_DATA_SIZE);
}


/**
 * @brief create a command to read the RDCU compressed data adaptive 1 size
 *	  register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_data_adaptive_1_size(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       COMPR_DATA_ADAPTIVE_1_SIZE);
}


/**
 * @brief create a command to read the RDCU compressed data adaptive 2 size
 *	  register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_data_adaptive_2_size(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       COMPR_DATA_ADAPTIVE_2_SIZE);
}


/**
 * @brief create a command to read the RDCU compression error register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_compr_error(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, COMPR_ERROR);
}


/**
 * @brief create a command to read the RDCU updated model start address register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_new_model_addr_used(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       USED_UPDATED_MODEL_START_ADDR);
}


/**
 * @brief create a command to read the RDCU used number of samples register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_samples_used(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd,
					       USED_NUMBER_OF_SAMPLES);
}


/**
 * @brief create a command to write the SRAM EDAC Control register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_write_cmd_sram_edac_ctrl(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_write_cmd_register_internal(trans_id, cmd, SRAM_EDAC_CTRL);
}

/**
 * @brief create a command to read the SRAM EDAC Status register
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_read_cmd_sram_edac_status(uint16_t trans_id, uint8_t *cmd)
{
	return rdcu_read_cmd_register_internal(trans_id, cmd, SRAM_EDAC_STATUS);
}
