#include "QtWebSocketClient.h"
#include <QRandomGenerator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

QtWebSocketClient::QtWebSocketClient(const QString &url, const WebSocketConfig &config, QObject *parent)
    : QObject(parent)
    , m_isWebSocket(true)
    , m_socket(new QTcpSocket(this))
    , m_handshakeComplete(false)
    , m_manualDisconnect(false)
    , m_config(config)
    , m_pingTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
{
    QUrl parsedUrl(url);
    m_host = parsedUrl.host();
    m_port = parsedUrl.port(parsedUrl.scheme() == "wss" ? 443 : 80) == -1
             ? QString::number(parsedUrl.scheme() == "wss" ? 443 : 80)
             : QString::number(parsedUrl.port());
    m_path = parsedUrl.path().isEmpty() ? "/" : parsedUrl.path();
    if (!config.jwtToken.isEmpty()) {
        m_path += "?token=" + config.jwtToken;
    }
    m_isWebSocket = (parsedUrl.scheme() == "ws" || parsedUrl.scheme() == "wss");

    connect(m_socket, &QTcpSocket::connected, this, &QtWebSocketClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &QtWebSocketClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &QtWebSocketClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &QtWebSocketClient::onErrorOccurred);
    connect(m_reconnectTimer, &QTimer::timeout, this, &QtWebSocketClient::attemptReconnect);
    connect(m_pingTimer, &QTimer::timeout, this, &QtWebSocketClient::sendPing);
}

QtWebSocketClient::~QtWebSocketClient() {
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

void QtWebSocketClient::connectToServer() {
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_reconnectAttempts = 0;
        m_manualDisconnect = false;
        m_receiveBuffer.clear();
        m_socket->connectToHost(m_host, m_port.toInt());
    }
}

void QtWebSocketClient::disconnectFromServer() {
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    m_manualDisconnect = true;
    m_handshakeComplete = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool QtWebSocketClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState && m_handshakeComplete;
}

void QtWebSocketClient::onConnected() {
    if (m_isWebSocket) {
        performHandshake();
    } else {
        m_handshakeComplete = true;
        m_reconnectAttempts = 0;
        m_reconnectTimer->stop();
        startPingTimer();
        emit connected();
    }
}

void QtWebSocketClient::performHandshake() {
    QByteArray key = generateWebSocketKey();
    QByteArray request;
    request.append("GET ").append(m_path.toUtf8()).append(" HTTP/1.1\r\n");
    request.append("Host: ").append(m_host.toUtf8()).append(":").append(m_port.toUtf8()).append("\r\n");
    request.append("Upgrade: websocket\r\n");
    request.append("Connection: Upgrade\r\n");
    request.append("Sec-WebSocket-Key: ").append(key).append("\r\n");
    request.append("Sec-WebSocket-Version: 13\r\n");
    request.append("\r\n");

    m_socket->write(request);
}

void QtWebSocketClient::onReadyRead() {
    m_receiveBuffer.append(m_socket->readAll());

    if (!m_handshakeComplete) {
        int headerEnd = m_receiveBuffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return;
        }

        QByteArray response = m_receiveBuffer.left(headerEnd);
        m_receiveBuffer.remove(0, headerEnd + 4);

        if (response.contains(" 101 ") || response.startsWith("HTTP/1.1 101")) {
            m_handshakeComplete = true;
            m_reconnectAttempts = 0;
            m_reconnectTimer->stop();
            startPingTimer();
            emit connected();
        } else {
            QString reason = QString::fromUtf8(response.left(200));
            emit errorOccurred("Handshake failed: " + reason);
            m_socket->disconnectFromHost();
        }
    }

    processBuffer();
}

bool QtWebSocketClient::processBuffer() {
    while (!m_receiveBuffer.isEmpty()) {
        if (m_receiveBuffer.size() < 2) return false;

        auto firstByte = static_cast<quint8>(m_receiveBuffer[0]);
        auto secondByte = static_cast<quint8>(m_receiveBuffer[1]);

        bool isMasked = (secondByte & 0x80) != 0;
        quint64 payloadLength = secondByte & 0x7F;
        int headerSize = 2;

        if (payloadLength == 126) {
            if (m_receiveBuffer.size() < 4) return false;
            payloadLength = (static_cast<quint8>(m_receiveBuffer[2]) << 8) | static_cast<quint8>(m_receiveBuffer[3]);
            headerSize = 4;
        } else if (payloadLength == 127) {
            if (m_receiveBuffer.size() < 10) return false;
            payloadLength = 0;
            for (int i = 0; i < 8; i++) {
                payloadLength = (payloadLength << 8) | static_cast<quint8>(m_receiveBuffer[2 + i]);
            }
            headerSize = 10;
        }

        int maskSize = isMasked ? 4 : 0;
        int totalFrameSize = headerSize + maskSize + static_cast<int>(payloadLength);

        if (m_receiveBuffer.size() < totalFrameSize) return false;

        QByteArray frame = m_receiveBuffer.left(totalFrameSize);
        m_receiveBuffer.remove(0, totalFrameSize);

        handleWebSocketFrame(frame);
    }
    return true;
}

bool QtWebSocketClient::handleWebSocketFrame(const QByteArray &frame) {
    if (frame.size() < 2) return false;

    quint8 firstByte = static_cast<quint8>(frame[0]);
    quint8 secondByte = static_cast<quint8>(frame[1]);

    bool isMasked = (secondByte & 0x80) != 0;
    quint64 payloadLength = secondByte & 0x7F;
    int offset = 2;

    if (payloadLength == 126) {
        payloadLength = (static_cast<quint8>(frame[2]) << 8) | static_cast<quint8>(frame[3]);
        offset = 4;
    } else if (payloadLength == 127) {
        payloadLength = 0;
        for (int i = 0; i < 8; i++) {
            payloadLength = (payloadLength << 8) | static_cast<quint8>(frame[2 + i]);
        }
        offset = 10;
    }

    if (isMasked) {
        offset += 4;
    }

    QByteArray payload = frame.mid(offset, static_cast<int>(payloadLength));

    quint8 opcode = firstByte & 0x0F;

    switch (opcode) {
    case 0x01:
        emit messageReceived(QString::fromUtf8(payload));
        break;
    case 0x02:
        emit binaryMessageReceived(payload);
        break;
    case 0x08:
        m_socket->close();
        break;
    case 0x09: {
        QByteArray pong;
        pong.append(static_cast<char>(0x8A));
        if (payload.size() < 126) {
            pong.append(static_cast<char>(payload.size()));
        }
        pong.append(payload);
        m_socket->write(pong);
        break;
    }
    case 0x0A:
        break;
    default:
        break;
    }
    return true;
}

QByteArray QtWebSocketClient::encodeFrame(const QByteArray &data, bool isBinary) {
    QByteArray frame;
    quint8 opcode = isBinary ? 0x02 : 0x01;
    frame.append(static_cast<char>(0x80 | opcode));

    quint8 maskBit = 0x80;
    if (data.size() < 126) {
        frame.append(static_cast<char>(maskBit | data.size()));
    } else if (data.size() < 65536) {
        frame.append(static_cast<char>(maskBit | 126));
        frame.append(static_cast<char>((data.size() >> 8) & 0xFF));
        frame.append(static_cast<char>(data.size() & 0xFF));
    } else {
        frame.append(static_cast<char>(maskBit | 127));
        for (int i = 7; i >= 0; i--) {
            frame.append(static_cast<char>((data.size() >> (i * 8)) & 0xFF));
        }
    }

    QByteArray mask(4, '\0');
    for (int i = 0; i < 4; i++) {
        mask[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    frame.append(mask);

    QByteArray maskedData = data;
    for (int i = 0; i < maskedData.size(); i++) {
        maskedData[i] = maskedData[i] ^ mask[i % 4];
    }
    frame.append(maskedData);
    return frame;
}

QByteArray QtWebSocketClient::generateWebSocketKey() {
    QByteArray key;
    for (int i = 0; i < 16; i++) {
        key.append(static_cast<char>(QRandomGenerator::global()->bounded(256)));
    }
    return key.toBase64();
}

QByteArray QtWebSocketClient::readAllFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void QtWebSocketClient::sendImage(const QString &filePath) {
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }

    QFileInfo fileInfo(filePath);
    QByteArray imageData = readAllFile(filePath);
    if (imageData.isEmpty()) {
        emit errorOccurred("Failed to read file: " + filePath);
        return;
    }

    QJsonObject meta;
    meta["type"] = "image_meta";
    meta["fileName"] = fileInfo.fileName();
    meta["fileType"] = fileInfo.suffix();
    meta["fileSize"] = static_cast<qint64>(imageData.size());

    QJsonDocument doc(meta);
    sendRawText(doc.toJson(QJsonDocument::Compact));

    sendBinaryMessage(imageData);
}

void QtWebSocketClient::onDisconnected() {
    m_pingTimer->stop();
    m_handshakeComplete = false;
    m_receiveBuffer.clear();
    emit disconnected();

    if (!m_manualDisconnect && m_config.reconnectIntervalMs > 0 && m_config.maxReconnectAttempts > 0) {
        m_reconnectTimer->start(m_config.reconnectIntervalMs);
    }
}

void QtWebSocketClient::onErrorOccurred(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    emit errorOccurred(m_socket->errorString());
}

void QtWebSocketClient::attemptReconnect() {
    if (m_reconnectAttempts >= m_config.maxReconnectAttempts) {
        m_reconnectTimer->stop();
        emit errorOccurred("Max reconnect attempts reached");
        return;
    }

    m_reconnectAttempts++;
    emit reconnecting(m_reconnectAttempts, m_config.maxReconnectAttempts);

    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_receiveBuffer.clear();
        m_socket->connectToHost(m_host, m_port.toInt());
    }
}

void QtWebSocketClient::sendRawText(const QString &message) {
    if (isConnected()) {
        m_socket->write(encodeFrame(message.toUtf8(), false));
    } else {
        emit errorOccurred("Not connected to server");
    }
}

void QtWebSocketClient::sendMessage(const QString &message) {
    QString json = QString(R"({"type":"message","content":"%1"})").arg(message);
    sendRawText(json);
}

void QtWebSocketClient::sendBinaryMessage(const QByteArray &data) {
    if (isConnected()) {
        m_socket->write(encodeFrame(data, true));
    } else {
        emit errorOccurred("Not connected to server");
    }
}

void QtWebSocketClient::startPingTimer() {
    if (m_config.pingIntervalMs > 0) {
        m_pingTimer->start(m_config.pingIntervalMs);
    }
}

void QtWebSocketClient::sendPing() {
    if (!isConnected()) return;

    QByteArray frame;
    frame.append(static_cast<char>(0x89));

    QByteArray mask(4, '\0');
    for (int i = 0; i < 4; i++) {
        mask[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    frame.append(static_cast<char>(0x80));
    frame.append(mask);

    m_socket->write(frame);
}
