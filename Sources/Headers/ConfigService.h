#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

class ConfigService : public QObject {
    Q_OBJECT

public:
    explicit ConfigService(const QString &baseUrl, QObject *parent = nullptr);

    void loadAll();
    void saveAll(const QMap<QString, QString> &configs);
    bool isServerAvailable() const;

signals:
    void allConfigsLoaded(const QMap<QString, QString> &configs);
    void serverStatusChanged(bool available);

private slots:
    void onLoadFinished(QNetworkReply *reply);
    void onSaveFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
    bool m_serverAvailable = false;
};

#endif // CONFIGSERVICE_H
