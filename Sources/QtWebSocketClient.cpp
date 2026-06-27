#include "QtWebSocketClient.h"
#include "SettingsDialog.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>

static void wsLog(const QString &msg) {
    QFile f(QCoreApplication::applicationDirPath() + "/ws_debug.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toUtf8() + " " + msg.toUtf8() + "\n");
    }
}
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
    , m_connectTimeoutTimer(new QTimer(this))
    , m_reconnectCountdownTimer(new QTimer(this))
    , m_reconnectAttempts(0)
    , m_reconnectCountdownSeconds(0)
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
    m_connectTimeoutTimer->setSingleShot(true);
    connect(m_connectTimeoutTimer, &QTimer::timeout, this, [this]() {
        wsLog(QString("[WS] TIMEOUT fired, state:%1 handshake:%2").arg(m_socket->state()).arg(m_handshakeComplete));
        if (m_socket->state() == QAbstractSocket::ConnectingState ||
            (m_socket->state() == QAbstractSocket::ConnectedState && !m_handshakeComplete)) {
            wsLog("[WS] TIMEOUT -> aborting");
            m_socket->abort();
        }
    });
    connect(m_reconnectCountdownTimer, &QTimer::timeout, this, [this]() {
        if (m_reconnectCountdownSeconds > 0) {
            m_reconnectCountdownSeconds--;
            wsLog(QString("[WS] countdown:%1 attempts:%2").arg(m_reconnectCountdownSeconds).arg(m_reconnectAttempts));
            emit reconnectCountdown(m_reconnectCountdownSeconds, m_reconnectAttempts, m_config.maxReconnectAttempts);
        }
    });
}

QtWebSocketClient::~QtWebSocketClient() {
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    m_connectTimeoutTimer->stop();
    m_reconnectCountdownTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

void QtWebSocketClient::connectToServer() {
    wsLog(QString("[WS] connectToServer, state:%1 host:%2 port:%3").arg(m_socket->state()).arg(m_host).arg(m_port));
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_reconnectAttempts = 0;
        m_manualDisconnect = false;
        m_receiveBuffer.clear();
        m_socket->connectToHost(m_host, m_port.toInt());
        m_connectTimeoutTimer->start(5000);
        wsLog("[WS] connectToHost called, timeout 5s started");
    }
}

void QtWebSocketClient::disconnectFromServer() {
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    m_connectTimeoutTimer->stop();
    m_reconnectCountdownTimer->stop();
    m_manualDisconnect = true;

    if (isConnected()) {
        QJsonObject leaveMsg;
        leaveMsg["type"] = "leave";
        sendRawText(QJsonDocument(leaveMsg).toJson(QJsonDocument::Compact));
    }

    m_handshakeComplete = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool QtWebSocketClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState && m_handshakeComplete;
}

void QtWebSocketClient::onConnected() {
    wsLog(QString("[WS] onConnected(TCP), state:%1").arg(m_socket->state()));
    m_reconnectCountdownTimer->stop();
    if (m_isWebSocket) {
        performHandshake();
        // 重启超时计时器，覆盖 WebSocket 握手阶段
        m_connectTimeoutTimer->start(5000);
    } else {
        m_connectTimeoutTimer->stop();
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
            wsLog("[WS] Handshake OK (101)");
            m_connectTimeoutTimer->stop();
            m_handshakeComplete = true;
            m_reconnectAttempts = 0;
            m_reconnectTimer->stop();
            startPingTimer();
            emit connected();
        } else {
            QString reason = QString::fromUtf8(response.left(200));
            wsLog("[WS] Handshake FAILED: " + reason);
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
    sendFileChunked(filePath);
}

void QtWebSocketClient::sendFileChunked(const QString &filePath) {
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }

    QFileInfo fileInfo(filePath);
    m_pendingChunkData = readAllFile(filePath);
    if (m_pendingChunkData.isEmpty()) {
        emit errorOccurred("Failed to read file: " + filePath);
        return;
    }

    m_pendingFileName = fileInfo.fileName();
    m_pendingFileType = fileInfo.suffix();
    m_totalChunks = (m_pendingChunkData.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
    m_sentChunks = 0;

    QJsonObject meta;
    meta["type"] = "image_meta";
    meta["fileName"] = m_pendingFileName;
    meta["fileType"] = m_pendingFileType;
    meta["fileSize"] = static_cast<qint64>(m_pendingChunkData.size());
    meta["totalSize"] = static_cast<qint64>(m_pendingChunkData.size());
    meta["totalChunks"] = m_totalChunks;

    sendRawText(QJsonDocument(meta).toJson(QJsonDocument::Compact));
}

void QtWebSocketClient::handleChunkAck() {
    if (m_sentChunks >= m_totalChunks) {
        QJsonObject complete;
        complete["type"] = "chunk_complete";
        sendRawText(QJsonDocument(complete).toJson(QJsonDocument::Compact));
        m_pendingChunkData.clear();
        emit chunkComplete();
        return;
    }

    int offset = m_sentChunks * CHUNK_SIZE;
    int size = qMin(CHUNK_SIZE, m_pendingChunkData.size() - offset);
    QByteArray chunk = m_pendingChunkData.mid(offset, size);
    sendBinaryMessage(chunk);
    m_sentChunks++;
    emit chunkProgress(m_sentChunks, m_totalChunks);
}

void QtWebSocketClient::onDisconnected() {
    wsLog(QString("[WS] onDisconnected, manual:%1 attempts:%2").arg(m_manualDisconnect).arg(m_reconnectAttempts));
    m_pingTimer->stop();
    m_connectTimeoutTimer->stop();
    m_handshakeComplete = false;
    m_receiveBuffer.clear();
    emit disconnected();

    if (!m_manualDisconnect && m_config.reconnectIntervalMs > 0 && m_config.maxReconnectAttempts > 0) {
        m_reconnectCountdownSeconds = m_config.reconnectIntervalMs / 1000;
        emit reconnectCountdown(m_reconnectCountdownSeconds, m_reconnectAttempts, m_config.maxReconnectAttempts);
        m_reconnectCountdownTimer->start(1000);
        m_reconnectTimer->start(m_config.reconnectIntervalMs);
    }
}

void QtWebSocketClient::onErrorOccurred(QAbstractSocket::SocketError error) {
    wsLog(QString("[WS] onErrorOccurred:%1 %2 state:%3").arg(error).arg(m_socket->errorString()).arg(m_socket->state()));
    emit errorOccurred(m_socket->errorString());

    // 连接被拒绝等场景，socket 直接变成 UnconnectedState，不会触发 onDisconnected
    // 所以在这里也启动重连逻辑
    if (m_socket->state() == QAbstractSocket::UnconnectedState && !m_manualDisconnect
        && m_config.reconnectIntervalMs > 0 && m_config.maxReconnectAttempts > 0) {
        m_connectTimeoutTimer->stop();
        m_reconnectCountdownSeconds = m_config.reconnectIntervalMs / 1000;
        emit reconnectCountdown(m_reconnectCountdownSeconds, m_reconnectAttempts, m_config.maxReconnectAttempts);
        m_reconnectCountdownTimer->start(1000);
        m_reconnectTimer->start(m_config.reconnectIntervalMs);
        wsLog(QString("[WS] onErrorOccurred -> starting reconnect countdown, %1s").arg(m_reconnectCountdownSeconds));
    }
}

void QtWebSocketClient::attemptReconnect() {
    wsLog(QString("[WS] attemptReconnect, attempts:%1 max:%2").arg(m_reconnectAttempts).arg(m_config.maxReconnectAttempts));
    m_reconnectCountdownTimer->stop();
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
        m_connectTimeoutTimer->start(5000);
        wsLog("[WS] reconnect connectToHost called, timeout 5s started");
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
