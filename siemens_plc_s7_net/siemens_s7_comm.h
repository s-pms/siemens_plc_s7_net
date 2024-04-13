#ifndef __H_SIEMENS_S7_COMM_H__
#define __H_SIEMENS_S7_COMM_H__
#include "utill.h"

#define MAX_RETRY_TIMES 3				// 最大重试次数
#define MIN_HEADER_SIZE 21				// 根据协议定义
#define BUFFER_SIZE 1024

typedef struct _tag_siemens_s7_address_data {
	byte	data_code;			// 类型的代号值
	ushort	db_block;			// 获取或设置PLC的DB块数据信息
	int		address_start;		// 数字的起始地址，也就是偏移地址
	int		length;				// 读取的数据长度
}siemens_s7_address_data;

bool s7_analysis_address(const char* address, int length, siemens_s7_address_data* address_data);

#endif//__H_SIEMENS_S7_COMM_H__