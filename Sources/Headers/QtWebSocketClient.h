#ifndef QT_WEBSOCKET_CLIENT_H
#define QT_WEBSOCKET_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QUrl>
#include <QString>
#include <QByteArray>
#include <QTimer>

struct WebSocketConfig {
    QString jwtToken;
    int pingIntervalMs = 30000;
    int reconnectIntervalMs = 5000;
    int maxReconnectAttempts = 500;
};

class QtWebSocketClient : public QObject {
    Q_OBJECT

public:
    explicit QtWebSocketClient(const QString &url, const WebSocketConfig &config = {}, QObject *parent = nullptr);
    ~QtWebSocketClient() override;

    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);
    void binaryMessageReceived(const QByteArray &message);
    void errorOccurred(const QString &errorString);
    void reconnecting(int attempt, int maxAttempts);

public slots:
    void sendMessage(const QString &message);
    void sendBinaryMessage(const QByteArray &data);
    void sendImage(const QString &filePath);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void attemptReconnect();
    void sendPing();

private:
    void sendRawText(const QString &message);
    void performHandshake();
    bool processBuffer();
    bool handleWebSocketFrame(const QByteArray &frame);
    QByteArray encodeFrame(const QByteArray &data, bool isBinary = false);
    QByteArray generateWebSocketKey();
    QByteArray readAllFile(const QString &filePath);
    void startPingTimer();

    QString m_host;
    QString m_port;
    QString m_path;
    bool m_isWebSocket;
    QTcpSocket *m_socket;
    bool m_handshakeComplete;
    bool m_manualDisconnect;
    WebSocketConfig m_config;
    QTimer *m_pingTimer;
    QTimer *m_reconnectTimer;
    int m_reconnectAttempts;
    QByteArray m_receiveBuffer;
};

#endif // QT_WEBSOCKET_CLIENT_H
