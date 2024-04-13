#ifndef __UTILL_H__
#define __UTILL_H__

#include "typedef.h"

#define RELEASE_DATA(data) do { if ((data) != NULL) { free(data); (data) = NULL; } } while(0)

typedef struct _tag_byte_array_info {
	byte* data;	// 内容
	int length; // 长度
}byte_array_info;

typedef struct _tag_bool_array_info {
	bool* data;	// 内容
	int length; // 长度
}bool_array_info;

void short2bytes(short i, byte* bytes);
short bytes2short(byte* bytes);

void ushort2bytes(ushort i, byte* bytes);
ushort bytes2ushort(byte* bytes);

void int2bytes(int32 i, byte* bytes);
int32 bytes2int32(byte* bytes);

void uint2bytes(uint32 i, byte* bytes);
uint32 bytes2uint32(byte* bytes);

void bigInt2bytes(int64 i, byte* bytes);
int64 bytes2bigInt(byte* bytes);

void ubigInt2bytes(uint64 i, byte* bytes);
uint64 bytes2ubigInt(byte* bytes);

void float2bytes(float i, byte* bytes);
float bytes2float(byte* bytes);

void double2bytes(double i, byte* bytes);
double bytes2double(byte* bytes);

int str_to_int(const char* address);
void str_toupper(char* input);
void str_tolower(char* input);
int str_start_with(const char* origin, char* prefix);



uint32 htonf_(float value);
float ntohf_(uint32 value);
uint64 htond_(double value);
double ntohd_(uint64 value);
uint64 htonll_(uint64 Value);
uint64 ntohll_(uint64 Value);

#ifndef _WIN32
char* itoa(unsigned long long  value, char str[], int radix);
#endif // !_WIN32

#endif
