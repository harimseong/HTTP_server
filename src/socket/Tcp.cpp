#include <sstream>

#include "socket/Tcp.hpp"

using namespace	std;

string
getFormattedAddress(uint32_t addr, uint16_t port)
{
	stringstream	ss;
	uint32_t		bitshift = 24;
	int64_t			bitmask = 0xff000000;

	for (; bitshift > 0; bitmask >>= 8, bitshift -= 8)
	{
		ss << ((addr & bitmask) >> bitshift) << '.';
	}
	ss << (addr & bitmask);
	if (port != 0)
		ss << ':' << port;
	return ss.str();
}

string
getFormattedAddress(sockaddr_in& addr)
{
	return getFormattedAddress(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
}

string
getFormattedAddress(int fd)
{
	sockaddr_in		addr;
	socklen_t		addrLen = Tcp::socketAddrLen;

	getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen);
	return getFormattedAddress(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
}
