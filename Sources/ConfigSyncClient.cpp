#include "ConfigSyncClient.h"
#include "QtWebSocketClient.h"
#include "SettingsDialog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

const QString ConfigSyncClient::CONFIG_ROOM_ID = "config";

ConfigSyncClient::ConfigSyncClient(const QString &url, const QString &jwtToken, QObject *parent)
    : QObject(parent)
{
    WebSocketConfig config;
    config.jwtToken = jwtToken;
    config.pingIntervalMs = 30000;
    config.reconnectIntervalMs = 5000;
    config.maxReconnectAttempts = 500;
    m_client = new QtWebSocketClient(url, config, this);

    connect(m_client, &QtWebSocketClient::connected, this, &ConfigSyncClient::onConnected);
    connect(m_client, &QtWebSocketClient::disconnected, this, &ConfigSyncClient::onDisconnected);
    connect(m_client, &QtWebSocketClient::messageReceived, this, &ConfigSyncClient::onMessageReceived);
    connect(m_client, &QtWebSocketClient::errorOccurred, this, &ConfigSyncClient::onErrorOccurred);
}

ConfigSyncClient::~ConfigSyncClient() {
    disconnectFromServer();
}

void ConfigSyncClient::connectToServer() {
    m_client->connectToServer();
}

void ConfigSyncClient::disconnectFromServer() {
    m_client->disconnectFromServer();
}

bool ConfigSyncClient::isConnected() const {
    return m_client->isConnected();
}

void ConfigSyncClient::requestSync() {
    QJsonObject msg;
    msg["type"] = "config_sync";
    m_client->sendRawText(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void ConfigSyncClient::pushUpdate(const QString &key, const QString &value) {
    QJsonObject msg;
    msg["type"] = "config_update";
    msg["key"] = key;
    msg["value"] = value;
    m_client->sendRawText(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void ConfigSyncClient::requestRoomList() {
    QJsonObject msg;
    msg["type"] = "room_list";
    msg["roomId"] = CONFIG_ROOM_ID;
    m_client->sendRawText(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void ConfigSyncClient::onConnected() {
    // 连接成功后立即发射连接信号，更新状态
    emit connected();

    // 然后加入 config_room
    QJsonObject joinMsg;
    joinMsg["type"] = "join";
    joinMsg["roomId"] = CONFIG_ROOM_ID;
    m_client->sendRawText(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));
}

void ConfigSyncClient::onDisconnected() {
    emit disconnected();
}

void ConfigSyncClient::onMessageReceived(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "joined") {
        // 加入房间成功后，发送同步和房间列表请求
        requestSync();
        requestRoomList();
    } else if (type == "error") {
        // 服务器返回错误，记录日志
        qDebug() << "ConfigSync error:" << obj["msg"].toString();
    } else if (type == "config_sync") {
        QJsonObject data = obj["data"].toObject();
        QMap<QString, QString> configs;
        for (auto it = data.begin(); it != data.end(); ++it) {
            configs[it.key()] = it.value().toString();
        }
        emit configSynced(configs);
    } else if (type == "config_update") {
        QString key = obj["key"].toString();
        QString value = obj["value"].toString();
        emit configUpdated(key, value);
    } else if (type == "room_list") {
        QJsonArray roomsArray = obj["rooms"].toArray();
        QStringList rooms;
        for (const auto &room : roomsArray) {
            rooms.append(room.toString());
        }
        emit roomListReceived(rooms);
    }
}

void ConfigSyncClient::onErrorOccurred(const QString &error) {
    emit errorOccurred(error);
}
