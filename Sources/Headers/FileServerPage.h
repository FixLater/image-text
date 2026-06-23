#ifndef FILESERVERPAGE_H
#define FILESERVERPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QDir>
#include <QVBoxLayout>
#include <QNetworkInterface>

class FileServerPage : public QWidget {
    Q_OBJECT

public:
    explicit FileServerPage(QWidget *parent = nullptr);
    ~FileServerPage() override;

    bool isRunning() const;
    QString serverAddress() const;
    int port() const;
    void onToggleServer();
    void refreshFromSettings();

signals:
    void statusChanged(bool running);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void setupUI();
    void startServer();
    void stopServer();
    void handleHttpRequest(QTcpSocket *socket, const QString &request);
    QString generateFileListHtml();
    QString getFileIcon(const QString &fileName);

    QTcpServer *m_server;
    bool m_running;
    int m_port;
    QString m_serveDir;
    QVBoxLayout *m_mainLayout;
    QPushButton *m_toggleBtn;
    QLabel *m_statusLabel;
    QLabel *m_addressLabel;
};

#endif // FILESERVERPAGE_H
