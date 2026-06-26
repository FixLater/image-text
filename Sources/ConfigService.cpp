#include "ConfigService.h"
#include "ConfigSyncClient.h"

ConfigService::ConfigService(const QString &baseUrl, QObject *parent)
    : QObject(parent), m_baseUrl(baseUrl) {
}

ConfigService::~ConfigService() {
    stopWebSocketSync();
}

void ConfigService::loadAll() {
    if (m_syncClient && m_syncClient->isConnected()) {
        m_syncClient->requestSync();
        m_syncClient->requestRoomList();
    }
}

void ConfigService::saveAll(const QMap<QString, QString> &configs) {
    if (m_syncClient && m_syncClient->isConnected()) {
        for (auto it = configs.constBegin(); it != configs.constEnd(); ++it) {
            m_syncClient->pushUpdate(it.key(), it.value());
        }
    }
}

bool ConfigService::isServerAvailable() const {
    return m_serverAvailable;
}

void ConfigService::startWebSocketSync(const QString &wsUrl, const QString &jwtToken) {
    if (m_syncClient) {
        stopWebSocketSync();
    }

    m_syncClient = new ConfigSyncClient(wsUrl, jwtToken, this);

    connect(m_syncClient, &ConfigSyncClient::connected, this, &ConfigService::onWebSocketConnected);
    connect(m_syncClient, &ConfigSyncClient::disconnected, this, &ConfigService::onWebSocketDisconnected);
    connect(m_syncClient, &ConfigSyncClient::configSynced, this, &ConfigService::onConfigSynced);
    connect(m_syncClient, &ConfigSyncClient::configUpdated, this, &ConfigService::onConfigUpdated);
    connect(m_syncClient, &ConfigSyncClient::roomListReceived, this, &ConfigService::onRoomListReceived);

    m_syncClient->connectToServer();
}

void ConfigService::stopWebSocketSync() {
    if (m_syncClient) {
        m_syncClient->disconnectFromServer();
        m_syncClient->deleteLater();
        m_syncClient = nullptr;
    }
}

bool ConfigService::isWebSocketSyncActive() const {
    return m_syncClient && m_syncClient->isConnected();
}

void ConfigService::onWebSocketConnected() {
    m_serverAvailable = true;
    emit serverStatusChanged(true);
}

void ConfigService::onWebSocketDisconnected() {
    if (!m_serverAvailable) return;
    m_serverAvailable = false;
    emit serverStatusChanged(false);
}

void ConfigService::onConfigSynced(const QMap<QString, QString> &configs) {
    emit allConfigsLoaded(configs);
}

void ConfigService::onConfigUpdated(const QString &key, const QString &value) {
    QMap<QString, QString> singleConfig;
    singleConfig[key] = value;
    emit allConfigsLoaded(singleConfig);
}

void ConfigService::onRoomListReceived(const QStringList &rooms) {
    emit roomListReceived(rooms);
}
