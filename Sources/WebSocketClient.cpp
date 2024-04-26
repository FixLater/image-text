
#include <iostream>

#include <iostream>
#include <boost/beast/core/error.hpp>
#include <fstream>
#include "WebSocketClient.h"
#include "MainWindow.h"
#include <cstdint>
#include "UnicodeUtil.cpp"

namespace beast = boost::beast;

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
        std::string text =
                type == 1 ? "Message:" + message : type == 2 ? "Download:" + message : "Clipboard:" + message;
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

void WebSocketClient::sendFile(const std::string &filePath) {
    if (ws.is_open()) {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return;
        }
        int size = 20;
        int chunkSize = 8192;
        int fileLength = static_cast<int>(file.tellg());
        long long totalChunks = (fileLength + chunkSize - 1) / chunkSize;
        file.seekg(0, std::ios::beg);
        std::vector<char> temp_buffer(chunkSize);
        long long chunk_number = 0;
        std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
        fileName = UnicodeUtil::gbk_to_utf8(fileName);
        int big_fileName_length = UnicodeUtil::to_big_endian_int(fileName.size());

        while (!file.eof()) {
            file.read(temp_buffer.data() + size, chunkSize - size);
            std::streamsize bytesRead = file.gcount();
            int64_t big_endian_chunk_number = UnicodeUtil::to_big_endian_long(chunk_number);
            int64_t big_endian_totalChunks = UnicodeUtil::to_big_endian_long(totalChunks);

            std::memcpy(temp_buffer.data(), &big_endian_chunk_number, 8);
            std::memcpy(temp_buffer.data() + 8, &big_endian_totalChunks, 8);
            std::memcpy(temp_buffer.data() + 16, &big_fileName_length, 4);  // Copy the file name length
            // 创建一个临时vector存储要插入的数据
            std::vector<char> insertData(fileName.begin(), fileName.end());
            // 在指定位置插入数据
            temp_buffer.insert(temp_buffer.begin() + 20, insertData.begin(), insertData.end());

            ws.binary(true);
            ws.write(boost::asio::buffer(temp_buffer.data(), bytesRead + size + fileName.size()));
            chunk_number++;
        }
    } else {
        emit disConnect();
    }
}
