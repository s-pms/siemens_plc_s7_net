#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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

#ifndef _WIN32
static int read_exact(int fd, unsigned char* buffer, int length) {
	int total = 0;
	while (total < length) {
		int current = (int)read(fd, buffer + total, length - total);
		if (current <= 0) {
			return -1;
		}
		total += current;
	}
	return total;
}

static int write_exact(int fd, const unsigned char* buffer, int length) {
	int total = 0;
	while (total < length) {
		int current = (int)write(fd, buffer + total, length - total);
		if (current <= 0) {
			return -1;
		}
		total += current;
	}
	return total;
}

static int create_socket_pair(int fds[2]) {
	return socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
}

static int wait_child_success(pid_t pid) {
	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
		return 0;
	}
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void build_success_write_response(unsigned char response[22]) {
	memset(response, 0, 22);
	response[0] = 0x03;
	response[1] = 0x00;
	response[2] = 0x00;
	response[3] = 0x16;
	response[21] = 0xFF;
}

static int drain_tpkt_request(int fd) {
	unsigned char header[4] = { 0 };
	if (read_exact(fd, header, 4) != 4) {
		return 0;
	}

	int packet_length = ((int)header[2] << 8) | (int)header[3];
	if (packet_length < 4) {
		return 0;
	}

	if (packet_length == 4) {
		return 1;
	}

	unsigned char* payload = (unsigned char*)malloc(packet_length - 4);
	if (payload == NULL) {
		return 0;
	}

	int ok = read_exact(fd, payload, packet_length - 4) == packet_length - 4;
	free(payload);
	return ok;
}

static int verify_command_roundtrip(s7_error_code_e (*command_fn)(int), const unsigned char* expected, int expected_len) {
	int fds[2] = { -1, -1 };
	if (create_socket_pair(fds) != 0) {
		return 0;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return 0;
	}

	if (pid == 0) {
		unsigned char* received = (unsigned char*)malloc(expected_len);
		unsigned char response[22];
		int exit_code = 1;

		close(fds[0]);
		if (received != NULL && read_exact(fds[1], received, expected_len) == expected_len && memcmp(received, expected, expected_len) == 0) {
			build_success_write_response(response);
			if (write_exact(fds[1], response, 22) == 22) {
				exit_code = 0;
			}
		}

		free(received);
		close(fds[1]);
		_exit(exit_code);
	}

	close(fds[1]);
	s7_error_code_e ret = command_fn(fds[0]);
	close(fds[0]);
	return ret == S7_ERROR_CODE_SUCCESS && wait_child_success(pid);
}
#endif

static void test_address_parser(void) {
	siemens_s7_address_data data = { 0 };

	EXPECT_TRUE("address: valid MX100", s7_analysis_address("MX100", 1, &data));
	EXPECT_TRUE("address: valid DB1.DBD70", s7_analysis_address("DB1.DBD70", 4, &data));
	EXPECT_TRUE("address: invalid prefix", !s7_analysis_address("ZZ100", 1, &data));
}

static void test_short_packet_guard(void) {
	char* type = NULL;
#ifdef _WIN32
	EXPECT_TRUE("plc_type: protocol short packet test skipped on Windows", true);
#else
	int fds[2] = { -1, -1 };
	EXPECT_TRUE("plc_type: socketpair created", create_socket_pair(fds) == 0);
	if (fds[0] >= 0 && fds[1] >= 0) {
		pid_t pid = fork();
		if (pid == 0) {
			unsigned char short_response[80] = { 0 };
			close(fds[0]);
			if (drain_tpkt_request(fds[1])) {
				short_response[0] = 0x03;
				short_response[1] = 0x00;
				short_response[2] = 0x00;
				short_response[3] = 0x50;
				write_exact(fds[1], short_response, 80);
				close(fds[1]);
				_exit(0);
			}
			close(fds[1]);
			_exit(1);
		}

		close(fds[1]);
		s7_error_code_e ret = s7_read_plc_type(fds[0], &type);
		close(fds[0]);
		EXPECT_TRUE("plc_type: short TPKT rejected", ret == S7_ERROR_CODE_RESPONSE_HEADER_FAILED);
		EXPECT_TRUE("plc_type: peer completed", wait_child_success(pid));
	}
#endif
}

static void test_remote_run_stop_packet_path(void) {
#ifdef _WIN32
	EXPECT_TRUE("remote_run/stop: protocol packet tests skipped on Windows", true);
#else
	const unsigned char expected_run[] = {
		0x03, 0x00, 0x00, 0x25, 0x02, 0xf0, 0x80, 0x32, 0x01, 0x00,
		0x00, 0x0c, 0x00, 0x00, 0x14, 0x00, 0x00, 0x28, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x09, 0x50, 0x5f,
		0x50, 0x52, 0x4f, 0x47, 0x52, 0x41, 0x4d
	};
	const unsigned char expected_stop[] = {
		0x03, 0x00, 0x00, 0x21, 0x02, 0xf0, 0x80, 0x32, 0x01, 0x00,
		0x00, 0x0e, 0x00, 0x00, 0x10, 0x00, 0x00, 0x29, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x09, 0x50, 0x5f, 0x50, 0x52, 0x4f, 0x47,
		0x52, 0x41, 0x4d
	};

	EXPECT_TRUE("remote_run: packet content verified", verify_command_roundtrip(s7_remote_run, expected_run, (int)sizeof(expected_run)));
	EXPECT_TRUE("remote_stop: packet content verified", verify_command_roundtrip(s7_remote_stop, expected_stop, (int)sizeof(expected_stop)));
#endif
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
