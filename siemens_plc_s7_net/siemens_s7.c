#include "siemens_helper.h"
#include "siemens_s7.h"
#include "siemens_s7_private.h"

#include "socket.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib") /* Linking with winsock library */
#pragma warning(disable:4996)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

siemens_plc_types_e current_plc = S1200;
ushort word_length = 2;
int port = 102;
char ip_address[64] = { 0 };

void s7_initialization(siemens_plc_types_e plc, char* ip)
{
	word_length = 2;
	strcpy(ip_address, ip);
	current_plc = plc;

	switch (plc)
	{
	case S1200:
		g_plc_head1[21] = 0;
		break;

	case S300:
		g_plc_head1[21] = 2;
		break;

	case S400:
		g_plc_head1[21] = 3;
		g_plc_head1[17] = 0x00;
		break;

	case S1500:
		g_plc_head1[21] = 0;
		break;

	case S200Smart:
		memcpy(g_plc_head1, g_plc_head1_200smart, sizeof(g_plc_head1));
		memcpy(g_plc_head2, g_plc_head2_200smart, sizeof(g_plc_head2));
		break;

	case S200:
		memcpy(g_plc_head1, g_plc_head1_200, sizeof(g_plc_head1));
		memcpy(g_plc_head2, g_plc_head2_200, sizeof(g_plc_head2));
		break;

	default:
		g_plc_head1[18] = 0;
		break;
	}
}

bool s7_connect(char* ip_addr, int port, siemens_plc_types_e plc, int* fd)
{
	bool ret = false;
	int temp_fd = -1;
	temp_fd = socket_open_tcp_client_socket(ip_addr, port);
	s7_initialization(plc, ip_addr);
	*fd = temp_fd;

	if (temp_fd > 0)
		ret = initialization_on_connect(temp_fd);

	if (!ret && temp_fd > 0)
	{
		socket_close_tcp_socket(temp_fd);
		*fd = -1;
	}
	return ret;
}

bool s7_disconnect(int fd)
{
	socket_close_tcp_socket(fd);
	return true;
}

//////////////////////////////////////////////////////////////////////////
s7_error_code_e s7_read_response(int fd, byte_array_info* response, int* read_count)
{
	if (fd <= 0 || read_count == 0 || response == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	byte* temp = malloc(BUFFER_SIZE); // 动态分配缓冲区
	if (temp == NULL)
		return S7_ERROR_CODE_MALLOC_FAILED;

	memset(temp, 0, BUFFER_SIZE);
	response->data = temp;
	response->length = BUFFER_SIZE;

	*read_count = 0;
	char* ptr = (char*)response->data;

	if (fd <= 0 || response->length <= 0) return -1;
	*read_count = (int)recv(fd, ptr, response->length, 0);

	if (*read_count < 0) {
		return S7_ERROR_CODE_FAILED;
	}
	response->length = *read_count;

	return S7_ERROR_CODE_SUCCESS;
}

s7_error_code_e s7_read_data(int fd, const char* address, int length, byte_array_info* out_bytes, bool is_bit)
{
	if (fd <= 0 || address == NULL || length <= 0 || out_bytes == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	siemens_s7_address_data address_data;
	if (!s7_analysis_address(address, length, &address_data))
		return S7_ERROR_CODE_PARSE_ADDRESS_FAILED;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info core_cmd = is_bit ? build_read_bit_command(address_data) : build_read_byte_command(address_data);
	if (core_cmd.data == NULL)
		return S7_ERROR_CODE_BUILD_CORE_CMD_FAILED;

	if (!try_send_data_to_server(fd, &core_cmd, NULL)) {
		RELEASE_DATA(core_cmd.data);
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}
	RELEASE_DATA(core_cmd.data);

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return ret;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	ret = is_bit ? s7_analysis_read_bit(response, out_bytes) : s7_analysis_read_byte(response, out_bytes);
	RELEASE_DATA(response.data);

	return ret;
}

s7_error_code_e read_bit_value(int fd, const char* address, int length, byte_array_info* out_bytes)
{
	return s7_read_data(fd, address, length, out_bytes, true);
}

s7_error_code_e read_byte_value(int fd, const char* address, int length, byte_array_info* out_bytes)
{
	return s7_read_data(fd, address, length, out_bytes, false);
}

s7_error_code_e s7_write_data(int fd, const char* address, int length, byte_array_info in_bytes, bool is_bit, bool value)
{
	if (fd <= 0 || address == NULL || length <= 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	siemens_s7_address_data address_data;
	if (!s7_analysis_address(address, length, &address_data))
		return S7_ERROR_CODE_PARSE_ADDRESS_FAILED;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info core_cmd = is_bit ? build_write_bit_command(address_data, value) : build_write_byte_command(address_data, in_bytes);
	if (core_cmd.data == NULL)
		return S7_ERROR_CODE_BUILD_CORE_CMD_FAILED;

	if (!try_send_data_to_server(fd, &core_cmd, NULL)) {
		RELEASE_DATA(core_cmd.data);
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}
	RELEASE_DATA(core_cmd.data);

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return ret;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	ret = s7_analysis_write(response);
	RELEASE_DATA(response.data);

	return ret;
}

s7_error_code_e write_bit_value(int fd, const char* address, int length, bool value)
{
	byte_array_info dummy = { 0 };
	return s7_write_data(fd, address, length, dummy, true, value);
}

s7_error_code_e write_byte_value(int fd, const char* address, int length, byte_array_info in_bytes)
{
	return s7_write_data(fd, address, length, in_bytes, false, false);
}

s7_error_code_e s7_remote_run(int fd)
{
	if (fd < 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	const byte* core_cmd_temp = g_s7_stop;

	int core_cmd_len = sizeof(core_cmd_temp);
	byte* core_cmd = (byte*)malloc(core_cmd_len);
	if (core_cmd == NULL)
		return S7_ERROR_CODE_BUILD_CORE_CMD_FAILED;

	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp = { 0 };
	temp.data = core_cmd;
	temp.length = core_cmd_len;

	if (!try_send_data_to_server(fd, &temp, NULL)) {
		RELEASE_DATA(temp.data);
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}
	RELEASE_DATA(temp.data);

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return false;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	ret = s7_analysis_write(response);
	RELEASE_DATA(response.data);

	return ret;
}

s7_error_code_e s7_remote_stop(int fd)
{
	if (fd <= 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	const byte* core_cmd_temp = g_s7_stop;
	int core_cmd_len = sizeof(g_s7_stop);

	byte* core_cmd = (byte*)malloc(core_cmd_len);
	if (core_cmd == NULL)
		return S7_ERROR_CODE_BUILD_CORE_CMD_FAILED;
	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp = { 0 };
	temp.data = core_cmd;
	temp.length = core_cmd_len;

	if (!try_send_data_to_server(fd, &temp, NULL)) {
		RELEASE_DATA(temp.data);
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}
	RELEASE_DATA(temp.data);

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return ret;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	ret = s7_analysis_write(response);
	RELEASE_DATA(response.data);

	return ret;
}

s7_error_code_e s7_remote_reset(int fd)
{
	if (fd <= 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte core_cmd_temp[] = { 0x06, 0x10, 0x00, 0x00, 0x01, 0x00 };

	int core_cmd_len = sizeof(core_cmd_temp) / sizeof(core_cmd_temp[0]);
	byte* core_cmd = (byte*)malloc(core_cmd_len);
	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp = { 0 };
	temp.data = core_cmd;
	temp.length = core_cmd_len;

	if (!try_send_data_to_server(fd, &temp, NULL)) {
		RELEASE_DATA(temp.data);
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}
	RELEASE_DATA(temp.data);

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return ret;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	ret = s7_analysis_write(response);
	RELEASE_DATA(response.data);

	return ret;
}

s7_error_code_e s7_read_plc_type(int fd, char** type)
{
	if (fd <= 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info out_bytes = { 0 };

	byte* core_cmd = g_plc_order_number;
	int core_cmd_len = sizeof(g_plc_order_number);

	byte_array_info temp = { 0 };
	temp.data = core_cmd;
	temp.length = core_cmd_len;

	if (!try_send_data_to_server(fd, &temp, NULL)) {
		return S7_ERROR_CODE_SOCKET_SEND_FAILED;
	}

	byte_array_info response = { 0 };
	int recv_size = 0;
	ret = s7_read_response(fd, &response, &recv_size);
	if (ret != S7_ERROR_CODE_SUCCESS)
	{
		RELEASE_DATA(response.data);
		return ret;
	}

	if (recv_size < MIN_HEADER_SIZE) {
		RELEASE_DATA(response.data);
		return S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}

	out_bytes.length = 20;
	out_bytes.data = (byte*)malloc(out_bytes.length + 1);
	memset(out_bytes.data, 0, out_bytes.length + 1);
	memcpy((char*)out_bytes.data, response.data + 71, out_bytes.length);
	if (out_bytes.length > 0)
	{
		*type = (char*)out_bytes.data;
	}

	return ret;
}

bool initialization_on_connect(int fd)
{
	byte_array_info ret;
	bool is_ok = false;

	// 第一次握手 -> First handshake
	byte_array_info temp = { 0 };
	temp.data = g_plc_head1;
	temp.length = sizeof(g_plc_head1);
	is_ok = read_data_from_core_server(fd, temp, &ret);
	if (!is_ok)
		return false;
	else
		if (NULL != ret.data) free(ret.data);

	// 第二次握手 -> Second handshake
	temp.data = g_plc_head2;
	temp.length = sizeof(g_plc_head2);
	is_ok = read_data_from_core_server(fd, temp, &ret);
	if (!is_ok)
		return false;

	// 调整单次接收的pdu长度信息
	g_pdu_length = ntohs(bytes2ushort(ret.data + ret.length - 2)) - 28;
	if (g_pdu_length < 200) g_pdu_length = 200;

	if (NULL != ret.data) free(ret.data);

	// 返回成功的信号 -> Return a successful signal
	return true;
}

//////////////////////////////////////////////////////////////////////////

s7_error_code_e s7_read_bool(int fd, const char* address, bool* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_bit_value(fd, address, 1, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length > 0)
	{
		*val = (bool)read_data.data[0];
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_byte(int fd, const char* address, byte* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 1, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length > 0)
	{
		*val = read_data.data[0];
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_short(int fd, const char* address, short* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 2, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length > 0)
	{
		*val = (short)ntohs(bytes2short(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_ushort(int fd, const char* address, ushort* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 2, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 2)
	{
		*val = ntohs(bytes2ushort(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_int32(int fd, const char* address, int32* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 4)
	{
		*val = (int32)ntohl(bytes2int32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_uint32(int fd, const char* address, uint32* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 2)
	{
		*val = ntohl(bytes2uint32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_int64(int fd, const char* address, int64* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 8)
	{
		*val = (int64)ntohll_(bytes2bigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_uint64(int fd, const char* address, uint64* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 8)
	{
		*val = ntohll_(bytes2ubigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_float(int fd, const char* address, float* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 4)
	{
		*val = ntohf_(bytes2uint32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_double(int fd, const char* address, double* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	ret = read_byte_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= 8)
	{
		*val = ntohd_(bytes2ubigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_string(int fd, const char* address, int length, char** val)
{
	if (fd <= 0 || address == NULL || length <= 0 || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data = { 0 };
	int read_len = (length % 2) == 1 ? length + 1 : length;
	ret = read_byte_value(fd, address, length, &read_data);
	if (ret == S7_ERROR_CODE_SUCCESS && read_data.length >= read_len)
	{
		char* ret_str = (char*)malloc(read_len);
		memset(ret_str, 0, read_len);
		memcpy(ret_str, read_data.data, read_len);
		RELEASE_DATA(read_data.data);
		*val = ret_str;
	}
	return ret;
}

s7_error_code_e s7_write_bool(int fd, const char* address, bool val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	return write_bit_value(fd, address, 1, val);
}

s7_error_code_e s7_write_byte(int fd, const char* address, byte val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	int write_len = 1;
	char temp[1] = { 0 };
	byte_array_info write_data = { 0 };
	write_data.data = temp;
	write_data.data[0] = val;
	write_data.length = write_len;

	return write_byte_value(fd, address, 1, write_data);
}

s7_error_code_e s7_write_short(int fd, const char* address, short val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 2;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	short2bytes(htons(val), write_data.data);
	ret = write_byte_value(fd, address, 2, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_ushort(int fd, const char* address, ushort val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 2;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	ushort2bytes(htons(val), write_data.data);
	ret = write_byte_value(fd, address, 2, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_int32(int fd, const char* address, int32 val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 4;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	int2bytes(htonl(val), write_data.data);
	ret = write_byte_value(fd, address, 4, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_uint32(int fd, const char* address, uint32 val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 4;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	uint2bytes(htonl(val), write_data.data);
	ret = write_byte_value(fd, address, 4, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_int64(int fd, const char* address, int64 val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 8;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	bigInt2bytes(htonll_(val), write_data.data);
	ret = write_byte_value(fd, address, 8, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_uint64(int fd, const char* address, uint64 val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 8;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	ubigInt2bytes(htonll_(val), write_data.data);
	ret = write_byte_value(fd, address, 8, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_float(int fd, const char* address, float val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 4;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	uint2bytes(htonf_(val), write_data.data);
	ret = write_byte_value(fd, address, 4, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_double(int fd, const char* address, double val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = 8;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	write_data.length = write_len;

	bigInt2bytes(htond_(val), write_data.data);
	ret = write_byte_value(fd, address, 8, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

s7_error_code_e s7_write_string(int fd, const char* address, int length, const char* val)
{
	if (fd <= 0 || address == NULL || strlen(address) == 0 || val == NULL)
		return S7_ERROR_CODE_INVALID_PARAMETER;

	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	int write_len = (length % 2) == 1 ? length + 1 : length;
	byte_array_info write_data = { 0 };
	write_data.data = (byte*)malloc(write_len);
	memset(write_data.data, 0, write_len);
	memcpy(write_data.data, val, length);
	write_data.length = write_len;

	ret = write_byte_value(fd, address, write_len, write_data);
	RELEASE_DATA(write_data.data);
	return ret;
}

byte get_plc_slot()
{
	return g_plc_slot;
}

void set_plc_slot(byte slot)
{
	g_plc_slot = slot;
	if (current_plc != S200 && current_plc != S200Smart)
		g_plc_head1[21] = (byte)((g_plc_rack * 0x20) + g_plc_slot);
}

byte get_plc_rack()
{
	return g_plc_rack;
}

void set_plc_rack(byte rack)
{
	g_plc_rack = rack;
	if (current_plc != S200 && current_plc != S200Smart)
		g_plc_head1[21] = (byte)((g_plc_rack * 0x20) + g_plc_slot);
}

byte get_plc_connection_type()
{
	return g_plc_head1[20];
}

void set_plc_connection_type(byte type)
{
	if (current_plc != S200 && current_plc != S200Smart)
		g_plc_head1[20] = type;
}

int get_plc_local_TSAP()
{
	if (current_plc == S200 || current_plc == S200Smart)
		return g_plc_head1[13] * 256 + g_plc_head1[14];
	else
		return g_plc_head1[16] * 256 + g_plc_head1[17];
}

void set_plc_local_TSAP(int tasp)
{
	byte temp[4] = { 0 };
	int2bytes(tasp, temp);

	if (current_plc == S200 || current_plc == S200Smart)
	{
		g_plc_head1[13] = temp[1];
		g_plc_head1[14] = temp[0];
	}
	else
	{
		g_plc_head1[16] = temp[1];
		g_plc_head1[17] = temp[0];
	}
}

int get_plc_dest_TSAP()
{
	if (current_plc == S200 || current_plc == S200Smart)
		return g_plc_head1[17] * 256 + g_plc_head1[18];
	else
		return g_plc_head1[20] * 256 + g_plc_head1[21];
}

void set_plc_dest_TSAP(int tasp)
{
	byte temp[4] = { 0 };
	int2bytes(tasp, temp);

	if (current_plc == S200 || current_plc == S200Smart)
	{
		g_plc_head1[17] = temp[1];
		g_plc_head1[18] = temp[0];
	}
	else
	{
		g_plc_head1[20] = temp[1];
		g_plc_head1[21] = temp[0];
	}
}

int get_plc_PDU_length()
{
	return g_pdu_length;
}