# 程序整体介绍

- 项目名称：siemens_plc_s7_net
- 开发语言：C语言
- 支持操作系统：windows/linux
- 测试设备：S1200

目前实现功能，实现西门子PLC通讯类，采用S7协议实现，需要在PLC侧先的以太网模块先进行配置。

## 头文件

```c
#include "siemens_s7.h"  //协议提供方法接口
#include "typedef.h"   //部分类型宏定义
```

## 西门子PLC地址说明

### 连接属性

- port 端口号，通常为102
- plc_type plc型号，S200、S200Smart、S300、S400、S1200、S1500

### PLC地址分类

类型的代号值（软元件代码，用于区分软元件类型，如：D，R）

| 序号  | 描述           | 地址类型 |
| :---: | :------------- | -------- |
|   1   | 中间继电器     | M        |
|   2   | 输入继电器     | I        |
|   3   | 输出继电器Q    | Q        |
|   4   | DB块寄存器DB   | DB       |
|   5   | V寄存器        | V        |
|   6   | 定时器的值     | T        |
|   7   | 计数器的值     | C        |
|   8   | 智能输入寄存器 | AI       |
|   9   | 智能输出寄存器 | AQ       |

## 实现方法

### 1.连接PLC设备

```c
bool s7_connect(char* ip_addr, int port, siemens_plc_types_e plc, int* fd);
bool s7_disconnect(int fd);

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
```

### 2.读取数据

```c
s7_error_code_e s7_read_bool(int fd, const char* address, bool* val);
s7_error_code_e s7_read_byte(int fd, const char* address, byte* val);
s7_error_code_e s7_read_short(int fd, const char* address, short* val);
s7_error_code_e s7_read_ushort(int fd, const char* address, ushort* val);
s7_error_code_e s7_read_int32(int fd, const char* address, int32* val);
s7_error_code_e s7_read_uint32(int fd, const char* address, uint32* val);
s7_error_code_e s7_read_int64(int fd, const char* address, int64* val);
s7_error_code_e s7_read_uint64(int fd, const char* address, uint64* val);
s7_error_code_e s7_read_float(int fd, const char* address, float* val);
s7_error_code_e s7_read_double(int fd, const char* address, double* val);
s7_error_code_e s7_read_string(int fd, const char* address, int length, char** val); //need free val
```

### 3.写入数据

```c
s7_error_code_e s7_write_bool(int fd, const char* address, bool val);
s7_error_code_e s7_write_byte(int fd, const char* address, byte val);
s7_error_code_e s7_write_short(int fd, const char* address, short val);
s7_error_code_e s7_write_ushort(int fd, const char* address, ushort val);
s7_error_code_e s7_write_int32(int fd, const char* address, int32 val);
s7_error_code_e s7_write_uint32(int fd, const char* address, uint32 val);
s7_error_code_e s7_write_int64(int fd, const char* address, int64 val);
s7_error_code_e s7_write_uint64(int fd, const char* address, uint64 val);
s7_error_code_e s7_write_float(int fd, const char* address, float val);
s7_error_code_e s7_write_double(int fd, const char* address, double val);
s7_error_code_e s7_write_string(int fd, const char* address, int length, const char* val);
```

## 使用样例

完整样例参见代码中**main.c**文件，如下提供主要代码和使用方法：

*读取地址，格式为"**M100**","**DB100**"

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
