#ifndef __SOCKET_H_
#define __SOCKET_H_

#include "utill.h"

int socket_send_data(int fd, void* ptr, int nbytes);
int socket_recv_data(int fd, void* ptr, int nbytes);
int socket_recv_data_one_loop(int fd, void* ptr, int nbytes);
int socket_open_tcp_client_socket(char* ip, short port);
void socket_close_tcp_socket(int sockFd);

#endif //__SOCKET_H_
