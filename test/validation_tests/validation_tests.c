#include <stdint.h>
#include <stdlib.h>



/* #include "../include/cmp_support.h" */
#include "../../include/cmp_icu.h"
#include "../../include/cmp_rdcu.h"
#include "../../include/rdcu_cmd.h"
#include "../Unity/src/unity.h"



/*TODO: remove this */
#define IMA_SAM2BYT                                                            \
sizeof(uint16_t) /* imagette sample to byte conversion factor; one imagette samples has 16 bits (2 bytes) */


void init_rdcu(void);


void setUp(void) {}
void tearDown(void) {}

static void gen_random_data(void *buffer, size_t size)
{
	uint8_t *p = buffer;
	size_t i;

	for (i = 0; i < size; i++)
		p[i] = (uint8_t)rand();
}


/**
 * @brief generate a test compressing the data in RAW mode (output = input)
 */

static int raw_mode_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	/* we use the whole SRAM: fist 1/2 -> data to compress -> second half
	 * "compressed" raw data */
	uint32_t const data_samples = (RDCU_SRAM_SIZE/2)/IMA_SAM2BYT;
	int err;

	(void)model_of_data;

	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW,
			       CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_LOSSLESS);
	TEST_ASSERT_NOT_EQUAL_INT(cfg->data_type, DATA_TYPE_UNKNOWN);


	gen_random_data(data_to_compress, cfg->samples*IMA_SAM2BYT);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, NULL, 0x0,
			       0x0, 0x0, RDCU_SRAM_SIZE/2, data_samples);
	TEST_ASSERT_FALSE(err);
}



int main(void)
{
	uint16_t *data_to_compress = malloc(RDCU_SRAM_SIZE);
	uint16_t *model_of_data = malloc(RDCU_SRAM_SIZE);
	uint16_t *compressed_data = malloc(RDCU_SRAM_SIZE);
	struct cmp_cfg cfg;
	struct cmp_status status;
	struct cmp_info info;
	int cnt = 0;

	init_rdcu();

	raw_mode_test(data_to_compress, model_of_data, &cfg);
	rdcu_compress_data(&cfg);

	/* start polling the compression status */
	/* alternative you can wait for an interrupt form the RDCU */
	do {
		/* check compression status */
		if (rdcu_read_cmp_status(&status)) {
			printf("Error occur during rdcu_read_cmp_statu");
			exit(EXIT_FAILURE);
		}

		cnt++;

		if (cnt > 5) {	/* wait for 5 polls */

			printf("Not waiting for compressor to become ready, will "
			       "check status and abort\n");

			/* interrupt the data compression */
			rdcu_interrupt_compression();

			/* now we may read the compression info register to get
			 * the error code */
			if (rdcu_read_cmp_info(&info)) {
				printf("Error occur during rdcu_read_cmp_info");
				exit(EXIT_FAILURE);
			}
			printf("Compressor error code: 0x%02X\n", info.cmp_err);
			exit(EXIT_FAILURE);
		}
	} while (!status.cmp_ready);

	/* now we may read the compressor registers */
	if (rdcu_read_cmp_info(&info)) {
		printf("Error occur during rdcu_read_cmp_info");
		exit(EXIT_FAILURE);
	}
	if (rdcu_read_cmp_bitstream(&info, compressed_data) < 0)
		printf("Error occurred by reading in the compressed data from the RDCU\n");

	free(data_to_compress);
	free(model_of_data);
	free(compressed_data);

	return 0;
}
