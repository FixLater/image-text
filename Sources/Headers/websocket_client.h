#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <QObject>
#include <thread>

class WebSocketClient : public QObject {
Q_OBJECT

public:
    explicit WebSocketClient(std::string url);
    ~WebSocketClient() override;

    bool close(bool isFirst = true);
    bool connected();
    void do_read();

signals:
    void messageReceived(const std::string& message);
    void disConnect();

public slots:

    void send(int type, const std::string &message); // 1: Message, 2: Download, 3: Clipboard


private:
    std::string host;
    std::string port;
    boost::asio::io_context ioc;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws;
    boost::beast::multi_buffer buffer;

    void on_read(boost::system::error_code ec, size_t bytes_transferred);
};

#endif // WEBSOCKET_CLIENT_H