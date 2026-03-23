/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2022-2026 wqliceman
 * GitHub: iceman
 * Email: wqliceman@gmail.com
 */

#ifndef __H_SIEMENS_S7_COMM_H__
#define __H_SIEMENS_S7_COMM_H__
#include "utill.h"

#define MAX_RETRY_TIMES 3				// Maximum retry count
#define MIN_HEADER_SIZE 21				// Defined by protocol specification
#define BUFFER_SIZE 1024

typedef struct _tag_siemens_s7_address_data {
	byte	data_code;			// Data type code
	ushort	db_block;			// PLC DB block number
	int		address_start;		// Start address (offset address)
	int		length;				// Data length to read
}siemens_s7_address_data;

bool s7_analysis_address(const char* address, int length, siemens_s7_address_data* address_data);

#endif//__H_SIEMENS_S7_COMM_H__