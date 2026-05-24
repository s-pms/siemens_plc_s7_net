/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2022-2026 wqliceman
 * GitHub: iceman
 * Email: wqliceman@gmail.com
 */

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
#include "../siemens_plc_s7_net/siemens_helper.h"
#include "../siemens_plc_s7_net/siemens_s7.h"

extern byte g_plc_head1[22];
extern byte g_plc_head2[25];
extern void s7_initialization(siemens_plc_types_e plc, char* ip);
extern bool initialization_on_connect(int fd);

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
	response[4] = 0x02;
	response[5] = 0xF0;
	response[6] = 0x80;
	response[7] = 0x32;
	response[21] = 0xFF;
}

static void build_handshake_response(unsigned char* response, int response_length, unsigned short negotiated_pdu_length) {
	memset(response, 0, (size_t)response_length);
	response[0] = 0x03;
	response[1] = 0x00;
	response[2] = (unsigned char)((unsigned int)response_length >> 8);
	response[3] = (unsigned char)(response_length & 0xFF);
	response[response_length - 2] = (unsigned char)((unsigned int)(negotiated_pdu_length + 28) >> 8);
	response[response_length - 1] = (unsigned char)((negotiated_pdu_length + 28) & 0xFF);
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
		// Child process plays the PLC side: verify the outgoing command, then send a minimal success response.
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

static int open_initialized_peer(int negotiated_pdu_length, int* client_fd) {
	int fds[2] = { -1, -1 };
	if (client_fd == NULL || create_socket_pair(fds) != 0) {
		return 0;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return 0;
	}

	if (pid == 0) {
		unsigned char first_response[22];
		unsigned char second_response[25];
		int exit_code = 1;

		close(fds[0]);
		build_handshake_response(first_response, (int)sizeof(first_response), 240);
		build_handshake_response(second_response, (int)sizeof(second_response), (unsigned short)negotiated_pdu_length);

		if (drain_tpkt_request(fds[1]) && write_exact(fds[1], first_response, (int)sizeof(first_response)) == (int)sizeof(first_response) &&
			drain_tpkt_request(fds[1]) && write_exact(fds[1], second_response, (int)sizeof(second_response)) == (int)sizeof(second_response)) {
			exit_code = 0;
		}

		close(fds[1]);
		_exit(exit_code);
	}

	close(fds[1]);
	if (!initialization_on_connect(fds[0]) || !wait_child_success(pid)) {
		close(fds[0]);
		return 0;
	}

	*client_fd = fds[0];
	return 1;
}
#endif

static void test_address_parser(void) {
	siemens_s7_address_data data = { 0 };

	EXPECT_TRUE("address: valid MX100", s7_analysis_address("MX100", 1, &data));
	EXPECT_TRUE("address: valid DB1.DBD70", s7_analysis_address("DB1.DBD70", 4, &data));
	EXPECT_TRUE("address: valid DB1.DBX0.1", s7_analysis_address("DB1.DBX0.1", 1, &data));
	EXPECT_TRUE("address: valid T100", s7_analysis_address("T100", 2, &data));
	EXPECT_TRUE("address: valid C100", s7_analysis_address("C100", 2, &data));
	EXPECT_TRUE("address: invalid prefix", !s7_analysis_address("ZZ100", 1, &data));
	EXPECT_TRUE("address: invalid fraction range", !s7_analysis_address("MX0.8", 1, &data));
	EXPECT_TRUE("address: invalid fraction token", !s7_analysis_address("MX0.A", 1, &data));
	EXPECT_TRUE("address: invalid empty string", !s7_analysis_address("", 0, &data));
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
			// Return a well-formed TPKT length that is still too short for the fixed PLC type payload offset.
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

static void test_malformed_header_guard(void) {
	char* type = NULL;
#ifdef _WIN32
	EXPECT_TRUE("plc_type: malformed header test skipped on Windows", true);
#else
	int fds[2] = { -1, -1 };
	EXPECT_TRUE("plc_type malformed: socketpair created", create_socket_pair(fds) == 0);
	if (fds[0] >= 0 && fds[1] >= 0) {
		pid_t pid = fork();
		if (pid == 0) {
			unsigned char response[91] = { 0 };
			close(fds[0]);
			// Keep the packet long enough, but corrupt the protocol signature bytes to exercise header validation.
			if (drain_tpkt_request(fds[1])) {
				response[0] = 0x03;
				response[1] = 0x00;
				response[2] = 0x00;
				response[3] = 0x5B;
				response[4] = 0x01;
				response[5] = 0xF0;
				response[6] = 0x80;
				response[7] = 0x32;
				write_exact(fds[1], response, 91);
				close(fds[1]);
				_exit(0);
			}
			close(fds[1]);
			_exit(1);
		}

		close(fds[1]);
		s7_error_code_e ret = s7_read_plc_type(fds[0], &type);
		close(fds[0]);
		EXPECT_TRUE("plc_type: malformed protocol header rejected", ret == S7_ERROR_CODE_RESPONSE_HEADER_FAILED);
		EXPECT_TRUE("plc_type malformed: peer completed", wait_child_success(pid));
	}
#endif
}

static void test_read_byte_segment_bounds(void) {
	byte raw_response[40] = { 0 };
	byte_array_info response = { raw_response, (int)sizeof(raw_response) };
	byte_array_info parsed = { 0 };

	/*
	 * Craft a byte-read item whose declared payload exceeds the actual frame body.
	 * A robust parser must reject it instead of reading past the received packet.
	 */
	raw_response[21] = 0xFF;
	raw_response[22] = 0x04;
	raw_response[23] = 0x00;
	raw_response[24] = 0x40; /* 64 bits => 8 payload bytes */
	raw_response[25] = 0x11;
	raw_response[26] = 0x22;

	EXPECT_TRUE(
		"read_byte: truncated payload rejected",
		s7_analysis_read_byte(response, &parsed) == S7_ERROR_CODE_RESPONSE_HEADER_FAILED);
	EXPECT_TRUE("read_byte: no buffer allocated on truncated payload", parsed.data == NULL && parsed.length == 0);
}

static void test_initialization_resets_standard_headers(void) {
	static const byte expected_head1[] = {
		0x03, 0x00, 0x00, 0x16, 0x11, 0xE0, 0x00, 0x00, 0x00, 0x01,
		0x00, 0xC0, 0x01, 0x0A, 0xC1, 0x02, 0x01, 0x02, 0xC2, 0x02,
		0x01, 0x00
	};
	static const byte expected_head2[] = {
		0x03, 0x00, 0x00, 0x19, 0x02, 0xF0, 0x80, 0x32, 0x01, 0x00,
		0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0x00, 0xF0, 0x00, 0x00,
		0x01, 0x00, 0x01, 0x01, 0xE0
	};
	char ip[] = "127.0.0.1";

	s7_initialization(S200Smart, ip);
	s7_initialization(S1200, ip);

	EXPECT_TRUE(
		"init: standard head1 restored after S200Smart",
		memcmp(g_plc_head1, expected_head1, sizeof(expected_head1)) == 0);
	EXPECT_TRUE(
		"init: standard head2 restored after S200Smart",
		memcmp(g_plc_head2, expected_head2, sizeof(expected_head2)) == 0);
}

static void test_connect_parameter_guard(void) {
	int fd = 1234;

	EXPECT_TRUE("connect: null fd rejected", !s7_connect("127.0.0.1", 102, S1200, NULL));

	fd = 1234;
	EXPECT_TRUE("connect: null ip rejected", !s7_connect(NULL, 102, S1200, &fd) && fd == -1);

	fd = 1234;
	EXPECT_TRUE("connect: empty ip rejected", !s7_connect("", 102, S1200, &fd) && fd == -1);

	fd = 1234;
	EXPECT_TRUE("connect: zero port rejected", !s7_connect("127.0.0.1", 0, S1200, &fd) && fd == -1);

	fd = 1234;
	EXPECT_TRUE("connect: overflow port rejected", !s7_connect("127.0.0.1", 70000, S1200, &fd) && fd == -1);
}

static void test_initialization_preserves_standard_configuration(void) {
	char ip[] = "127.0.0.1";

	set_plc_connection_type(0x03);
	set_plc_rack(0x01);
	set_plc_slot(0x02);
	set_plc_local_TSAP(0x1234);

	s7_initialization(S1200, ip);

	EXPECT_TRUE("init: standard connection type preserved", get_plc_connection_type() == 0x03);
	EXPECT_TRUE("init: standard local TSAP preserved", get_plc_local_TSAP() == 0x1234);
	EXPECT_TRUE("init: standard dest TSAP preserved", get_plc_dest_TSAP() == 0x0322);

	set_plc_connection_type(0x01);
	set_plc_rack(0x00);
	set_plc_slot(0x00);
	set_plc_local_TSAP(0x0102);
	set_plc_dest_TSAP(0x0100);
}

static void test_initialization_preserves_s200smart_configuration(void) {
	char ip[] = "127.0.0.1";

	s7_initialization(S1200, ip);
	set_plc_local_TSAP(0x5678);
	set_plc_dest_TSAP(0x1234);

	s7_initialization(S200Smart, ip);

	EXPECT_TRUE("init: S200Smart local TSAP preserved before first connect", get_plc_local_TSAP() == 0x5678);
	EXPECT_TRUE("init: S200Smart dest TSAP preserved before first connect", get_plc_dest_TSAP() == 0x1234);

	set_plc_local_TSAP(0x0102);
	set_plc_dest_TSAP(0x0300);
	set_plc_connection_type(0x01);
	set_plc_rack(0x00);
	set_plc_slot(0x00);
	s7_initialization(S1200, ip);
}

static void test_standard_configuration_updates_after_s200smart_context(void) {
	char ip[] = "127.0.0.1";

	s7_initialization(S200Smart, ip);
	set_plc_connection_type(0x03);
	set_plc_rack(0x01);
	set_plc_slot(0x02);
	set_plc_local_TSAP(0x1234);

	s7_initialization(S1200, ip);

	EXPECT_TRUE("init: standard connection type can be configured after S200Smart", get_plc_connection_type() == 0x03);
	EXPECT_TRUE("init: standard local TSAP can be configured after S200Smart", get_plc_local_TSAP() == 0x1234);
	EXPECT_TRUE("init: standard dest TSAP can be configured after S200Smart", get_plc_dest_TSAP() == 0x0322);

	set_plc_connection_type(0x01);
	set_plc_rack(0x00);
	set_plc_slot(0x00);
	set_plc_local_TSAP(0x0102);
	set_plc_dest_TSAP(0x0100);
}

static void test_connection_scoped_pdu_length(void) {
#ifdef _WIN32
	EXPECT_TRUE("pdu: connection scoped test skipped on Windows", true);
#else
	int fd_one = -1;
	int fd_two = -1;

	EXPECT_TRUE("pdu: first handshake succeeds", open_initialized_peer(240, &fd_one));
	EXPECT_TRUE("pdu: second handshake succeeds", open_initialized_peer(480, &fd_two));
	EXPECT_TRUE("pdu: first connection retains its negotiated length", s7_get_pdu_length(fd_one) == 240);
	EXPECT_TRUE("pdu: second connection retains its negotiated length", s7_get_pdu_length(fd_two) == 480);
	EXPECT_TRUE("pdu: legacy getter tracks latest connection", get_plc_PDU_length() == 480);

	if (fd_one >= 0) {
		s7_disconnect(fd_one);
		EXPECT_TRUE("pdu: disconnected first connection removed", s7_get_pdu_length(fd_one) == 0);
	}
	if (fd_two >= 0) {
		s7_disconnect(fd_two);
		EXPECT_TRUE("pdu: disconnected second connection removed", s7_get_pdu_length(fd_two) == 0);
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
	test_malformed_header_guard();
	test_read_byte_segment_bounds();
	test_initialization_resets_standard_headers();
	test_connect_parameter_guard();
	test_initialization_preserves_standard_configuration();
	test_initialization_preserves_s200smart_configuration();
	test_standard_configuration_updates_after_s200smart_context();
	test_connection_scoped_pdu_length();
	test_remote_run_stop_packet_path();

	if (g_failed == 0) {
		printf("All tests passed.\n");
		return 0;
	}

	printf("Tests failed: %d\n", g_failed);
	return 1;
}
