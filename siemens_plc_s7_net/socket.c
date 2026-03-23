/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2022-2026 wqliceman
 * GitHub: iceman
 * Email: wqliceman@gmail.com
 */

#include "socket.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib") /* Linking with winsock library */
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

static int socket_is_interrupted_error(void) {
#ifdef _WIN32
	return WSAGetLastError() == WSAEINTR;
#else
	return errno == EINTR;
#endif
}

static int socket_set_nonblocking(int fd, int nonblocking) {
#ifdef _WIN32
	u_long mode = nonblocking ? 1UL : 0UL;
	return ioctlsocket(fd, FIONBIO, &mode);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		return -1;
	}
	if (nonblocking) {
		flags |= O_NONBLOCK;
	}
	else {
		flags &= ~O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flags);
#endif
}

static int socket_connect_with_timeout_impl(int sockFd, struct sockaddr_in* serverAddr, int timeout_ms) {
	if (timeout_ms <= 0) {
		timeout_ms = 5000;
	}

	if (socket_set_nonblocking(sockFd, 1) != 0) {
		return -1;
	}

	int ret = connect(sockFd, (struct sockaddr*)serverAddr, sizeof(*serverAddr));
	if (ret == 0) {
		socket_set_nonblocking(sockFd, 0);
		return 0;
	}

#ifdef _WIN32
	int connect_err = WSAGetLastError();
	if (connect_err != WSAEWOULDBLOCK && connect_err != WSAEINPROGRESS && connect_err != WSAEINVAL) {
		socket_set_nonblocking(sockFd, 0);
		return -1;
	}
#else
	if (errno != EINPROGRESS) {
		socket_set_nonblocking(sockFd, 0);
		return -1;
	}
#endif

	fd_set writefds = { 0 };
	FD_ZERO(&writefds);
	FD_SET(sockFd, &writefds);

	struct timeval tv = { 0 };
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;

	ret = select(sockFd + 1, NULL, &writefds, NULL, &tv);
	if (ret <= 0) {
		socket_set_nonblocking(sockFd, 0);
		return -1;
	}

	int so_error = 0;
#ifdef _WIN32
	int opt_len = sizeof(so_error);
#else
	socklen_t opt_len = sizeof(so_error);
#endif
	if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &opt_len) != 0 || so_error != 0) {
		socket_set_nonblocking(sockFd, 0);
		return -1;
	}

	socket_set_nonblocking(sockFd, 0);
	return 0;
}

int socket_send_data(int fd, void* buf, int nbytes) {
	int   nleft, nwritten;
	char* ptr = (char*)buf;

	if (fd < 0) return -1;

	nleft = nbytes;
	while (nleft > 0) {
		nwritten = send(fd, ptr, nleft, 0);
		if (nwritten <= 0) {
			if (socket_is_interrupted_error())
				continue;
			else
				return -1;
		}
		else {
				// Currently this function performs only one receive loop by design.
			ptr += nwritten;
		}
	}

	return (nbytes - nleft);
}

int socket_recv_data(int fd, void* buf, int nbytes) {
	int   nleft, nread;
	char* ptr = (char*)buf;

	if (fd < 0) return -1;

	nleft = nbytes;
	while (nleft > 0) {
		nread = recv(fd, ptr, nleft, 0);
		if (nread == 0) {
			break;
		}
		else if (nread < 0) {
			if (socket_is_interrupted_error())
				continue;
			else
				return -1;
		}
		else {
			nleft -= nread;
			ptr += nread;
		}
	}

	return (nbytes - nleft);
}

int socket_recv_data_one_loop(int fd, void* buf, int nbytes) {
	int   nleft, nread;
	char* ptr = (char*)buf;

	if (fd < 0) return -1;

	nleft = nbytes;
	while (nleft > 0) {
		nread = recv(fd, ptr, nleft, 0);
		if (nread == 0) {
			break;
		}
		else if (nread < 0) {
			if (socket_is_interrupted_error())
				continue;
			else
				return -1;
		}
		else {
			nleft -= nread;
			ptr += nread;

			// Currently this function performs only one receive loop by design.
			break;
		}
	}

	return (nbytes - nleft);
}

int socket_open_tcp_client_socket_with_timeout(char* destIp, short destPort, int timeout_ms) {
	int                sockFd = 0;
	struct sockaddr_in serverAddr = {0};
	int                ret;

	sockFd = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sockFd < 0) {
		return -1;
#pragma warning(disable:4996)
	}

	memset((char*)&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(destIp);
	serverAddr.sin_port = (uint16_t)htons((uint16_t)destPort);

	ret = socket_connect_with_timeout_impl(sockFd, &serverAddr, timeout_ms);
	if (ret != 0) {
		socket_close_tcp_socket(sockFd);
		return -1;
	}

#ifdef _WIN32
	int timeout = timeout_ms > 0 ? timeout_ms : 5000;
	ret = setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
	ret = setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
	struct timeval timeout = { (timeout_ms > 0 ? timeout_ms : 5000) / 1000, ((timeout_ms > 0 ? timeout_ms : 5000) % 1000) * 1000 };
	ret = setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
	ret = setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

	return sockFd;
}

int socket_open_tcp_client_socket(char* destIp, short destPort) {
	return socket_open_tcp_client_socket_with_timeout(destIp, destPort, 5000);
}

void socket_close_tcp_socket(int sockFd) {
	if (sockFd >= 0)
	{
#ifdef _WIN32
		closesocket(sockFd);
#else
		close(sockFd);
#endif
	}
}

static void tinet_ntoa(char* ipstr, unsigned int ip) {
	sprintf(ipstr, "%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
}
