#include "utill.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define _WS2_32_WINSOCK_SWAP_LONG(l)                \
            ( ( ((l) >> 24) & 0x000000FFL ) |       \
              ( ((l) >>  8) & 0x0000FF00L ) |       \
              ( ((l) <<  8) & 0x00FF0000L ) |       \
              ( ((l) << 24) & 0xFF000000L ) )

#define _WS2_32_WINSOCK_SWAP_LONGLONG(l)            \
            ( ( ((l) >> 56) & 0x00000000000000FFLL ) |       \
              ( ((l) >> 40) & 0x000000000000FF00LL ) |       \
              ( ((l) >> 24) & 0x0000000000FF0000LL ) |       \
              ( ((l) >>  8) & 0x00000000FF000000LL ) |       \
              ( ((l) <<  8) & 0x000000FF00000000LL ) |       \
              ( ((l) << 24) & 0x0000FF0000000000LL ) |       \
              ( ((l) << 40) & 0x00FF000000000000LL ) |       \
              ( ((l) << 56) & 0xFF00000000000000LL ) )

void short2bytes(short i, byte* bytes) {
    bytes[0] = (byte)(i & 0xFF);
    bytes[1] = (byte)((i >> 8) & 0xFF);
}

short bytes2short(byte* bytes) {
    return (short)(bytes[0] & 0xFF | (bytes[1] << 8));
}

void ushort2bytes(ushort i, byte* bytes) {
    bytes[0] = (byte)(i & 0xFF);
    bytes[1] = (byte)((i >> 8) & 0xFF);
}

ushort bytes2ushort(byte* bytes) {
    return (ushort)(bytes[0] & 0xFF | (bytes[1] << 8));
}

void int2bytes(int32 i, byte* bytes) {
    for (int j = 0; j < 4; j++) {
        bytes[j] = (byte)((i >> (j * 8)) & 0xFF);
    }
}

int32 bytes2int32(byte* bytes) {
    int32 iRetVal = 0;
    for (int j = 0; j < 4; j++) {
        iRetVal |= ((int32)bytes[j] << (j * 8));
    }
    return iRetVal;
}

void uint2bytes(uint32 i, byte* bytes) {
    for (int j = 0; j < 4; j++) {
        bytes[j] = (byte)((i >> (j * 8)) & 0xFF);
    }
}

uint32 bytes2uint32(byte* bytes) {
    uint32 iRetVal = 0;
    for (int j = 0; j < 4; j++) {
        iRetVal |= ((uint32)bytes[j] << (j * 8));
    }
    return iRetVal;
}

void bigInt2bytes(int64 i, byte* bytes) {
    for (int j = 0; j < 8; j++) {
        bytes[j] = (byte)((i >> (j * 8)) & 0xFF);
    }
}

int64 bytes2bigInt(byte* bytes) {
    int64 iRetVal = 0;
    for (int j = 0; j < 8; j++) {
        iRetVal |= ((int64)bytes[j] << (j * 8));
    }
    return iRetVal;
}

void ubigInt2bytes(uint64 i, byte* bytes) {
    for (int j = 0; j < 8; j++) {
        bytes[j] = (byte)((i >> (j * 8)) & 0xFF);
    }
}

uint64 bytes2ubigInt(byte* bytes) {
    uint64 iRetVal = 0;
    for (int j = 0; j < 8; j++) {
        iRetVal |= ((uint64)bytes[j] << (j * 8));
    }
    return iRetVal;
}

void float2bytes(float i, byte* bytes) {
    int temp = *(int*)&i;
    int2bytes(temp, bytes);
}

float bytes2float(byte* bytes) {
    int temp = bytes2int32(bytes);
    return *(float*)&temp;
}

void double2bytes(double i, byte* bytes) {
    int64 temp = *(int64*)&i;
    bigInt2bytes(temp, bytes);
}

double bytes2double(byte* bytes) {
    int64 temp = bytes2bigInt(bytes);
    return *(double*)&temp;
}

int str_to_int(const char* address)
{
	int ret = 0;
	ret = (int)strtol(address, NULL, 10);
	return ret;
}

void str_toupper(char* input)
{
	if (input == NULL)
		return;

	int32 len = strlen(input), i = 0;
	for (; i < len; i++)
		input[i] = toupper(input[i]);
}

void str_tolower(char* input)
{
	if (input == NULL)
		return;

	int32 len = strlen(input), i = 0;
	for (; i < len; i++)
		input[i] = tolower(input[i]);
}

/**
* ×Ö·û´®originÒÔ×Ö·û´®prefix¿ªÍ·£¬·µ»Ø0£»·ñÔò·µ»Ø1£»Òì³£·µ»Ø-1
*/
int str_start_with(const char* origin, char* prefix)
{
	if (origin == NULL ||
		prefix == NULL ||
		strlen(prefix) > strlen(origin))
	{
		return -1;
	}

	int n = strlen(prefix), i;
	for (i = 0; i < n; i++) {
		if (origin[i] != prefix[i]) {
			return 1;
		}
	}
	return 0;
}

/**
* ×Ö·û´®originÒÔ×Ö·û´®end½áÎ²£¬·µ»Ø0£»·ñÔò·µ»Ø1£»Òì³£·µ»Ø-1
*/
int str_end_with(const char* origin, char* end)
{
	if (origin == NULL ||
		end == NULL ||
		strlen(end) > strlen(origin))
	{
		return -1;
	}

	int n = strlen(end);
	int m = strlen(origin);
	int i;
	for (i = 0; i < n; i++)
	{
		if (origin[m - i - 1] != end[n - i - 1])
			return 1;
	}
	return 0;
}

uint32 htonf_(float value)
{
	uint32 Tempval;
	uint32 Retval;
	Tempval = *(uint32*)(&value);
	Retval = _WS2_32_WINSOCK_SWAP_LONG(Tempval);
	return Retval;
}

float ntohf_(uint32 value)
{
	const uint32 Tempval = _WS2_32_WINSOCK_SWAP_LONG(value);
	float Retval;
	*((uint32*)&Retval) = Tempval;
	return Retval;
}

uint64 htond_(double value)
{
	uint64 Tempval;
	uint64 Retval;
	Tempval = *(uint64*)(&value);
	Retval = _WS2_32_WINSOCK_SWAP_LONGLONG(Tempval);
	return Retval;
}

double ntohd_(uint64 value)
{
	const uint64 Tempval = _WS2_32_WINSOCK_SWAP_LONGLONG(value);
	double Retval;
	*((uint64*)&Retval) = Tempval;
	return Retval;
}

uint64 htonll_(uint64 Value)
{
	const uint64 Retval = _WS2_32_WINSOCK_SWAP_LONGLONG(Value);
	return Retval;
}

uint64 ntohll_(uint64 Value)
{
	const uint64 Retval = _WS2_32_WINSOCK_SWAP_LONGLONG(Value);
	return Retval;
}

#ifndef _WIN32
/*
=============
itoa

Convert integer to string

PARAMS:
- value     A 64-bit number to convert
- str       Destination buffer; should be 66 characters long for radix2, 24 - radix8, 22 - radix10, 18 - radix16.
- radix     Radix must be in range -36 .. 36. Negative values used for signed numbers.
=============
*/

char* itoa(unsigned long long  value, char str[], int radix)
{
	char        buf[66];
	char* dest = buf + sizeof(buf);
	bool     sign = false;

	if (value == 0) {
		memcpy(str, "0", 2);
		return str;
	}

	if (radix < 0) {
		radix = -radix;
		if ((long long)value < 0) {
			value = -value;
			sign = true;
		}
	}

	*--dest = '\0';

	switch (radix)
	{
	case 16:
		while (value) {
			*--dest = '0' + (value & 0xF);
			if (*dest > '9') *dest += 'A' - '9' - 1;
			value >>= 4;
		}
		break;
	case 10:
		while (value) {
			*--dest = '0' + (value % 10);
			value /= 10;
		}
		break;

	case 8:
		while (value) {
			*--dest = '0' + (value & 7);
			value >>= 3;
		}
		break;

	case 2:
		while (value) {
			*--dest = '0' + (value & 1);
			value >>= 1;
		}
		break;

	default:            // The slow version, but universal
		while (value) {
			*--dest = '0' + (value % radix);
			if (*dest > '9') *dest += 'A' - '9' - 1;
			value /= radix;
		}
		break;
	}

	if (sign) *--dest = '-';

	memcpy(str, dest, buf + sizeof(buf) - dest);
	return str;
}
#endif