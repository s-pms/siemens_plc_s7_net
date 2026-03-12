#include "siemens_s7_comm.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/// <summary>
/// 计算特殊的地址信息
/// </summary>
/// <param name="address">字符串地址</param>
/// <param name="isCT">是否是定时器和计数器的地址</param>
/// <returns></returns>
static bool is_decimal_text(const char* text)
{
	// Address segments are numeric-only; reject mixed tokens early to avoid silent truncation.
	if (text == NULL || text[0] == '\0')
		return false;

	for (int i = 0; text[i] != '\0'; ++i)
	{
		if (!isdigit((unsigned char)text[i]))
			return false;
	}

	return true;
}

static bool calculate_address_started(const char* address, bool isCT, int* ret_count)
{
	if (address == NULL || ret_count == NULL)
		return false;

	int addr_len = strlen(address);
	if (addr_len <= 0)
		return false;

	const char* dot = strchr(address, '.');
	if (dot == NULL)
	{
		if (!is_decimal_text(address))
			return false;

		*ret_count = isCT ? str_to_int(address) : str_to_int(address) * 8;
	}
	else
	{
		// Bit-address form must be exactly "byte.bit" and bit must stay within 0-7.
		if (isCT || dot == address || dot[1] == '\0' || strchr(dot + 1, '.') != NULL)
			return false;

		int byte_index_len = (int)(dot - address);
		char* byte_index_text = (char*)malloc((size_t)byte_index_len + 1);
		if (byte_index_text == NULL)
		{
			return false;
		}

		memcpy(byte_index_text, address, (size_t)byte_index_len);
		byte_index_text[byte_index_len] = '\0';

		if (!is_decimal_text(byte_index_text) || !is_decimal_text(dot + 1))
		{
			free(byte_index_text);
			return false;
		}

		int bit_index = str_to_int(dot + 1);
		if (bit_index < 0 || bit_index > 7)
		{
			free(byte_index_text);
			return false;
		}

		*ret_count = str_to_int(byte_index_text) * 8 + bit_index;
		free(byte_index_text);
	}
	return true;
}

bool s7_analysis_address(const char* address, int length, siemens_s7_address_data* address_data)
{
	if (address == NULL || length <= 0 || address_data == NULL)
		return false;

	int address_len = (int)strlen(address);
	if (address_len <= 0)
		return false;

	address_data->length = length;
	address_data->data_code = 0;
	address_data->db_block = 0;
	address_data->address_start = 0;

	char* normalized = (char*)malloc((size_t)address_len + 1);
	if (normalized == NULL)
		return false;

	memcpy(normalized, address, (size_t)address_len + 1);
	// Parse on a normalized uppercase copy so all edge-case validation uses one code path.
	str_toupper(normalized);

	bool matched = true;
	int prefix_len = 0;
	bool is_counter_timer = false;

	if (strncmp(normalized, "AI", 2) == 0) {
		address_data->data_code = 0x06;
		prefix_len = 2;
	}
	else if (strncmp(normalized, "AQ", 2) == 0) {
		address_data->data_code = 0x07;
		prefix_len = 2;
	}
	else if (strncmp(normalized, "DB", 2) == 0) {
		address_data->data_code = 0x84;
		prefix_len = 2;
	}
	else if (strncmp(normalized, "I", 1) == 0) {
		address_data->data_code = 0x81;
		prefix_len = 1;
	}
	else if (strncmp(normalized, "Q", 1) == 0) {
		address_data->data_code = 0x82;
		prefix_len = 1;
	}
	else if (strncmp(normalized, "M", 1) == 0) {
		address_data->data_code = 0x83;
		prefix_len = 1;
	}
	else if (strncmp(normalized, "D", 1) == 0) {
		address_data->data_code = 0x84;
		prefix_len = 1;
	}
	else if (strncmp(normalized, "T", 1) == 0) {
		address_data->data_code = 0x1F;
		prefix_len = 1;
		is_counter_timer = true;
	}
	else if (strncmp(normalized, "C", 1) == 0) {
		address_data->data_code = 0x1E;
		prefix_len = 1;
		is_counter_timer = true;
	}
	else if (strncmp(normalized, "V", 1) == 0) {
		address_data->data_code = 0x84;
		prefix_len = 1;
	}
	else {
		matched = false;
	}

	if (matched && address_data->data_code == 0x84 && (prefix_len == 1 || prefix_len == 2) && (normalized[0] == 'D'))
	{
		// DB addresses carry both block number and optional DBX/DBB/DBW/DBD suffix.
		char* dot = strchr(normalized, '.');
		char* data_part = NULL;
		char saved = '\0';

		if (dot != NULL) {
			saved = *dot;
			*dot = '\0';
			data_part = dot + 1;
		}

		if (!is_decimal_text(normalized + prefix_len)) {
			matched = false;
		}
		else {
			address_data->db_block = str_to_int(normalized + prefix_len);
		}

		if (dot != NULL) {
			*dot = saved;
			if (matched) {
				if (strncmp(data_part, "DBX", 3) == 0 ||
					strncmp(data_part, "DBB", 3) == 0 ||
					strncmp(data_part, "DBW", 3) == 0 ||
					strncmp(data_part, "DBD", 3) == 0) {
					data_part += 3;
				}

				if (!calculate_address_started(data_part, false, &address_data->address_start)) {
					matched = false;
				}
			}
		}
	}
	else if (matched)
	{
		if ((strncmp(normalized, "AIX", 3) == 0) ||
			(strncmp(normalized, "AIB", 3) == 0) ||
			(strncmp(normalized, "AIW", 3) == 0) ||
			(strncmp(normalized, "AID", 3) == 0) ||
			(strncmp(normalized, "AQX", 3) == 0) ||
			(strncmp(normalized, "AQB", 3) == 0) ||
			(strncmp(normalized, "AQW", 3) == 0) ||
			(strncmp(normalized, "AQD", 3) == 0) ||
			(strncmp(normalized, "IX", 2) == 0) ||
			(strncmp(normalized, "IB", 2) == 0) ||
			(strncmp(normalized, "IW", 2) == 0) ||
			(strncmp(normalized, "ID", 2) == 0) ||
			(strncmp(normalized, "QX", 2) == 0) ||
			(strncmp(normalized, "QB", 2) == 0) ||
			(strncmp(normalized, "QW", 2) == 0) ||
			(strncmp(normalized, "QD", 2) == 0) ||
			(strncmp(normalized, "MX", 2) == 0) ||
			(strncmp(normalized, "MB", 2) == 0) ||
			(strncmp(normalized, "MW", 2) == 0) ||
			(strncmp(normalized, "MD", 2) == 0) ||
			(strncmp(normalized, "VX", 2) == 0) ||
			(strncmp(normalized, "VB", 2) == 0) ||
			(strncmp(normalized, "VW", 2) == 0) ||
			(strncmp(normalized, "VD", 2) == 0)) {
			prefix_len++;
		}

		if (!calculate_address_started(normalized + prefix_len, is_counter_timer, &address_data->address_start)) {
			matched = false;
		}
	}

	free(normalized);
	return matched;
}