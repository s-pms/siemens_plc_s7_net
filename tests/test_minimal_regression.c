#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../siemens_plc_s7_net/siemens_s7_comm.h"
#include "../siemens_plc_s7_net/siemens_s7.h"

static int g_failed = 0;

#define EXPECT_TRUE(name, cond) \
	do { \
		if (!(cond)) { \
			printf("[FAIL] %s\n", name); \
			g_failed++; \
		} else { \
			printf("[PASS] %s\n", name); \
		} \
	} while (0)

static void test_address_parser(void) {
	siemens_s7_address_data data = { 0 };

	EXPECT_TRUE("address: valid MX100", s7_analysis_address("MX100", 1, &data));
	EXPECT_TRUE("address: valid DB1.DBD70", s7_analysis_address("DB1.DBD70", 4, &data));
	EXPECT_TRUE("address: invalid prefix", !s7_analysis_address("ZZ100", 1, &data));
}

static void test_short_packet_guard(void) {
	char* type = NULL;
	s7_error_code_e ret = s7_read_plc_type(-1, &type);
	EXPECT_TRUE("plc_type: invalid fd rejected", ret == S7_ERROR_CODE_INVALID_PARAMETER);
}

static void test_remote_run_stop_packet_path(void) {
	s7_error_code_e run_ret = s7_remote_run(-1);
	s7_error_code_e stop_ret = s7_remote_stop(-1);

	EXPECT_TRUE("remote_run: invalid fd rejected", run_ret == S7_ERROR_CODE_INVALID_PARAMETER);
	EXPECT_TRUE("remote_stop: invalid fd rejected", stop_ret == S7_ERROR_CODE_INVALID_PARAMETER);
}

int main(void) {
	printf("Running minimal regression tests...\n");

	test_address_parser();
	test_short_packet_guard();
	test_remote_run_stop_packet_path();

	if (g_failed == 0) {
		printf("All tests passed.\n");
		return 0;
	}

	printf("Tests failed: %d\n", g_failed);
	return 1;
}
