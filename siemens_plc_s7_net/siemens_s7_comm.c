#include "siemens_s7_comm.h"
#include <stdlib.h>
#include <string.h>
#include "dynstr.h"

/// <summary>
/// 计算特殊的地址信息
/// </summary>
/// <param name="address">字符串地址</param>
/// <param name="isCT">是否是定时器和计数器的地址</param>
/// <returns></returns>
int calculate_address_started(const char* address, bool isCT)
{
	int addr_len = strlen(address);
	int ret_count = 0;

	int ret = dynstr_find(address, addr_len, ".", 1);
	if (ret < 0)  // 未找到 -1
	{
		ret_count = isCT ? str_to_int(address) : str_to_int(address) * 8;
	}
	else
	{
		int temp_split_count = 0;
		dynstr* ret_splits = dynstr_split(address, addr_len, ".", 1, &temp_split_count);
		ret_count = str_to_int(ret_splits[0]) * 8 + str_to_int(ret_splits[1]);

		dynstr_freesplitres(ret_splits, temp_split_count);
	}
	return ret_count;
}

siemens_s7_address_data s7_analysis_address(const char* address, int length)
{
	siemens_s7_address_data address_data = { 0 };
	address_data.length = length;
	address_data.db_block = 0;

	dynstr temp_address = dynstr_new(address);
	str_toupper(temp_address);

	if (0 == str_start_with(temp_address, "AI"))
	{
		address_data.data_code = 0x06;
		int sub_str_len = 2;

		if (0 == str_start_with(temp_address, "AIX") ||
			0 == str_start_with(temp_address, "AIB") ||
			0 == str_start_with(temp_address, "AIW") ||
			0 == str_start_with(temp_address, "AID"))
		{
			sub_str_len = 3;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, false);
	}
	else if (0 == str_start_with(temp_address, "AQ"))
	{
		address_data.data_code = 0x07;
		int sub_str_len = 2;

		if (0 == str_start_with(temp_address, "AQX") ||
			0 == str_start_with(temp_address, "AQB") ||
			0 == str_start_with(temp_address, "AQW") ||
			0 == str_start_with(temp_address, "AQD"))
		{
			sub_str_len = 3;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, false);
	}
	else if (0 == str_start_with(temp_address, "I"))
	{
		address_data.data_code = 0x81;
		int sub_str_len = 1;

		if (0 == str_start_with(temp_address, "IX") ||
			0 == str_start_with(temp_address, "IB") ||
			0 == str_start_with(temp_address, "IW") ||
			0 == str_start_with(temp_address, "ID"))
		{
			sub_str_len = 2;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, false);
	}
	else if (0 == str_start_with(temp_address, "Q"))
	{
		address_data.data_code = 0x82;
		int sub_str_len = 1;

		if (0 == str_start_with(temp_address, "QX") ||
			0 == str_start_with(temp_address, "QB") ||
			0 == str_start_with(temp_address, "QW") ||
			0 == str_start_with(temp_address, "QD"))
		{
			sub_str_len = 2;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, false);
	}
	else if (0 == str_start_with(temp_address, "M"))
	{
		address_data.data_code = 0x83;
		int sub_str_len = 1;

		if (0 == str_start_with(temp_address, "MX") ||
			0 == str_start_with(temp_address, "MB") ||
			0 == str_start_with(temp_address, "MW") ||
			0 == str_start_with(temp_address, "MD"))
		{
			sub_str_len = 2;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, false);
	}
	else if (0 == str_start_with(temp_address, "D") || 0 == str_start_with(temp_address, "DB"))
	{
		address_data.data_code = 0x84;
		int sub_str_len = 1;

		int temp_split_count = 0;
		dynstr* ret_splits = dynstr_split(temp_address, strlen(temp_address), ".", 1, &temp_split_count);
		if (0 == str_start_with(ret_splits[0], "DB"))
			sub_str_len = 2;

		dynstr_range(ret_splits[0], sub_str_len, -1);
		address_data.db_block = str_to_int(ret_splits[0]);

		// 后缀码
		if (temp_split_count > 1)
		{
			sub_str_len = 0;
			if (0 == str_start_with(ret_splits[1], "DBX") ||
				0 == str_start_with(ret_splits[1], "DBB") ||
				0 == str_start_with(ret_splits[1], "DBW") ||
				0 == str_start_with(ret_splits[1], "DBD"))
			{
				sub_str_len = 3;
			}

			dynstr temp_addr = ret_splits[1];
			if (temp_split_count > 2)
			{
				dynstr temp_addr2 = dynstr_cat(ret_splits[1], ".");
				temp_addr = dynstr_cat_dynstr(temp_addr2, ret_splits[2]);
				dynstr_free(temp_addr2);
			}

			dynstr_range(temp_addr, sub_str_len, -1);
			address_data.address_start = calculate_address_started(temp_addr, false);
			dynstr_free(temp_addr);

			dynstr_freesplitres(ret_splits,temp_split_count);
		}
	}
	else if (0 == str_start_with(temp_address, "T"))
	{
		address_data.data_code = 0x1F;
		int sub_str_len = 1;

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, true);
	}
	else if (0 == str_start_with(temp_address, "C"))
	{
		address_data.data_code = 0x1E;
		int sub_str_len = 1;

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, true);
	}
	else if (0 == str_start_with(temp_address, "V"))
	{
		address_data.data_code = 0x84;
		address_data.db_block = 1;
		int sub_str_len = 1;
		if (0 == str_start_with(temp_address, "VX") ||
			0 == str_start_with(temp_address, "VB") ||
			0 == str_start_with(temp_address, "VW") ||
			0 == str_start_with(temp_address, "VD"))
		{
			sub_str_len = 2;
		}

		dynstr_range(temp_address, sub_str_len, -1);
		address_data.address_start = calculate_address_started(temp_address, true);
	}

	dynstr_free(temp_address);
	return address_data;
}