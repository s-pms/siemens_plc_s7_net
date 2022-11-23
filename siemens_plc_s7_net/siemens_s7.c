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

#define RELEASE_DATA(addr) { if(addr != NULL) { free(addr);} }

#define BUFFER_SIZE 1024

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
s7_error_code_e read_bit_value(int fd, const char* address, int length, byte_array_info* out_bytes)
{
	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	siemens_s7_address_data address_data = s7_analysis_address(address, length);
	byte_array_info core_cmd = build_read_bit_command(address_data);
	if (core_cmd.data != NULL)
	{
		int need_send = core_cmd.length;
		int real_sends = socket_send_data(fd, core_cmd.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
			{
				ret = s7_analysis_read_bit(response, out_bytes);
			}
		}
		free(core_cmd.data);
	}
	return ret;
}

s7_error_code_e read_word_value(int fd, const char* address, int length, byte_array_info* out_bytes)
{
	siemens_s7_address_data address_data = s7_analysis_address(address, length);
	return read_address_data(fd, address_data, out_bytes);
}

s7_error_code_e read_address_data(int fd, siemens_s7_address_data address_data, byte_array_info* out_bytes)
{
	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info core_cmd = build_read_byte_command(address_data);
	if (core_cmd.data != NULL)
	{
		int need_send = core_cmd.length;
		int real_sends = socket_send_data(fd, core_cmd.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
				ret = s7_analysis_read_byte(response, out_bytes);
		}
		free(core_cmd.data);
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////
s7_error_code_e write_bit_value(int fd, const char* address, int length, bool value)
{
	siemens_s7_address_data address_data = s7_analysis_address(address, length);
	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info core_cmd = build_write_bit_command(address_data, value);
	if (core_cmd.data != NULL)
	{
		int need_send = core_cmd.length;
		int real_sends = socket_send_data(fd, core_cmd.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
				ret = s7_analysis_write(response);
		}
		free(core_cmd.data);
	}
	return ret;
}

s7_error_code_e write_word_value(int fd, const char* address, int length, byte_array_info in_bytes)
{
	siemens_s7_address_data address_data = s7_analysis_address(address, length);
	return write_address_data(fd, address_data, in_bytes);
}

s7_error_code_e write_address_data(int fd, siemens_s7_address_data address_data, byte_array_info in_bytes)
{
	s7_error_code_e ret = S7_ERROR_CODE_UNKOWN;
	byte_array_info core_cmd = build_write_byte_command(address_data, in_bytes);
	if (core_cmd.data != NULL)
	{
		int need_send = core_cmd.length;
		int real_sends = socket_send_data(fd, core_cmd.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
				ret = s7_analysis_write(response);
		}
		free(core_cmd.data);
	}
	return ret;
}

bool s7_remote_run(int fd)
{
	bool ret = false;
	const byte* core_cmd_temp = g_s7_stop;

	int core_cmd_len = sizeof(core_cmd_temp);
	byte* core_cmd = (byte*)malloc(core_cmd_len);
	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp;
	temp.data = core_cmd;
	temp.length = core_cmd_len;
	if (core_cmd != NULL)
	{
		int need_send = temp.length;
		int real_sends = socket_send_data(fd, temp.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
			{
				s7_error_code_e ret_code = s7_analysis_write(response);
				if (ret_code == S7_ERROR_CODE_OK)
					ret = true;
			}
		}
		free(temp.data);
	}

	return ret;
}

bool s7_remote_stop(int fd)
{
	bool ret = false;
	const byte* core_cmd_temp = g_s7_stop;
	int core_cmd_len = sizeof(g_s7_stop);

	byte* core_cmd = (byte*)malloc(core_cmd_len);
	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp;
	temp.data = core_cmd;
	temp.length = core_cmd_len;
	if (temp.data != NULL)
	{
		int need_send = temp.length;
		int real_sends = socket_send_data(fd, temp.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
			{
				s7_error_code_e ret_code = s7_analysis_write(response);
				if (ret_code == S7_ERROR_CODE_OK)
					ret = true;
			}
		}
		free(temp.data);
	}

	return ret;
}

bool s7_remote_reset(int fd)
{
	bool ret = false;
	byte core_cmd_temp[] = { 0x06, 0x10, 0x00, 0x00, 0x01, 0x00 };

	int core_cmd_len = sizeof(core_cmd_temp) / sizeof(core_cmd_temp[0]);
	byte* core_cmd = (byte*)malloc(core_cmd_len);
	memcpy(core_cmd, core_cmd_temp, core_cmd_len);

	byte_array_info temp;
	temp.data = core_cmd;
	temp.length = core_cmd_len;
	if (temp.data != NULL)
	{
		int need_send = temp.length;
		int real_sends = socket_send_data(fd, temp.data, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 11;
			if (recv_size >= min) //header size
			{
				s7_error_code_e ret_code = s7_analysis_write(response);
				if (ret_code == S7_ERROR_CODE_OK)
					ret = true;
			}
		}
		free(temp.data);
	}

	return ret;
}

char* s7_read_plc_type(int fd)
{
	bool is_ok = false;
	byte_array_info out_bytes;
	memset(&out_bytes, 0, sizeof(out_bytes));

	byte* core_cmd = g_plc_order_number;
	int core_cmd_len = sizeof(g_plc_order_number);
	if (core_cmd != NULL)
	{
		int need_send = core_cmd_len;
		int real_sends = socket_send_data(fd, core_cmd, need_send);
		if (real_sends == need_send)
		{
			byte temp[BUFFER_SIZE];
			memset(temp, 0, BUFFER_SIZE);
			byte_array_info response;
			response.data = temp;
			response.length = BUFFER_SIZE;

			int recv_size = s7_read_response(fd, &response);
			int min = 21;
			if (recv_size >= min) //header size
			{
				out_bytes.length = 20;
				out_bytes.data = (byte*)malloc(out_bytes.length + 1);
				memset(out_bytes.data, 0, out_bytes.length + 1);
				memcpy((char*)out_bytes.data, response.data + 71, out_bytes.length);
				is_ok = true;
			}
		}
	}

	if (is_ok && out_bytes.length > 0)
	{
		return (char*)out_bytes.data;
	}
	return NULL;
}

bool initialization_on_connect(int fd)
{
	byte_array_info ret;
	bool is_ok = false;

	// 第一次握手 -> First handshake
	byte_array_info temp;
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

int s7_read_response(int fd, byte_array_info* response)
{
	int   nread = 0;
	char* ptr = (char*)response->data;

	if (fd < 0 || response->length <= 0) return -1;
	nread = (int)recv(fd, ptr, response->length, 0);

	if (nread < 0) {
		return -1;
	}
	response->length = nread;

	return nread;
}

//////////////////////////////////////////////////////////////////////////

s7_error_code_e s7_read_bool(int fd, const char* address, bool* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_bit_value(fd, address, 1, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length > 0)
	{
		*val = (bool)read_data.data[0];
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_short(int fd, const char* address, short* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 2, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length > 0)
	{
		*val = (short)ntohs(bytes2short(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_ushort(int fd, const char* address, ushort* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 2, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 2)
	{
		*val = ntohs(bytes2ushort(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_int32(int fd, const char* address, int32* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 4)
	{
		*val = (int32)ntohl(bytes2int32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_uint32(int fd, const char* address, uint32* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 2)
	{
		*val = ntohl(bytes2uint32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_int64(int fd, const char* address, int64* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 8)
	{
		*val = (int64)ntohll_(bytes2bigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_uint64(int fd, const char* address, uint64* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 8)
	{
		*val = ntohll_(bytes2ubigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_float(int fd, const char* address, float* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 4, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 4)
	{
		*val = ntohf_(bytes2uint32(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_double(int fd, const char* address, double* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	byte_array_info read_data;
	memset(&read_data, 0, sizeof(read_data));
	ret = read_word_value(fd, address, 8, &read_data);
	if (ret == S7_ERROR_CODE_OK && read_data.length >= 8)
	{
		*val = ntohd_(bytes2ubigInt(read_data.data));
		RELEASE_DATA(read_data.data);
	}
	return ret;
}

s7_error_code_e s7_read_string(int fd, const char* address, int length, char** val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (length > 0)
	{
		byte_array_info read_data;
		memset(&read_data, 0, sizeof(read_data));
		int read_len = (length % 2) == 1 ? length + 1 : length;
		ret = read_word_value(fd, address, length, &read_data);
		if (ret == S7_ERROR_CODE_OK && read_data.length >= read_len)
		{
			char* ret_str = (char*)malloc(read_len);
			memset(ret_str, 0, read_len);
			memcpy(ret_str, read_data.data, read_len);
			RELEASE_DATA(read_data.data);
			*val = ret_str;
		}
	}
	return ret;
}

s7_error_code_e s7_write_bool(int fd, const char* address, bool val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		ret = write_bit_value(fd, address, 1, val);
	}
	return ret;
}

s7_error_code_e s7_write_short(int fd, const char* address, short val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 2;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		short2bytes(htons(val), write_data.data);
		ret = write_word_value(fd, address, 2, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_ushort(int fd, const char* address, ushort val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 2;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		ushort2bytes(htons(val), write_data.data);
		ret = write_word_value(fd, address, 2, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_int32(int fd, const char* address, int32 val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 4;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		int2bytes(htonl(val), write_data.data);
		ret = write_word_value(fd, address, 4, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_uint32(int fd, const char* address, uint32 val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 4;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		uint2bytes(htonl(val), write_data.data);
		ret = write_word_value(fd, address, 4, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_int64(int fd, const char* address, int64 val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 8;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		bigInt2bytes(htonll_(val), write_data.data);
		ret = write_word_value(fd, address, 8, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_uint64(int fd, const char* address, uint64 val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 8;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		ubigInt2bytes(htonll_(val), write_data.data);
		ret = write_word_value(fd, address, 8, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_float(int fd, const char* address, float val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 4;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		uint2bytes(htonf_(val), write_data.data);
		ret = write_word_value(fd, address, 4, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_double(int fd, const char* address, double val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL)
	{
		int write_len = 8;
		byte_array_info write_data;
		memset(&write_data, 0, sizeof(write_data));
		write_data.data = (byte*)malloc(write_len);
		write_data.length = write_len;

		bigInt2bytes(htond_(val), write_data.data);
		ret = write_word_value(fd, address, 8, write_data);
	}
	return ret;
}

s7_error_code_e s7_write_string(int fd, const char* address, int length, const char* val)
{
	s7_error_code_e ret = S7_ERROR_CODE_FAILED;
	if (fd > 0 && address != NULL && val != NULL)
	{
		int write_len = (length % 2) == 1 ? length + 1 : length;
		byte_array_info write_data;
		write_data.data = (byte*)malloc(write_len);
		memset(write_data.data, 0, write_len);
		memcpy(write_data.data, val, length);
		write_data.length = write_len;

		ret = write_word_value(fd, address, write_len / 2, write_data);
	}
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