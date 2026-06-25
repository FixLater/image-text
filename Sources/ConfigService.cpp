#include "ConfigService.h"
#include "ConfigSyncClient.h"
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

ConfigService::ConfigService(const QString &baseUrl, QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_baseUrl(baseUrl) {
}

ConfigService::~ConfigService() {
    stopWebSocketSync();
}

void ConfigService::loadAll() {
    QNetworkRequest request(QUrl(m_baseUrl + "/config/list"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onLoadFinished(reply);
    });
}

void ConfigService::onLoadFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        bool was = m_serverAvailable;
        m_serverAvailable = false;
        if (was) emit serverStatusChanged(false);
        return;
    }

    m_serverAvailable = true;
    emit serverStatusChanged(true);

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) return;

    QJsonArray arr = doc.object()["data"].toArray();
    QMap<QString, QString> configs;
    for (const auto &item : arr) {
        QJsonObject obj = item.toObject();
        configs[obj["configKey"].toString()] = obj["configValue"].toString();
    }
    emit allConfigsLoaded(configs);
}

void ConfigService::saveAll(const QMap<QString, QString> &configs) {
    // 通过 WebSocket 逐条推送更新
    if (m_syncClient && m_syncClient->isConnected()) {
        for (auto it = configs.constBegin(); it != configs.constEnd(); ++it) {
            m_syncClient->pushUpdate(it.key(), it.value());
        }
    }

    // 同时通过 HTTP 备份保存
    QJsonObject body;
    for (auto it = configs.constBegin(); it != configs.constEnd(); ++it) {
        body[it.key()] = it.value();
    }

    QNetworkRequest request(QUrl(m_baseUrl + "/config/saveAll"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSaveFinished(reply);
    });
}

void ConfigService::onSaveFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        bool was = m_serverAvailable;
        m_serverAvailable = false;
        if (was) emit serverStatusChanged(false);
    }
}

bool ConfigService::isServerAvailable() const {
    return m_serverAvailable || (m_syncClient && m_syncClient->isConnected());
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
