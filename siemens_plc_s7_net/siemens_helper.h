#ifndef __H_SIEMENS_HELPER_H__
#define __H_SIEMENS_HELPER_H__
#include "siemens_s7_comm.h"

byte_array_info build_read_byte_command(siemens_s7_address_data address);
byte_array_info build_read_bit_command(siemens_s7_address_data address);
byte_array_info build_write_byte_command(siemens_s7_address_data address, byte_array_info value);
byte_array_info build_write_bit_command(siemens_s7_address_data address, bool value);

s7_error_code_e s7_analysis_read_bit(byte_array_info resposne, byte_array_info* ret);
s7_error_code_e s7_analysis_read_byte(byte_array_info response, byte_array_info* ret);
s7_error_code_e s7_analysis_write(byte_array_info response);

bool read_data_from_core_server(int fd, byte_array_info send, byte_array_info* ret);
bool send_data_to_core_server(int fd, byte_array_info send);
bool try_send_data_to_server(int fd, byte_array_info* in_bytes, int* real_sends);

#endif//__H_SIEMENS_HELPER_H__