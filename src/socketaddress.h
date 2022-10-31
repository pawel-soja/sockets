#pragma once

#include <string>
#include <memory>

class SocketAddress
{
    public:
        static const char *unixDomainPrefix;

    public:
        SocketAddress() = default;
        explicit SocketAddress(const std::string &hostName, unsigned short port);

    public:
        static bool isUnix(const std::string &hostName);
        bool isUnix() const;

    public:
        void unlink();

    public:
        bool isValid() const
        {
            return data() != nullptr;
        }

        const struct sockaddr *data() const
        {
            return mData.get();
        }

        size_t size() const
        {
            return mSize;
        }

    public:
        const struct sockaddr *operator&() const
        {
            return data();
        }

    protected:
        static SocketAddress afInet(const std::string &hostName, unsigned short port);
        static SocketAddress afUnix(const std::string &hostName);

    protected:
        std::string mHostName;
        std::unique_ptr<struct sockaddr> mData;
        size_t mSize;
};
