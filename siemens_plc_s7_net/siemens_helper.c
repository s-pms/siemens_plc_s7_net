#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "siemens_helper.h"
#include "socket.h"

#define BUFFER_SIZE 1024

// 从地址构造核心报文
byte_array_info build_read_byte_command(siemens_s7_address_data address)
{
	const ushort command_len = 19 + 12;	// head + block
	byte* command = (byte*)malloc(command_len);

	command[0] = 0x03;												// 报文头 -> Head
	command[1] = 0x00;
	command[2] = (byte)(command_len / 256);							// 长度 -> Length
	command[3] = (byte)(command_len % 256);
	command[4] = 0x02;												// 固定 -> Fixed
	command[5] = 0xF0;
	command[6] = 0x80;
	command[7] = 0x32;                                              // 协议标识 -> Protocol identification
	command[8] = 0x01;                                              // 命令：发 -> Command: Send
	command[9] = 0x00;                                              // redundancy identification (reserved): 0x0000;
	command[10] = 0x00;
	command[11] = 0x00;                                             // protocol data unit reference; it’s increased by request event;
	command[12] = 0x01;
	command[13] = (byte)((command_len - 17) / 256);					// 参数命令数据总长度 -> Parameter command Data total length
	command[14] = (byte)((command_len - 17) % 256);
	command[15] = 0x00;                                              // 读取内部数据时为00，读取CPU型号为Data数据长度 -> Read internal data is 00, read CPU model is data length
	command[16] = 0x00;
	// =====================================================================================
	command[17] = 0x04;                                              // 读写指令，04读，05写 -> Read-write instruction, 04 read, 05 Write
	command[18] = 0x01;												 // 读取数据块个数 -> Number of data blocks read

	//===========================================================================================
	// 指定有效值类型 -> Specify a valid value type
	command[19] = 0x12;
	// 接下来本次地址访问长度 -> The next time the address access length
	command[20] = 0x0A;
	// 语法标记，ANY -> Syntax tag, any
	command[21] = 0x10;
	// 按字为单位 -> by word
	if (address.data_code == 0x1E || address.data_code == 0x1F)
	{
		command[22] = address.data_code;
		// 访问数据的个数 -> Number of Access data
		command[23] = (byte)(address.length / 2 / 256);
		command[24] = (byte)(address.length / 2 % 256);
	}
	else
	{
		if (address.data_code == 0x06 || address.data_code == 0x07)
		{
			// 访问数据的个数 -> Number of Access data
			command[22] = 0x04;
			command[23] = (byte)(address.length / 2 / 256);
			command[24] = (byte)(address.length / 2 % 256);
		}
		else
		{
			command[22] = 0x02;
			// 访问数据的个数 -> Number of Access data
			command[23] = (byte)(address.length / 256);
			command[24] = (byte)(address.length % 256);
		}
	}
	// DB块编号，如果访问的是DB块的话 -> DB block number, if you are accessing a DB block
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// 访问数据类型 -> Accessing data types
	command[27] = address.data_code;
	// 偏移位置 -> Offset position
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

	command[0] = 0x03;
	command[1] = 0x00;
	// 长度 -> Length
	command[2] = (byte)(command_len / 256);
	command[3] = (byte)(command_len % 256);
	// 固定 -> Fixed
	command[4] = 0x02;
	command[5] = 0xF0;
	command[6] = 0x80;
	command[7] = 0x32;
	// 命令：发 -> command to send
	command[8] = 0x01;
	// 标识序列号
	command[9] = 0x00;
	command[10] = 0x00;
	command[11] = 0x00;
	command[12] = 0x01;
	// 命令数据总长度 -> Identification serial Number
	command[13] = (byte)((command_len - 17) / 256);
	command[14] = (byte)((command_len - 17) % 256);

	command[15] = 0x00;
	command[16] = 0x00;

	// 命令起始符 -> Command start character
	command[17] = 0x04;
	// 读取数据块个数 -> Number of data blocks read
	command[18] = 0x01;

	//===========================================================================================
	// 读取地址的前缀 -> Read the prefix of the address
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	// 读取的数据时位 -> Data read-time bit
	command[22] = 0x01;
	// 访问数据的个数 -> Number of Access data
	command[23] = 0x00;
	command[24] = 0x01;
	// DB块编号，如果访问的是DB块的话 -> DB block number, if you are accessing a DB block
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// 访问数据类型 -> Types of reading data
	command[27] = address.data_code;
	// 偏移位置 -> Offset position
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
	command[0] = 0x03;
	command[1] = 0x00;
	// 长度 -> Length
	command[2] = (byte)((command_len) / 256);
	command[3] = (byte)((command_len) % 256);
	// 固定 -> Fixed
	command[4] = 0x02;
	command[5] = 0xF0;
	command[6] = 0x80;
	command[7] = 0x32;
	// 命令 发 -> command to send
	command[8] = 0x01;
	// 标识序列号 -> Identification serial Number
	command[9] = 0x00;
	command[10] = 0x00;
	command[11] = 0x00;
	command[12] = 0x01;
	// 固定 -> Fixed
	command[13] = 0x00;
	command[14] = 0x0E;
	// 写入长度+4 -> Write Length +4
	command[15] = (byte)((4 + val_len) / 256);
	command[16] = (byte)((4 + val_len) % 256);
	// 读写指令 -> Read and write instructions
	command[17] = 0x05;
	// 写入数据块个数 -> Number of data blocks written
	command[18] = 0x01;
	// 固定，返回数据长度 -> Fixed, return data length
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	if (address.data_code == 0x06 || address.data_code == 0x07)
	{
		// 写入方式，1是按位，2是按字 -> Write mode, 1 is bitwise, 2 is by byte, 4 is by word
		command[22] = 0x04;
		// 写入数据的个数 -> Number of Write Data
		command[23] = (byte)(val_len / 2 / 256);
		command[24] = (byte)(val_len / 2 % 256);
	}
	else
	{
		// 写入方式，1是按位，2是按字 -> Write mode, 1 is bitwise, 2 is by word
		command[22] = 0x02;
		// 写入数据的个数 -> Number of Write Data
		command[23] = (byte)(val_len / 256);
		command[24] = (byte)(val_len % 256);
	}
	// DB块编号，如果访问的是DB块的话 -> DB block number, if you are accessing a DB block
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// 写入数据的类型 -> Types of writing data
	command[27] = address.data_code;
	// 偏移位置 -> Offset position
	command[28] = (byte)(address.address_start / 256 / 256 % 256); ;
	command[29] = (byte)(address.address_start / 256 % 256);
	command[30] = (byte)(address.address_start % 256);
	// 按字写入 -> Write by Word
	command[31] = 0x00;
	command[32] = 0x04;
	// 按位计算的长度 -> The length of the bitwise calculation
	command[33] = (byte)(val_len * 8 / 256);
	command[34] = (byte)(val_len * 8 % 256);

	if (value.data != NULL)
	{
		memcpy(command + 35, value.data, val_len);
		free(value.data);
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

	command[0] = 0x03;
	command[1] = 0x00;
	// 长度 -> length
	command[2] = (byte)((command_len) / 256);
	command[3] = (byte)((command_len) % 256);
	// 固定 -> fixed
	command[4] = 0x02;
	command[5] = 0xF0;
	command[6] = 0x80;
	command[7] = 0x32;
	// 命令 发 -> command to send
	command[8] = 0x01;
	// 标识序列号 -> Identification serial Number
	command[9] = 0x00;
	command[10] = 0x00;
	command[11] = 0x00;
	command[12] = 0x01;
	// 固定 -> fixed
	command[13] = 0x00;
	command[14] = 0x0E;
	// 写入长度+4 -> Write Length +4
	command[15] = (byte)((4 + buffer_len) / 256);
	command[16] = (byte)((4 + buffer_len) % 256);
	// 命令起始符 -> Command start character
	command[17] = 0x05;
	// 写入数据块个数 -> Number of data blocks written
	command[18] = 0x01;
	command[19] = 0x12;
	command[20] = 0x0A;
	command[21] = 0x10;
	// 写入方式，1是按位，2是按字 -> Write mode, 1 is bitwise, 2 is by word
	command[22] = 0x01;
	// 写入数据的个数 -> Number of Write Data
	command[23] = (byte)(buffer_len / 256);
	command[24] = (byte)(buffer_len % 256);
	// DB块编号，如果访问的是DB块的话 -> DB block number, if you are accessing a DB block
	command[25] = (byte)(address.db_block / 256);
	command[26] = (byte)(address.db_block % 256);
	// 写入数据的类型 -> Types of writing data
	command[27] = address.data_code;
	// 偏移位置 -> Offset position
	command[28] = (byte)(address.address_start / 256 / 256);
	command[29] = (byte)(address.address_start / 256);
	command[30] = (byte)(address.address_start % 256);
	// 按位写入 -> Bitwise Write
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
	// 按位计算的长度 -> The length of the bitwise calculation
	command[33] = (byte)(buffer_len / 256);
	command[34] = (byte)(buffer_len % 256);

	memcpy(command + 35, buffer, buffer_len);

	byte_array_info ret = { 0 };
	ret.data = command;
	ret.length = command_len;
	return ret;
}

/// <summary>
/// 读取BOOL时，根据S7协议的返回报文，正确提取出实际的数据内容
/// </summary>
/// <param name="response"></param>
/// <param name="ret"></param>
/// <returns></returns>
s7_error_code_e s7_analysis_read_bit(byte_array_info response, byte_array_info* ret)
{
	s7_error_code_e ret_code = S7_ERROR_CODE_OK;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	if (response.length >= 21 && response.data[20] == 1)
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
	s7_error_code_e ret_code = S7_ERROR_CODE_OK;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	int i = 0, j = 0;
	byte buffer[1024] = { 0 };
	int buffer_length = 0;
	if (response.length >= 21)
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
						memcpy(buffer + buffer_length, (void*)response.data[i + 5 + 3 * j], 2);
						buffer_length += 2;
					}
				}
				else
				{
					for (j = 0; j < count / 5; j++)
					{
						memcpy(buffer + buffer_length, (void*)response.data[i + 7 + 5 * j], 2);
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
		memset(ret->data, 0, buffer_length);
		memcpy(ret->data, buffer, buffer_length);
		ret->length = buffer_length;
	}
	else
	{
		ret_code = S7_ERROR_CODE_UNKOWN;
	}
	return ret_code;
}

s7_error_code_e s7_analysis_write(byte_array_info response)
{
	s7_error_code_e ret_code = S7_ERROR_CODE_OK;
	if (response.length == 0)
		return S7_ERROR_CODE_FAILED;

	byte buffer[1024] = { 0 };
	int buffer_length = 0;
	if (response.length >= 22)
	{
		byte code = response.data[21];
		if (code != 0xFF)
			ret_code = S7_ERROR_CODE_WRITE_ERROR;
	}
	else
	{
		ret_code = S7_ERROR_CODE_UNKOWN;
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
		byte temp[BUFFER_SIZE];
		memset(temp, 0, BUFFER_SIZE);

		int recv_size = socket_recv_data_one_loop(fd, temp, BUFFER_SIZE);
		int min = 21;
		if (recv_size >= min) //header size
		{
			ret->data = (byte*)malloc(recv_size);
			memset(ret->data, 0, recv_size);
			memcpy(ret->data, temp, recv_size);
			ret->length = recv_size;

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