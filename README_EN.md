# Overall Program Introduction

- Project Name: siemens_plc_s7_net
- Development Language: C
- Supported Operating Systems: Windows/Linux
- Test Device: S1200
Currently implemented functionality includes a Siemens PLC communication class utilizing the S7 protocol. Configuration of the Ethernet module on the PLC side is required beforehand.

## Header Files

```c
#include "siemens_s7.h"  // Provides method interfaces for the protocol
#include "typedef.h"   // Contains some macro definitions for types
```

## Siemens PLC Address Description

### Connection Attributes

- port: Port number, typically 102
- plc_type: PLC model, such as S200, S200Smart, S300, S400, S1200, S1500

### PLC Address Classification

Code values for types (soft element codes used to distinguish soft element types, e.g., D, R)

| Serial No.  | Description         | Address Type |
| :---: | :------------- | -------- |
|   1   | Intermediate relay        | M        |
|   2   | Input relay               | I        |
|   3   | Output relay (Q)          | Q        |
|   4   | DB block register (DB)    | DB       |
|   5   | V register                | V        |
|   6   | Timer value               | T        |
|   7   | Counter value             | C        |
|   8   | Intelligent input relay   | AI       |
|   9   | Intelligent output relay  | AQ       |

## Implemented Methods

### 1. Connecting to PLC Devices

```c
bool s7_connect(char* ip_addr, int port, siemens_plc_types_e plc, int* fd);
/* Connects to a PLC device
 * Parameters:
 *   ip_addr: The IP address of the PLC
 *   port: The port number of the PLC
 *   plc: The model of the PLC
 *   fd: Upon successful connection, returns the file descriptor
 * Return Value:
 *   Returns true if the connection succeeds, false otherwise
 */

bool s7_disconnect(int fd);
/* Disconnects from the PLC
 * Parameters:
 *   fd: The file descriptor used to connect to the PLC
 * Return Value:
 *   Returns true if the disconnection succeeds, false otherwise
 */

byte get_plc_slot();
/* Retrieves the slot number of the PLC
 * Return Value:
 *   Returns the slot number of the PLC
 */

void set_plc_slot(byte slot);
/* Sets the slot number of the PLC
 * Parameters:
 *   slot: The desired slot number to set
 */

byte get_plc_rack();
/* Retrieves the rack number of the PLC
 * Return Value:
 *   Returns the rack number of the PLC
 */

void set_plc_rack(byte rack);
/* Sets the rack number of the PLC
 * Parameters:
 *   rack: The desired rack number to set
 */

byte get_plc_connection_type();
/* Retrieves the connection type of the PLC
 * Return Value:
 *   Returns the connection type of the PLC
 */

void set_plc_connection_type(byte rack);
/* Sets the connection type of the PLC
 * Parameters:
 *   rack: The desired connection type to set
 */

int get_plc_local_TSAP();
/* Retrieves the local TSAP of the PLC
 * Return Value:
 *   Returns the local TSAP of the PLC
 */

void set_plc_local_TSAP(int tasp);
/* Sets the local TSAP of the PLC
 * Parameters:
 *   tasp: The desired local TSAP to set
 */

int get_plc_dest_TSAP();
/* Retrieves the destination TSAP of the PLC
 * Return Value:
 *   Returns the destination TSAP of the PLC
 */

void set_plc_dest_TSAP(int tasp);
/* Sets the destination TSAP of the PLC
 * Parameters:
 *   tasp: The desired destination TSAP to set
 */

int get_plc_PDU_length();
/* Retrieves the PDU length of the PLC
 * Return Value:
 *   Returns the PDU length of the PLC
 */
```

### 2. Reading Data

```c
s7_error_code_e s7_read_bool(int fd, const char* address, bool* val);
/* Reads a boolean value from the PLC
 * Parameters:
 *   fd: The file descriptor for the PLC connection
 *   address: The address of the data in the PLC
 *   val: Pointer to receive the read data
 * Return Value:
 *   Returns S7_ERROR_CODE_SUCCESS if the read operation succeeds; otherwise, returns the corresponding error code
 */

// Similar function declarations for reading other data types (e.g., byte, short, etc.) are omitted for brevity

s7_error_code_e s7_read_string(int fd, const char* address, int length, char** val); 
/* Reads a string from the PLC. The memory for the returned string must be manually freed.
 * Parameters:
 *   fd: The file descriptor for the PLC connection
 *   address: The address of the data in the PLC
 *   length: The length of the string to read
 *   val: Pointer to receive the read string; memory should be freed after use
 * Return Value:
 *   Returns S7_ERROR_CODE_SUCCESS if the read operation succeeds; otherwise, returns the corresponding error code
 */
```

### 3. Writing Data

```c
s7_error_code_e s7_write_bool(int fd, const char* address, bool val);
/* Writes a boolean value to the PLC
 * Parameters:
 *   fd: The file descriptor for the PLC connection
 *   address: The address of the data in the PLC
 *   val: The value to write
 * Return Value:
 *   Returns S7_ERROR_CODE_SUCCESS if the write operation succeeds; otherwise, returns the corresponding error code
 */

// Similar function declarations for writing other data types (e.g., byte, short, etc.) are omitted for brevity

s7_error_code_e s7_write_string(int fd, const char* address, int length, const char* val);
/* Writes a string to the PLC
 * Parameters:
 *   fd: The file descriptor for the PLC connection
 *   address: The address of the data in the PLC
 *   length: The length of the string to write
 *   val: The string to write
 * Return Value:
 *   Returns S7_ERROR_CODE_SUCCESS if the write operation succeeds; otherwise, returns the corresponding error code
 */
```

## Usage Example

For the complete example, refer to the main.c file in the code. Below is the main code and usage method:

Read addresses in the format "M100", "DB100"

```c
#ifdef _WIN32
#include <WinSock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#pragma warning( disable : 4996)

#define GET_RESULT(ret){ if(!ret) faild_count++; }

#include "siemens_s7.h"

int main(int argc, char** argv)
{
#ifdef _WIN32
 WSADATA wsa;
 if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
 {
  return -1;
 }
#endif

 char* plc_ip = "192.168.123.170";
 int plc_port = 102;
 if (argc > 1)
 {
  plc_ip = argv[1];
  plc_port = atoi(argv[2]);
 }

 int fd = -1;
 bool ret = s7_connect(plc_ip, plc_port, S1200, &fd);
 if (ret && fd > 0)
 {
  s7_error_code_e ret = S7_ERROR_CODE_FAILED;

  char* type = s7_read_plc_type(fd);
  printf("plc type: %s\n", type);
  free(type);

  const int TEST_COUNT = 5000;
  const int TEST_SLEEP_TIME = 1000;
  int faild_count = 0;
  char address[50] = { 0 };
  int i = 0;

  for (i = 0; i < TEST_COUNT; i++)
  {
   printf("==============Test count: %d==============\n", i + 1);
   bool all_success = false;
   //////////////////////////////////////////////////////////////////////////
   bool val = true;
   strcpy(address, "MX100");
   ret = s7_write_bool(fd, address, val);
   printf("Write\t %s \tbool:\t %d, \tret: %d\n", address, val, ret);
   GET_RESULT(ret);

   val = false;
   ret = s7_read_bool(fd, address, &val);
   printf("Read\t %s \tbool:\t %d\n", address, val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   short w_s_val = 23;
   strcpy(address, "MW100");
   ret = s7_write_short(fd, address, w_s_val);
   printf("Write\t %s \tshort:\t %d, \tret: %d\n", address, w_s_val, ret);
   GET_RESULT(ret);

   short s_val = 0;
   ret = s7_read_short(fd, address, &s_val);
   printf("Read\t %s \tshort:\t %d\n", address, s_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   ushort w_us_val = 255;
   strcpy(address, "MW100");
   ret = s7_write_ushort(fd, address, w_us_val);
   printf("Write\t %s \tushort:\t %d, \tret: %d\n", address, w_us_val, ret);
   GET_RESULT(ret);

   ushort us_val = 0;
   ret = s7_read_ushort(fd, address, &us_val);
   printf("Read\t %s \tushort:\t %d\n", address, us_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   int32 w_i_val = 12345;
   strcpy(address, "DB1.70");
   ret = s7_write_int32(fd, address, w_i_val);
   printf("Write\t %s \tint32:\t %d, \tret: %d\n", address, w_i_val, ret);
   GET_RESULT(ret);

   int i_val = 0;
   ret = s7_read_int32(fd, address, &i_val);
   printf("Read\t %s \tint32:\t %d\n", address, i_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   uint32 w_ui_val = 22345;
   ret = s7_write_uint32(fd, address, w_ui_val);
   printf("Write\t %s \tuint32:\t %d, \tret: %d\n", address, w_ui_val, ret);
   GET_RESULT(ret);

   uint32 ui_val = 0;
   ret = s7_read_uint32(fd, address, &ui_val);
   printf("Read\t %s \tuint32:\t %d\n", address, ui_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   int64 w_i64_val = 333334554;
   strcpy(address, "DB1.DBW70");
   ret = s7_write_int64(fd, address, w_i64_val);
   printf("Write\t %s \tuint64:\t %lld, \tret: %d\n", address, w_i64_val, ret);
   GET_RESULT(ret);

   int64 i64_val = 0;
   ret = s7_read_int64(fd, address, &i64_val);
   printf("Read\t %s \tint64:\t %lld\n", address, i64_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   uint64 w_ui64_val = 4333334554;
   strcpy(address, "DB1.DBW70");
   ret = s7_write_uint64(fd, address, w_ui64_val);
   printf("Write\t %s \tuint64:\t %lld, \tret: %d\n", address, w_ui64_val, ret);
   GET_RESULT(ret);

   int64 ui64_val = 0;
   ret = s7_read_uint64(fd, address, &ui64_val);
   printf("Read\t %s \tuint64:\t %lld\n", address, ui64_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   float w_f_val = 32.454f;
   strcpy(address, "DB1.DBW70");
   ret = s7_write_float(fd, address, w_f_val);
   printf("Write\t %s \tfloat:\t %f, \tret: %d\n", address, w_f_val, ret);
   GET_RESULT(ret);

   float f_val = 0;
   ret = s7_read_float(fd, address, &f_val);
   printf("Read\t %s \tfloat:\t %f\n", address, f_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   double w_d_val = 12345.6789;
   ret = s7_write_double(fd, address, w_d_val);
   printf("Write\t %s \tdouble:\t %lf, \tret: %d\n", address, w_d_val, ret);
   GET_RESULT(ret);

   double d_val = 0;
   ret = s7_read_double(fd, address, &d_val);
   printf("Read\t %s \tdouble:\t %lf\n", address, d_val);
   GET_RESULT(ret);

   //////////////////////////////////////////////////////////////////////////
   const char sz_write[] = "wqliceman@gmail.com";
   int length = sizeof(sz_write) / sizeof(sz_write[0]);
   ret = s7_write_string(fd, address, length, sz_write);
   printf("Write\t %s \tstring:\t %s, \tret: %d\n", address, sz_write, ret);
   GET_RESULT(ret);

   char* str_val = NULL;
   ret = s7_read_string(fd, address, length, &str_val);
   printf("Read\t %s \tstring:\t %s\n", address, str_val);
   free(str_val);
   GET_RESULT(ret);

#ifdef _WIN32
   Sleep(TEST_SLEEP_TIME);
#else
   usleep(TEST_SLEEP_TIME* 1000);
#endif
  }

  printf("All Failed count: %d\n", faild_count);

  //mc_remote_run(fd);
  //mc_remote_stop(fd);
  //mc_remote_reset(fd);
  s7_disconnect(fd);

  system("pause");
 }

#ifdef _WIN32
 WSACleanup();
#endif
}
```
