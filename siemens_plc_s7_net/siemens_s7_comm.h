#ifndef __H_SIEMENS_S7_COMM_H__
#define __H_SIEMENS_S7_COMM_H__
#include "utill.h"

typedef struct _tag_siemens_s7_address_data {
	byte	data_code;			// 类型的代号值
	ushort	db_block;			// 获取或设置PLC的DB块数据信息
	int		address_start;		// 数字的起始地址，也就是偏移地址
	int		length;				// 读取的数据长度
}siemens_s7_address_data;

siemens_s7_address_data s7_analysis_address(const char* address, int length);

#endif//__H_SIEMENS_S7_COMM_H__