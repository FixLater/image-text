#include "FileServerPage.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QDesktopServices>
#include <QGraphicsDropShadowEffect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileIconProvider>
#include <QStandardPaths>

FileServerPage::FileServerPage(QWidget *parent)
    : QWidget(parent)
    , m_server(new QTcpServer(this))
    , m_running(false)
    , m_port(SettingsDialog::fileServerPort())
    , m_serveDir(QDir::currentPath())
{
    setupUI();
    connect(m_server, &QTcpServer::newConnection, this, &FileServerPage::onNewConnection);
}

FileServerPage::~FileServerPage() {
    stopServer();
}

void FileServerPage::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(32, 24, 32, 24);
    m_mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel("HTTP 文件服务器");
    titleLabel->setStyleSheet(
        "font-size: 16pt; font-weight: bold; color: #e2e8f0; background: transparent; border: none;"
    );
    m_mainLayout->addWidget(titleLabel);

    auto *descLabel = new QLabel("启动后可在局域网内通过浏览器访问文件列表");
    descLabel->setStyleSheet(
        "font-size: 9pt; color: #64748b; background: transparent; border: none;"
    );
    m_mainLayout->addWidget(descLabel);

    m_mainLayout->addSpacing(8);

    auto *cardWidget = new QWidget();
    cardWidget->setObjectName("serverCard");
    cardWidget->setStyleSheet(
        "#serverCard { background-color: #1a1b1e; border: 1px solid #36383d; border-radius: 8px; }"
    );
    auto *cardLayout = new QVBoxLayout(cardWidget);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(12);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);

    m_statusLabel = new QLabel("●");
    m_statusLabel->setStyleSheet(
        "color: #64748b; font-size: 12pt; background: transparent; border: none;"
    );
    statusRow->addWidget(m_statusLabel);

    auto *statusTextLabel = new QLabel("服务状态");
    statusTextLabel->setStyleSheet(
        "color: #94a3b8; font-size: 9pt; background: transparent; border: none;"
    );
    statusRow->addWidget(statusTextLabel);

    statusRow->addStretch(1);

    m_toggleBtn = new QPushButton("启动服务");
    m_toggleBtn->setFixedSize(80, 30);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setStyleSheet(
        "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { background-color: #38bdf8; }"
    );
    connect(m_toggleBtn, &QPushButton::clicked, this, &FileServerPage::onToggleServer);
    statusRow->addWidget(m_toggleBtn);

    cardLayout->addLayout(statusRow);

    m_addressLabel = new QLabel("未启动");
    m_addressLabel->setStyleSheet(
        "color: #475569; font-size: 9pt; background: transparent; border: none;"
    );
    cardLayout->addWidget(m_addressLabel);

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    cardWidget->setGraphicsEffect(shadow);

    m_mainLayout->addWidget(cardWidget);
    m_mainLayout->addStretch(1);
}

bool FileServerPage::isRunning() const {
    return m_running;
}

QString FileServerPage::serverAddress() const {
    if (!m_running) return QString();

    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            return addr.toString();
        }
    }
    return "127.0.0.1";
}

int FileServerPage::port() const {
    return m_port;
}

void FileServerPage::onToggleServer() {
    if (m_running) {
        stopServer();
    } else {
        startServer();
    }
}

void FileServerPage::startServer() {
    if (m_server->listen(QHostAddress::Any, m_port)) {
        m_running = true;
        m_toggleBtn->setText("停止服务");
        m_toggleBtn->setStyleSheet(
            "QPushButton { background-color: #dc2626; color: white; border: none; border-radius: 4px; font-size: 9pt; }"
            "QPushButton:hover { background-color: #ef4444; }"
        );
        m_statusLabel->setStyleSheet(
            "color: #34d399; font-size: 12pt; background: transparent; border: none;"
        );
        QString addr = QString("http://%1:%2").arg(serverAddress()).arg(m_port);
        m_addressLabel->setText(addr);
        m_addressLabel->setStyleSheet(
            "color: #0ea5e9; font-size: 9pt; background: transparent; border: none; text-decoration: underline;"
        );
        m_addressLabel->setOpenExternalLinks(true);
        connect(m_addressLabel, &QLabel::linkActivated, this, [](const QString &link) {
            QDesktopServices::openUrl(QUrl(link));
        });
        emit statusChanged(true);
    } else {
        m_statusLabel->setStyleSheet(
            "color: #dc2626; font-size: 12pt; background: transparent; border: none;"
        );
        m_addressLabel->setText("启动失败: " + m_server->errorString());
        m_addressLabel->setStyleSheet(
            "color: #dc2626; font-size: 9pt; background: transparent; border: none;"
        );
    }
}

void FileServerPage::stopServer() {
    m_server->close();
    m_running = false;
    m_toggleBtn->setText("启动服务");
    m_toggleBtn->setStyleSheet(
        "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { background-color: #38bdf8; }"
    );
    m_statusLabel->setStyleSheet(
        "color: #64748b; font-size: 12pt; background: transparent; border: none;"
    );
    m_addressLabel->setText("未启动");
    m_addressLabel->setStyleSheet(
        "color: #475569; font-size: 9pt; background: transparent; border: none;"
    );
    emit statusChanged(false);
}

void FileServerPage::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &FileServerPage::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &FileServerPage::onDisconnected);
    }
}

void FileServerPage::onReadyRead() {
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QString request = QString::fromUtf8(data);

    handleHttpRequest(socket, request);
}

void FileServerPage::onDisconnected() {
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

QString FileServerPage::getFileIcon(const QString &fileName) {
    QString lower = fileName.toLower();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") ||
        lower.endsWith(".gif") || lower.endsWith(".bmp") || lower.endsWith(".svg")) {
        return "🖼️";
    } else if (lower.endsWith(".mp4") || lower.endsWith(".avi") || lower.endsWith(".mkv") ||
               lower.endsWith(".mov") || lower.endsWith(".wmv")) {
        return "🎬";
    } else if (lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".flac") ||
               lower.endsWith(".aac") || lower.endsWith(".ogg")) {
        return "🎵";
    } else if (lower.endsWith(".pdf")) {
        return "📄";
    } else if (lower.endsWith(".zip") || lower.endsWith(".rar") || lower.endsWith(".7z") ||
               lower.endsWith(".tar") || lower.endsWith(".gz")) {
        return "📦";
    } else if (lower.endsWith(".txt") || lower.endsWith(".md") || lower.endsWith(".log")) {
        return "📝";
    } else if (lower.endsWith(".exe") || lower.endsWith(".msi")) {
        return "⚙️";
    } else if (lower.endsWith(".doc") || lower.endsWith(".docx")) {
        return "📘";
    } else if (lower.endsWith(".xls") || lower.endsWith(".xlsx")) {
        return "📊";
    } else if (lower.endsWith(".ppt") || lower.endsWith(".pptx")) {
        return "📙";
    }
    return "📁";
}

QString FileServerPage::generateFileListHtml() {
    QDir dir(m_serveDir);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                              QDir::DirsFirst | QDir::Name);

    QString html = R"(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>)" + dir.dirName() + R"( - 文件服务器</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; background: #0f172a; color: #e2e8f0; padding: 20px; }
  h1 { font-size: 20px; margin-bottom: 16px; color: #38bdf8; }
  .breadcrumb { color: #64748b; font-size: 14px; margin-bottom: 16px; }
  .breadcrumb a { color: #38bdf8; text-decoration: none; }
  .breadcrumb a:hover { text-decoration: underline; }
  table { width: 100%; border-collapse: collapse; }
  th { text-align: left; padding: 10px 12px; color: #64748b; border-bottom: 1px solid #1e293b; font-size: 12px; }
  td { padding: 10px 12px; border-bottom: 1px solid #1e293b; }
  tr:hover { background: #1e293b; }
  a { color: #e2e8f0; text-decoration: none; }
  a:hover { color: #38bdf8; }
  .icon { margin-right: 8px; }
  .size { color: #64748b; font-size: 13px; }
  .empty { color: #475569; text-align: center; padding: 40px; }
</style>
</head>
<body>
<h1>📂 )" + dir.dirName() + R"(</h1>
<div class="breadcrumb">)" + QString("共 %1 项").arg(entries.size()) + R"(</div>
<table>
<tr><th>名称</th><th style="text-align:right">大小</th></tr>
)";

    if (dir.exists() && dir.cdUp()) {
        QString parentPath = dir.absolutePath();
        QString encodedPath = QString::fromUtf8(QUrl::toPercentEncoding(parentPath.replace("\\", "/")));
        html += QString(R"(<tr><td><a href="/files?dir=%1">📁 ..</a></td><td class="size">-</td></tr>)").arg(encodedPath);
        dir.cd(dir.dirName());
    }

    for (const QFileInfo &entry : entries) {
        QString name = entry.fileName();
        QString encodedName = QString::fromUtf8(QUrl::toPercentEncoding(name));
        QString icon = entry.isDir() ? "📁" : getFileIcon(name);
        QString size = entry.isDir() ? "-" : QString("%1 KB").arg(entry.size() / 1024.0, 0, 'f', 1);

        if (entry.isDir()) {
            QString dirPath = entry.absoluteFilePath().replace("\\", "/");
            QString encodedDirPath = QString::fromUtf8(QUrl::toPercentEncoding(dirPath));
            html += QString(R"(<tr><td><a href="/files?dir=%1"><span class="icon">%2</span>%3</a></td><td class="size">%4</td></tr>)")
                        .arg(encodedDirPath, icon, name, size);
        } else {
            QString filePath = entry.absoluteFilePath().replace("\\", "/");
            QString encodedFilePath = QString::fromUtf8(QUrl::toPercentEncoding(filePath));
            html += QString(R"(<tr><td><a href="/download?file=%1"><span class="icon">%2</span>%3</a></td><td class="size">%4</td></tr>)")
                        .arg(encodedFilePath, icon, name, size);
        }
    }

    html += QStringLiteral("</table></body></html>");
    return html;
}

void FileServerPage::handleHttpRequest(QTcpSocket *socket, const QString &request) {
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }

    QStringList parts = lines[0].split(" ");
    if (parts.size() < 2) {
        socket->disconnectFromHost();
        return;
    }

    QString method = parts[0];
    QString path = parts[1];

    if (method == "GET" && path.startsWith("/files?dir=")) {
        QString dirParam = QUrl::fromPercentEncoding(path.mid(11).toUtf8());
        if (QDir(dirParam).exists()) {
            m_serveDir = dirParam;
        }
        QString html = generateFileListHtml();
        QByteArray response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + QByteArray::number(html.toUtf8().size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + html.toUtf8();
        socket->write(response);
        socket->disconnectFromHost();
    } else if (method == "GET" && path.startsWith("/download?file=")) {
        QString filePath = QUrl::fromPercentEncoding(path.mid(15).toUtf8());
        QFile file(filePath);
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QFileInfo info(filePath);
            QString contentType = "application/octet-stream";
            QString lower = info.fileName().toLower();
            if (lower.endsWith(".html") || lower.endsWith(".htm")) contentType = "text/html";
            else if (lower.endsWith(".css")) contentType = "text/css";
            else if (lower.endsWith(".js")) contentType = "application/javascript";
            else if (lower.endsWith(".json")) contentType = "application/json";
            else if (lower.endsWith(".png")) contentType = "image/png";
            else if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) contentType = "image/jpeg";
            else if (lower.endsWith(".gif")) contentType = "image/gif";
            else if (lower.endsWith(".svg")) contentType = "image/svg+xml";
            else if (lower.endsWith(".pdf")) contentType = "application/pdf";
            else if (lower.endsWith(".txt")) contentType = "text/plain";

            QByteArray fileData = file.readAll();
            QByteArray response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + contentType.toUtf8() + "\r\n"
                "Content-Disposition: attachment; filename=\"" + info.fileName().toUtf8() + "\"\r\n"
                "Content-Length: " + QByteArray::number(fileData.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + fileData;
            socket->write(response);
        } else {
            QByteArray response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n";
            socket->write(response);
        }
        socket->disconnectFromHost();
    } else {
        QString html = generateFileListHtml();
        QByteArray response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + QByteArray::number(html.toUtf8().size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + html.toUtf8();
        socket->write(response);
        socket->disconnectFromHost();
    }
}
