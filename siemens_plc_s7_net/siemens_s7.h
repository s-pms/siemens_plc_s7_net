#ifndef	__H_SIEMENS_S7_H__
#define __H_SIEMENS_S7_H__

#include "typedef.h"

/////////////////////////////////////////////////////////////

byte get_plc_slot();
void set_plc_slot(byte slot);

byte get_plc_rack();
void set_plc_rack(byte rack);

byte get_plc_connection_type();
void set_plc_connection_type(byte rack);

int get_plc_local_TSAP();
void set_plc_local_TSAP(int tasp);

int get_plc_dest_TSAP();
void set_plc_dest_TSAP(int tasp);

int get_plc_PDU_length();

/////////////////////////////////////////////////////////////

bool s7_connect(char* ip_addr, int port, siemens_plc_types_e plc, int* fd);
bool s7_disconnect(int fd);

//read
s7_error_code_e s7_read_bool(int fd, const char* address, bool* val);
s7_error_code_e s7_read_short(int fd, const char* address, short* val);
s7_error_code_e s7_read_ushort(int fd, const char* address, ushort* val);
s7_error_code_e s7_read_int32(int fd, const char* address, int32* val);
s7_error_code_e s7_read_uint32(int fd, const char* address, uint32* val);
s7_error_code_e s7_read_int64(int fd, const char* address, int64* val);
s7_error_code_e s7_read_uint64(int fd, const char* address, uint64* val);
s7_error_code_e s7_read_float(int fd, const char* address, float* val);
s7_error_code_e s7_read_double(int fd, const char* address, double* val);
s7_error_code_e s7_read_string(int fd, const char* address, int length, char** val); //need free val

//write
s7_error_code_e s7_write_bool(int fd, const char* address, bool val);
s7_error_code_e s7_write_short(int fd, const char* address, short val);
s7_error_code_e s7_write_ushort(int fd, const char* address, ushort val);
s7_error_code_e s7_write_int32(int fd, const char* address, int32 val);
s7_error_code_e s7_write_uint32(int fd, const char* address, uint32 val);
s7_error_code_e s7_write_int64(int fd, const char* address, int64 val);
s7_error_code_e s7_write_uint64(int fd, const char* address, uint64 val);
s7_error_code_e s7_write_float(int fd, const char* address, float val);
s7_error_code_e s7_write_double(int fd, const char* address, double val);
s7_error_code_e s7_write_string(int fd, const char* address, int length, const char* val);

//
bool s7_remote_run(int fd);
bool s7_remote_stop(int fd);
bool s7_remote_reset(int fd);
char* s7_read_plc_type(int fd);

#endif //__H_SIEMENS_S7_H__