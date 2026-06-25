#include "ConfigSyncClient.h"
#include "QtWebSocketClient.h"
#include "SettingsDialog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

const QString ConfigSyncClient::CONFIG_ROOM_ID = "config_room";

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

void ConfigSyncClient::onConnected() {
    QJsonObject joinMsg;
    joinMsg["type"] = "join";
    joinMsg["roomId"] = CONFIG_ROOM_ID;
    m_client->sendRawText(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));

    requestSync();
    emit connected();
}

void ConfigSyncClient::onDisconnected() {
    emit disconnected();
}

void ConfigSyncClient::onMessageReceived(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "config_sync") {
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
    }
}

void ConfigSyncClient::onErrorOccurred(const QString &error) {
    emit errorOccurred(error);
}
