#ifndef __H_TYPEDEF_H__
#define __H_TYPEDEF_H__

#include <stdint.h>
#include <stdbool.h>

typedef unsigned char byte;
typedef unsigned short ushort;
typedef signed int	int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

typedef enum _tag_siemens_plc_type {
	S1200 = 1,		// 1200系列
	S300 = 2,		// 300系列
	S400 = 3,		// 400系列
	S1500 = 4,		// 1500系列PLC
	S200Smart = 5,	// 200的smart系列
	S200 = 6		// 200系统，需要额外配置以太网模块
} siemens_plc_types_e;

typedef enum _tag_s7_error_code {
	S7_ERROR_CODE_OK = 0,							// 成功
	S7_ERROR_CODE_FAILED = 1,						// 错误
	S7_ERROR_CODE_FW_ERROR,							// 发生了异常，具体信息查找Fetch/Write协议文档
	S7_ERROR_CODE_ERROR_0006,						// 当前操作的数据类型不支持
	S7_ERROR_CODE_ERROR_000A,						// 尝试读取不存在的DB块数据
	S7_ERROR_CODE_WRITE_ERROR,						// 写入数据异常
	S7_ERROR_CODE_DB_SIZE_TOO_LARGE,				// DB块数据无法大于255
	S7_ERROR_CODE_READ_LENGTH_MAST_BE_EVEN,			// 读取的数据长度必须为偶数
	S7_ERROR_CODE_DATA_LENGTH_CHECK_FAILED,			// 数据块长度校验失败，请检查是否开启put/get以及关闭db块优化
	S7_ERROR_CODE_READ_LENGTH_OVER_PLC_ASSIGN,		// 读取的数据范围超出了PLC的设定
	S7_ERROR_CODE_READ_LENGTH_CANNT_LARAGE_THAN_19,	// 读取的数组数量不允许大于19
	S7_ERROR_CODE_UNKOWN = 99,						// 未知错误
} s7_error_code_e;

#endif // !__H_TYPEDEF_H__
