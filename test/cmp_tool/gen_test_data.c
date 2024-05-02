#include <stdio.h>
#include <string.h>

#include "../bench/ref_short_cadence_1_cmp.h"
#include "../bench/ref_short_cadence_2_cmp.h"


int main(int argc, char *argv[])
{
	int i;
	FILE *fp;
	size_t s;

	if (argc < 1)
		return 1;

	for (i = 1; i < argc; i++) {
		if (strstr(argv[i], "ref_short_cadence_1_cmp")) {
			fp = fopen(argv[i], "wb");
			if(!fp)
				return 1;
			s = fwrite(ref_short_cadence_1_cmp, 1, ref_short_cadence_1_cmp_len, fp);
			fclose(fp);
			if (s!=ref_short_cadence_1_cmp_len)
				return 1;
		} else if (strstr(argv[i], "ref_short_cadence_2_cmp")) {
			fp = fopen(argv[i], "wb");
			if(!fp)
				return 1;
			s = fwrite(ref_short_cadence_2_cmp, 1, ref_short_cadence_2_cmp_len, fp);
			fclose(fp);
			if (s!=ref_short_cadence_2_cmp_len)
				return 1;
		} else {
			fprintf(stderr,"Unknown test data\n");
			return 1;
		}
	}

	return 0;
}
