#include "ConfigService.h"
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

ConfigService::ConfigService(const QString &baseUrl, QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_baseUrl(baseUrl) {
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
    return m_serverAvailable;
}
