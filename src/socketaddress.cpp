#include "socketaddress.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#endif

const char *SocketAddress::unixDomainPrefix = "localhost:";

SocketAddress::SocketAddress(const std::string &hostName, unsigned short port)
    : mHostName(hostName)
{
    if (isUnix(hostName))
        *this = SocketAddress::afUnix(hostName.substr(strlen(unixDomainPrefix)));
    else
        *this = SocketAddress::afInet(hostName, port);
}

void SocketAddress::unlink()
{
    if (isUnix())
    {
        ::unlink(mHostName.c_str());
    }
}

SocketAddress SocketAddress::afInet(const std::string &hostName, unsigned short port)
{
    struct hostent *hp = gethostbyname(hostName.c_str());
    if (hp == nullptr)
        return SocketAddress();

    if (hp->h_addr_list == nullptr)
        return SocketAddress();

    if (hp->h_addr_list[0] == nullptr)
        return SocketAddress();

    struct sockaddr_in *sa_in = new sockaddr_in;
    (void)memset(sa_in, 0, sizeof(struct sockaddr_in));
    sa_in->sin_family      = AF_INET;
    sa_in->sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sa_in->sin_port        = htons(port);

    SocketAddress result;
    result.mHostName = hostName;
    result.mData.reset(reinterpret_cast<struct sockaddr*>(sa_in));
    result.mSize = sizeof(struct sockaddr_in);
    return result;
}

#ifdef _WIN32
SocketAddress SocketAddress::afUnix(const std::string &)
{
    return SocketAddress();
}
#else
SocketAddress SocketAddress::afUnix(const std::string &unixPath)
{
    struct sockaddr_un *sa_un = new struct sockaddr_un;

    (void)memset(sa_un, 0, sizeof(struct sockaddr_un));
    sa_un->sun_family = AF_UNIX;

    std::string path = unixPath;

    // Using abstract socket path to avoid filesystem boilerplate
#ifndef __linux__
    path.insert(0, std::string("\0", 1));
#endif

    strncpy(sa_un->sun_path, path.c_str(), sizeof(sa_un->sun_path) - 1);

    SocketAddress result;
    result.mHostName = unixPath;
    result.mData.reset(reinterpret_cast<struct sockaddr *>(sa_un));
    result.mSize = offsetof(struct sockaddr_un, sun_path) + path.size();
    return result;
}
#endif

bool SocketAddress::isUnix(const std::string &hostName)
{
    return hostName.rfind(unixDomainPrefix, 0) == 0;
}

bool SocketAddress::isUnix() const
{
    return isValid() ? mData->sa_family == AF_UNIX : false;
}
