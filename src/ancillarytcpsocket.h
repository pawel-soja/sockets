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
#pragma once

#include "tcpsocket.h"

#include <string>
#include <vector>

class AncillaryTcpSocket: public TcpSocket
{
    public:
        int sendWithFds(const std::string &data, const std::vector<int> &fds);
        int readWithFds(void *data, size_t size, std::vector<int> &fds, size_t maxFds);

    public:
        std::vector<int> &fds()
        {
            return mFds;
        }
        void setMaxFds(size_t maxFds)
        {
            mMaxFds = maxFds;
        }
        size_t maxFds() const
        {
            return mMaxFds;
        }

    protected:
        void readyRead();

    protected:
        std::vector<int> mFds;
        size_t mMaxFds = 16;
};
