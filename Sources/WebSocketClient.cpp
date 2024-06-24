
#include <iostream>

#include <iostream>
#include <boost/beast/core/error.hpp>
#include "WebSocketClient.h"
#include "MainWindow.h"
#include <cstdint>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <codecvt>
#include <boost/filesystem/operations.hpp>
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
    std::thread([this] {
        ioc.run();
    }).detach();
}

void WebSocketClient::on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::asio::error::eof) {
            emit messageReceived("连接已关闭");
        }
        return;
    }
    std::string message = boost::beast::buffers_to_string(buffer.data());
    std::istringstream iss(message);
    std::vector<std::string> result;
    for(std::string s; std::getline(iss, s, ':'); ) {
        result.push_back(s);
    }
    if (result[0] == "Clipboard") {
        emit messageReceived("粘贴板：" + result[1]);
        return;
    }
    emit messageReceived("服务端：" + message);
    buffer.consume(bytes_transferred);
    do_read();
}

void WebSocketClient::sendFile(const std::string &filePath) {
    if (ws.is_open()) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cvt_utf;
        std::wstring wstr = cvt_utf.from_bytes(filePath);
        boost::filesystem::path path(wstr);
        if (!boost::filesystem::exists(path)) {
            emit messageReceived("服务端：" + filePath + "文件不存在");
            return;
        }
        int chunkSize = 8192;
        int fileLength = static_cast<int>(boost::filesystem::file_size(path));
        long long totalChunks = (fileLength + chunkSize - 1) / chunkSize;
        boost::filesystem::ifstream file(wstr, std::ios::binary | std::ios::ate);
        file.seekg(0, std::ios::beg);
        std::vector<char> temp_buffer(chunkSize);
        long long chunk_number = 0;
        std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
        int big_fileName_length = UnicodeUtil::to_big_endian_int(fileName.size());

        while (!file.eof()) {
            file.read(temp_buffer.data(), chunkSize);
            std::streamsize bytesRead = file.gcount();
            int64_t big_endian_chunk_number = UnicodeUtil::to_big_endian_long(chunk_number);
            int64_t big_endian_totalChunks = UnicodeUtil::to_big_endian_long(totalChunks);
            std::vector<char> dataToInsert;
            dataToInsert.insert(dataToInsert.end(), reinterpret_cast<char*>(&big_endian_chunk_number), reinterpret_cast<char*>(&big_endian_chunk_number) + sizeof(big_endian_chunk_number));
            dataToInsert.insert(dataToInsert.end(), reinterpret_cast<char*>(&big_endian_totalChunks), reinterpret_cast<char*>(&big_endian_totalChunks) + sizeof(big_endian_totalChunks));
            dataToInsert.insert(dataToInsert.end(), reinterpret_cast<char*>(&big_fileName_length), reinterpret_cast<char*>(&big_fileName_length) + sizeof(big_fileName_length));
            dataToInsert.insert(dataToInsert.end(), fileName.begin(), fileName.end());
            temp_buffer.insert(temp_buffer.begin(), dataToInsert.begin(), dataToInsert.end());

            ws.binary(true);
            ws.write(boost::asio::buffer(temp_buffer.data(), bytesRead + sizeof(big_endian_chunk_number) + sizeof(big_endian_totalChunks) + sizeof(big_fileName_length) + fileName.size()));
            chunk_number++;
        }
    } else {
        emit disConnect();
    }
}
