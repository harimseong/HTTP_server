#ifndef SOCKET_TCP_HPP
#define SOCKET_TCP_HPP

#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <cstring>
#include <string>

#include "OsDependency.hpp"
#include "socket/Socket.hpp"

struct	Tcp
{
	struct	SocketAddr: public sockaddr_in
	{
		SocketAddr()
		{
			sockaddr_in::operator=(GET_SOCKADDR_IN(0, 0));
			::memset(sin_zero, 0, sizeof(sin_zero));
		};

		SocketAddr(const sockaddr_in& addr)
		{
			sockaddr_in::operator=(addr);
			::memset(sin_zero, 0, sizeof(sin_zero));
		};
	};

	static int	initSocket(int fd)
	{
		int socketOption = 1;

		return ::setsockopt(fd, SOL_SOCKET, SOCKET_CLOEXEC | SO_REUSEADDR, &socketOption, sizeof(socketOption));
	}

	static int	removeSocket(int fd, const sockaddr_in& sockAddr)
	{
		(void)sockAddr;
		return ::close(fd);
	}

	static const socklen_t	socketAddrLen = sizeof(sockaddr_in);
	static const int		domain = AF_INET;
	static const int		type = SOCK_STREAM;
	static const int		protocol = 0;

};

std::string	getFormattedAddress(uint32_t addr, uint16_t port);
std::string	getFormattedAddress(sockaddr_in& addr);
std::string	getFormattedAddress(int fd);

#endif
