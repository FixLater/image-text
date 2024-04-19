
#include <iostream>

#include <iostream>
#include "websocket_client.h"
#include "mainwindow.h"

WebSocketClient::WebSocketClient(std::string url)
        : QObject(), ws(ioc) {
    std::string protocol, path;
    auto protocol_end = url.find("://");
    if (protocol_end != std::string::npos) {
        protocol = url.substr(0, protocol_end);
        url.erase(0, protocol_end + 3);
    }

    auto host_end = url.find(':');
    if (host_end != std::string::npos) {
        this->host = url.substr(0, host_end);
        url.erase(0, host_end + 1);
    }

    auto port_end = url.find('/');
    if (port_end != std::string::npos) {
        this->port = url.substr(0, port_end);
        path = url.substr(port_end);
    } else {
        port = url;
    }
}

WebSocketClient::~WebSocketClient() {
    ioc.stop();
}

bool WebSocketClient::connected() {
    // Resolve the host and port to an endpoint
    try {
        auto const results = boost::asio::ip::tcp::resolver(ioc).resolve(host, port);
        // Connect the websocket stream to the endpoint
        boost::asio::connect(ws.next_layer(), results.begin(), results.end());
        // Perform the websocket handshake
        ws.handshake(host, "/");
        return true;
    } catch (const std::exception &e) {
        emit messageReceived("连接错误: " + QString::fromLocal8Bit(e.what()).toStdString());
        return false;
    }
}

void WebSocketClient::send(int type, const std::string &message) {
    if (ws.is_open()) {
        std::string text = type == 1 ? "Message:" + message : type == 2 ? "Download:" + message : "Clipboard:" + message;
        ws.write(boost::asio::buffer(std::string(text)));
        std::thread([this] {
            ioc.run();
        }).detach();
    } else {
        emit disConnect();
    }
}

bool WebSocketClient::close(bool isFirst) {
    try {
        if (ws.is_open()) {
            ws.next_layer().cancel();
            ws.close(boost::beast::websocket::close_code::normal);
            ioc.stop();
        }
        return true;
    } catch (const std::exception &e) {
        if (isFirst) {
            return close(false);
        }
        emit messageReceived("关闭错误: " + std::string(e.what()));
        return false;
    }
}


void WebSocketClient::do_read() {
    ws.async_read(buffer, boost::beast::bind_front_handler(&WebSocketClient::on_read, this));
}

void WebSocketClient::on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::asio::error::eof) {
            emit messageReceived("连接已关闭");
        }
        return;
    }
    std::string message = boost::beast::buffers_to_string(buffer.data());
    emit messageReceived("服务端：" + message);
    buffer.consume(bytes_transferred);
    do_read();
}