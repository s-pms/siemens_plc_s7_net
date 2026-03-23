/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2022-2026 wqliceman
 * GitHub: iceman
 * Email: wqliceman@gmail.com
 */

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
	S1200 = 1,		// S7-1200 series
	S300 = 2,		// S7-300 series
	S400 = 3,		// S7-400 series
	S1500 = 4,		// S7-1500 series
	S200Smart = 5,	// S7-200 Smart series
	S200 = 6		// S7-200 series (requires extra Ethernet module configuration)
} siemens_plc_types_e;

typedef enum _tag_s7_error_code {
	S7_ERROR_CODE_SUCCESS = 0,							// Success
	S7_ERROR_CODE_FAILED = 1,						// Generic failure
	S7_ERROR_CODE_FW_ERROR,							// PLC returned protocol-level error, see Fetch/Write docs
	S7_ERROR_CODE_ERROR_0006,						// Unsupported data type for current operation
	S7_ERROR_CODE_ERROR_000A,						// Attempted to read a non-existent DB block
	S7_ERROR_CODE_WRITE_ERROR,						// Write operation failed
	S7_ERROR_CODE_DB_SIZE_TOO_LARGE,				// DB block size cannot exceed 255
	S7_ERROR_CODE_READ_LENGTH_MAST_BE_EVEN,			// Read length must be even
	S7_ERROR_CODE_DATA_LENGTH_CHECK_FAILED,			// Data block length check failed (check PUT/GET and DB optimization)
	S7_ERROR_CODE_READ_LENGTH_OVER_PLC_ASSIGN,		// Read range exceeds PLC assignment
	S7_ERROR_CODE_READ_LENGTH_CANNT_LARAGE_THAN_19,	// Read array count must not exceed 19
	S7_ERROR_CODE_MALLOC_FAILED,					// Memory allocation failed
	S7_ERROR_CODE_INVALID_PARAMETER,				// Invalid parameter
	S7_ERROR_CODE_PARSE_ADDRESS_FAILED,				// Address parsing failed
	S7_ERROR_CODE_BUILD_CORE_CMD_FAILED,			// Failed to build core command
	S7_ERROR_CODE_SOCKET_SEND_FAILED,				// Failed to send command
	S7_ERROR_CODE_RESPONSE_HEADER_FAILED,			// Incomplete response header
	S7_ERROR_CODE_UNKOWN = 99,						// Unknown error
} s7_error_code_e;

#endif // !__H_TYPEDEF_H__