#include <gtest/gtest.h>
#include <tcpsocket.h>

TEST(TcpSocketTests, connectTest)
{
    bool connected = false;
    bool disconnected = false;
    bool error = false;

    TcpSocket socket;

    socket.onConnected([&]
    {
        connected = true;
    });

    socket.onDisconnected([&]
    {
        disconnected = true;
    });

    socket.onErrorOccurred([&] (TcpSocket::SocketError)
    {
        error = true;
    });

    socket.connectToHost("google.com", 80);
    ASSERT_EQ(socket.waitForConnected(), true);
    ASSERT_EQ(connected, true);
    ASSERT_EQ(disconnected, false);

    socket.disconnectFromHost();
    ASSERT_EQ(socket.waitForDisconnected(), true);
    ASSERT_EQ(disconnected, true);
}

TEST(TcpSocketTests, invalidConnectionTest)
{
    bool connected = false;
    bool disconnected = false;
    bool error = false;

    TcpSocket socket;

    socket.onConnected([&]
    {
        connected = true;
    });

    socket.onDisconnected([&]
    {
        disconnected = true;
    });

    socket.onErrorOccurred([&] (TcpSocket::SocketError)
    {
        error = true;
    });

    socket.connectToHost("invalid.invalid", 80);
    ASSERT_EQ(socket.waitForConnected(500), false);
    ASSERT_EQ(connected, false);
    ASSERT_EQ(disconnected, false);

    socket.disconnectFromHost();
    ASSERT_EQ(socket.waitForDisconnected(), true);
    ASSERT_EQ(disconnected, false);
}
