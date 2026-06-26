#ifndef CONFIGSYNCCLIENT_H
#define CONFIGSYNCCLIENT_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

class QtWebSocketClient;

class ConfigSyncClient : public QObject {
    Q_OBJECT

public:
    explicit ConfigSyncClient(const QString &url, const QString &jwtToken, QObject *parent = nullptr);
    ~ConfigSyncClient() override;

    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    void requestSync();
    void pushUpdate(const QString &key, const QString &value);
    void requestRoomList();

signals:
    void connected();
    void disconnected();
    void configSynced(const QMap<QString, QString> &configs);
    void configUpdated(const QString &key, const QString &value);
    void roomListReceived(const QStringList &rooms);
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QString &message);
    void onErrorOccurred(const QString &error);

private:
    QtWebSocketClient *m_client;
    static const QString CONFIG_ROOM_ID;
};

#endif // CONFIGSYNCCLIENT_H
