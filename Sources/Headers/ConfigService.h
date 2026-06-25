#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

class ConfigSyncClient;

class ConfigService : public QObject {
    Q_OBJECT

public:
    explicit ConfigService(const QString &baseUrl, QObject *parent = nullptr);
    ~ConfigService() override;

    void loadAll();
    void saveAll(const QMap<QString, QString> &configs);
    bool isServerAvailable() const;

    void startWebSocketSync(const QString &wsUrl, const QString &jwtToken);
    void stopWebSocketSync();
    bool isWebSocketSyncActive() const;

signals:
    void allConfigsLoaded(const QMap<QString, QString> &configs);
    void serverStatusChanged(bool available);

private slots:
    void onLoadFinished(QNetworkReply *reply);
    void onSaveFinished(QNetworkReply *reply);
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onConfigSynced(const QMap<QString, QString> &configs);
    void onConfigUpdated(const QString &key, const QString &value);

private:
    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
    bool m_serverAvailable = false;
    ConfigSyncClient *m_syncClient = nullptr;
};

#endif // CONFIGSERVICE_H
