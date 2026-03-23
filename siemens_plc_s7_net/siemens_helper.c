/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2022-2026 wqliceman
 * GitHub: iceman
 * Email: wqliceman@gmail.com
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "siemens_helper.h"
#include "socket.h"

#define BUFFER_SIZE 1024

// Extract common command header building logic
static void build_command_header(byte* command, ushort command_len, byte command_type) {
	command[0] = 0x03;
	command[1] = 0x00;
	command[2] = (byte)(command_len / 256);
	command[3] = (byte)(command_len % 256);
	command[4] = 0x02;
	command[5] = 0xF0;
	command[6] = 0x80;
	command[7] = 0x32;
	command[8] = 0x01;
	command[9] = 0x00;
	command[10] = 0x00;
	command[11] = 0x00;
	command[12] = 0x01;
	command[13] = (byte)((command_len - 17) / 256);
	command[14] = (byte)((command_len - 17) % 256);
	command[15] = 0x00;
	command[16] = 0x00;
	command[17] = command_type;
	command[18] = 0x01;
}

// Build core packet from address
byte_array_info build_read_byte_command(siemens_s7_address_data address)
{
	const ushort command_len = 19 + 12;	// head + block
	byte* command = (byte*)malloc(command_len);
	if (command == NULL)
		return (byte_array_info) { 0 };

	build_command_header(command, command_len, 0x04);

	// Specify valid value type
	command[19] = 0x12;
	// Address access length for this request
	command[20] = 0x0A;
	// Syntax tag, ANY
	command[21] = 0x10;
	// Unit: word
	if (address.data_code == 0x1E || address.data_code == 0x1F)
	{
		command[22] = address.data_code;
		// Number of data items to access
		command[23] = (byte)(address.length / 2 / 256);
		command[24] = (byte)(address.length / 2 % 256);
	}
	else
	{
		if (address.data_code == 0x06 || address.data_code == 0x07)
		{
			// Number of data items to access
			command[22] = 0x04;
			command[23] = (byte)(address.length / 2 / 256);
			command[24] = (byte)(address.length / 2 % 256);
		}
		else
		{
			command[22] = 0x02;
			// Number of data items to access
			command[23] = (byte)(address.length / 256);
			command[24] = (byte)(address.length % 256);
		}
	}
	// DB block number
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// Data type to access
	command[27] = address.data_code;
	// Offset position
	command[28] = (byte)(address.address_start / 256 / 256 % 256);
	command[29] = (byte)(address.address_start / 256 % 256);
	command[30] = (byte)(address.address_start % 256);

	byte_array_info ret = { 0 };
	ret.data = command;
	ret.length = command_len;
	return ret;
}

byte_array_info build_read_bit_command(siemens_s7_address_data address)
{
	const ushort command_len = 19 + 12;	// head + block
	byte* command = (byte*)malloc(command_len);
	if (command == NULL)
		return (byte_array_info) { 0 };

	build_command_header(command, command_len, 0x04);

	// Read address prefix
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	// Data read: bit mode
	command[22] = 0x01;
	// Number of data items to access
	command[23] = 0x00;
	command[24] = 0x01;
	// DB block number
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// Data type to access
	command[27] = address.data_code;
	// Offset position
	command[28] = (byte)(address.address_start / 256 / 256 % 256);
	command[29] = (byte)(address.address_start / 256 % 256);
	command[30] = (byte)(address.address_start % 256);

	byte_array_info ret = { 0 };
	ret.data = command;
	ret.length = command_len;
	return ret;
}

byte_array_info build_write_byte_command(siemens_s7_address_data address, byte_array_info value)
{
	int val_len = 0;
	if (value.data != NULL)
		val_len = value.length;

	const ushort command_len = 35 + val_len;
	byte* command = (byte*)malloc(command_len);
	if (command == NULL)
		return (byte_array_info) { 0 };

	build_command_header(command, command_len, 0x05);

	// Write length + 4
	command[15] = (byte)((4 + val_len) / 256);
	command[16] = (byte)((4 + val_len) % 256);
	// Read/write instruction
	command[17] = 0x05;
	// Number of data blocks to write
	command[18] = 0x01;
	// Fixed, return data length
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	if (address.data_code == 0x06 || address.data_code == 0x07)
	{
		// Write mode: 1=bitwise, 2=byte, 4=word
		command[22] = 0x04;
		// Number of data items to write
		command[23] = (byte)(val_len / 2 / 256);
		command[24] = (byte)(val_len / 2 % 256);
	}
	else
	{
		// Write mode: 1=bitwise, 2=byte, 4=word
		command[22] = 0x02;
		// Number of data items to write
		command[23] = (byte)(val_len / 256);
		command[24] = (byte)(val_len % 256);
	}
	// DB block number
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// Data type to write
	command[27] = address.data_code;
	// Offset position
	command[28] = (byte)(address.address_start / 256 / 256 % 256); ;
	command[29] = (byte)(address.address_start / 256 % 256);
	command[30] = (byte)(address.address_start % 256);
	// Write by word
	command[31] = 0x00;
	command[32] = 0x04;
	// Bit-count length
	command[33] = (byte)(val_len * 8 / 256);
	command[34] = (byte)(val_len * 8 % 256);

	if (value.data != NULL)
	{
		memcpy(command + 35, value.data, val_len);
	}

	byte_array_info ret = { 0 };
	ret.data = command;
	ret.length = command_len;
	return ret;
}

byte_array_info build_write_bit_command(siemens_s7_address_data address, bool value)
{
	byte buffer[1] = { 0 };
	ushort buffer_len = sizeof(buffer);
	buffer[0] = value ? (byte)0x01 : (byte)0x00;

	const ushort command_len = 35 + buffer_len;
	byte* command = (byte*)malloc(command_len);
	if (command == NULL)
		return (byte_array_info) { 0 };

	build_command_header(command, command_len, 0x05);

	// Write length + 4
	command[15] = (byte)((4 + buffer_len) / 256);
	command[16] = (byte)((4 + buffer_len) % 256);
	// Command start marker
	command[17] = 0x05;
	// Number of data blocks to write
	command[18] = 0x01;
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	// Write mode: 1=bitwise, 2=byte, 4=word
	command[22] = 0x01;
	// Number of data items to write
	command[23] = (byte)(buffer_len / 256);
	command[24] = (byte)(buffer_len % 256);
	// DB block number
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// Data type to write
	command[27] = address.data_code;
	// Offset position
	command[28] = (byte)(address.address_start / 256 / 256);
	command[29] = (byte)(address.address_start / 256);
	command[30] = (byte)(address.address_start % 256);
	// Bitwise write
	if (address.data_code == 0x1C)
	{
		command[31] = 0x00;
		command[32] = 0x09;
	}
	else
	{
		command[31] = 0x00;
		command[32] = 0x03;
	}
	// Bit-count length
	command[33] = (byte)(buffer_len / 256);
	command[34] = (byte)(buffer_len % 256);

	memcpy(command + 35, buffer, buffer_len);

	byte_array_info ret = { 0 };
	ret.data = command;
	ret.length = command_len;
	return ret;
}

// Extract actual data content from the S7 protocol response when reading a BOOL value
s7_error_code_e s7_analysis_read_bit(byte_array_info response, byte_array_info* ret)
{
	s7_error_code_e ret_code = S7_ERROR_CODE_SUCCESS;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	if (response.length >= MIN_HEADER_SIZE && response.data[20] == 1)
	{
		byte buffer[1] = { 0 };
		if (22 < response.length)
		{
			if (response.data[21] == 0xFF && response.data[22] == 0x03)
			{
				buffer[0] = response.data[25];
			}
			else if (response.data[21] == 0x05 && response.data[22] == 0x00)
			{
				ret_code = S7_ERROR_CODE_READ_LENGTH_OVER_PLC_ASSIGN;
			}
			else if (response.data[21] == 0x06 && response.data[22] == 0x00)
			{
				ret_code = S7_ERROR_CODE_ERROR_0006;
			}
			else if (response.data[21] == 0x0A && response.data[22] == 0x00)
			{
				ret_code = S7_ERROR_CODE_ERROR_000A;
			}
			else
			{
				ret_code = S7_ERROR_CODE_UNKOWN;
			}
		}

		ret->data = (byte*)malloc(1);
		if (ret->data == NULL)
		{
			return S7_ERROR_CODE_MALLOC_FAILED;
		}
		memset(ret->data, 0, 1);
		memcpy(ret->data, buffer, 1);
		ret->length = 1;
	}
	else
	{
		ret_code = S7_ERROR_CODE_DATA_LENGTH_CHECK_FAILED;
	}

	return ret_code;
}

s7_error_code_e s7_analysis_read_byte(byte_array_info response, byte_array_info* ret)
{
	s7_error_code_e ret_code = S7_ERROR_CODE_SUCCESS;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	int i = 0, j = 0;
	byte buffer[1024] = { 0 };
	int buffer_length = 0;
	int temp_index = 0;
	if (response.length >= MIN_HEADER_SIZE)
	{
		for (i = 21; i < response.length - 1; i++)
		{
			if (response.data[i] == 0xFF && response.data[i + 1] == 0x04)
			{
				int count = (response.data[i + 2] * 256 + response.data[i + 3]) / 8;
				memcpy(buffer + buffer_length, response.data + i + 4, count);
				buffer_length += count;

				i += count + 3;
			}
			else if (response.data[i] == 0xFF && response.data[i + 1] == 0x09)
			{
				int count = response.data[i + 2] * 256 + response.data[i + 3];
				if (count % 3 == 0)
				{
					for (j = 0; j < count / 3; j++)
					{
						temp_index = i + 5 + 3 * j;
						memcpy(buffer + buffer_length, (void*)(response.data + temp_index), 2);
						buffer_length += 2;
					}
				}
				else
				{
					for (j = 0; j < count / 5; j++)
					{
						temp_index = i + 7 + 5 * j;
						memcpy(buffer + buffer_length, (void*)(response.data + temp_index), 2);
						buffer_length += 2;
					}
				}
				i += count + 4;
			}
			else if (response.data[i] == 0x05 && response.data[i + 1] == 0x00)
				ret_code = S7_ERROR_CODE_READ_LENGTH_OVER_PLC_ASSIGN;
			else if (response.data[i] == 0x06 && response.data[i + 1] == 0x00)
				ret_code = S7_ERROR_CODE_ERROR_0006;
			else if (response.data[i] == 0x0A && response.data[i + 1] == 0x00)
				ret_code = S7_ERROR_CODE_ERROR_000A;
		}

		ret->data = (byte*)malloc(buffer_length);
		if (ret->data == NULL)
		{
			return S7_ERROR_CODE_MALLOC_FAILED;
		}
		memset(ret->data, 0, buffer_length);
		memcpy(ret->data, buffer, buffer_length);
		ret->length = buffer_length;
	}
	else
	{
		ret_code = S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}
	return ret_code;
}

s7_error_code_e s7_analysis_write(byte_array_info response)
{
	s7_error_code_e ret_code = S7_ERROR_CODE_SUCCESS;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	byte buffer[1024] = { 0 };
	int buffer_length = 0;
	if (response.length > 21)
	{
		byte code = response.data[21];
		if (code != 0xFF)
			ret_code = S7_ERROR_CODE_WRITE_ERROR;
	}
	else
	{
		ret_code = S7_ERROR_CODE_RESPONSE_HEADER_FAILED;
	}
	return ret_code;
}

bool read_data_from_core_server(int fd, byte_array_info send, byte_array_info* ret)
{
	bool is_ok = false;
	int need_send = send.length;
	int real_sends = socket_send_data(fd, send.data, need_send);
	if (real_sends == need_send)
	{
		byte header[4] = { 0 };
		int header_size = socket_recv_data(fd, header, 4);
		if (header_size != 4)
			return false;

		int recv_size = ((int)header[2] << 8) | (int)header[3];
		int min = 21;
		if (recv_size >= min) //header size
		{
			byte* temp = (byte*)malloc(recv_size);
			if (temp == NULL)
				return false;

			memcpy(temp, header, 4);
			if (recv_size > 4)
			{
				int body_size = socket_recv_data(fd, temp + 4, recv_size - 4);
				if (body_size != recv_size - 4)
				{
					RELEASE_DATA(temp);
					return false;
				}
			}

			ret->data = (byte*)malloc(recv_size);
			if (ret->data == NULL)
			{
				RELEASE_DATA(temp);
				return false;
			}
			memset(ret->data, 0, recv_size);
			memcpy(ret->data, temp, recv_size);
			ret->length = recv_size;
			RELEASE_DATA(temp);

			is_ok = true;
		}
	}
	return is_ok;
}

bool send_data_to_core_server(int fd, byte_array_info send)
{
	bool is_ok = false;
	int need_send = send.length;
	int real_sends = socket_send_data(fd, send.data, need_send);
	if (real_sends == need_send)
	{
		is_ok = true;
	}
	return is_ok;
}

bool try_send_data_to_server(int fd, byte_array_info* in_bytes, int* real_sends)
{
	if (fd < 0 || in_bytes == NULL)
		return false;

	int temp_real_sends = 0;
	if (real_sends == NULL)
		real_sends = &temp_real_sends;

	int retry_times = 0;
	while (retry_times < MAX_RETRY_TIMES) {
		int need_send = in_bytes->length;
		*real_sends = socket_send_data(fd, in_bytes->data, need_send);
		if (*real_sends == need_send) {
			break;
		}
		// Handle send failure, e.g. retry or record an error.
		retry_times++;
	}

	if (retry_times >= MAX_RETRY_TIMES) {
		return false;
	}

	return true;
}