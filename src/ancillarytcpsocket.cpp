/*
    Copyright (C) 2022 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ancillarytcpsocket.h"

#include <cstring>

#include <sys/socket.h>

int AncillaryTcpSocket::sendWithFds(const std::string &data, const std::vector<int> &fds)
{
    const int fdsSpace = fds.size() * sizeof(int);

    std::vector<char> control(CMSG_SPACE(fdsSpace));

    struct iovec io;
    io.iov_base = const_cast<char *>(data.c_str());
    io.iov_len = data.size();

    struct msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = control.data();
    msg.msg_controllen = control.size();

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(fdsSpace);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    memcpy(CMSG_DATA(cmsg), fds.data(), fdsSpace);

    return sendmsg(socketDescriptor(), &msg, MSG_NOSIGNAL);
}

int AncillaryTcpSocket::readWithFds(void *data, size_t size, std::vector<int> &fds, size_t maxFds)
{
    const int fdsSpace = maxFds * sizeof(int);

    std::vector<char> control(CMSG_SPACE(fdsSpace));

    struct iovec io;
    io.iov_base = data;
    io.iov_len = size;

    struct msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = control.data();
    msg.msg_controllen = control.size();

    int recvflag;
#ifdef __linux__
    recvflag = MSG_CMSG_CLOEXEC;
#else
    recvflag = 0;
#endif

    int ret = recvmsg(socketDescriptor(), &msg, recvflag);

    for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level != SOL_SOCKET && cmsg->cmsg_type != SCM_RIGHTS)
        {
            //log(fmt("Ignoring ancillary data level %d, type %d\n", cmsg->cmsg_level, cmsg->cmsg_type));
            continue;
        }

        int *cmsgData = (int*)CMSG_DATA(cmsg);
        int  cmsgDataCount = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        //log(fmt("Received %d fds\n", cmsgDataCount));
        for(int i = 0; i < cmsgDataCount; ++i)
        {
#ifndef __linux__
            fcntl(cmsgData[i], F_SETFD, FD_CLOEXEC);
#endif
            fds.push_back(cmsgData[i]);
        }

    }
    return ret;
}

void AncillaryTcpSocket::readyRead()
{
    char buffer[65536];

    int n = readWithFds(buffer, sizeof(buffer), mFds, mMaxFds);

    if (n <= 0)
    {
        disconnectFromHost();
        return;
    }

    emitData(buffer, n);
}
