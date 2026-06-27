#include "WebSocketPage.h"
#include "ui_WebSocketPage.h"
#include "QtWebSocketClient.h"
#include "ChatLogWidget.h"
#include "SettingsDialog.h"
#include "ConfigService.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStandardItem>
#include <QMenu>
#include <QDateTime>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTextCursor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "StarBackground.h"

WebSocketPage::WebSocketPage(QWidget *parent) : QWidget(parent),
                                                ui(new Ui::WebSocketPage),
                                                m_contextRow(-1) {
    ui->setupUi(this);
    setObjectName("WebSocketPage");

    m_starBg = new StarBackground(this);
    m_starBg->lower();
    m_starBg->resize(size());

    applyApiFoxStyle();

    m_chatLog = new ChatLogWidget(this);

    ui->rightLayout->removeWidget(ui->log);
    ui->log->hide();
    ui->rightLayout->addWidget(m_chatLog);

    ui->mainSplitter->setSizes({500, 500});

    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();

    // 添加房间列表下拉框和加入按钮
    m_roomComboBox = new QComboBox(this);
    m_roomComboBox->setObjectName("roomComboBox");
    m_roomComboBox->setMinimumWidth(120);
    m_roomComboBox->view()->setStyleSheet(
        "QAbstractItemView {"
        "  background-color: #252830;"
        "  color: #e2e8f0;"
        "  border: 1px solid #3a3f4b;"
        "  border-radius: 6px;"
        "  outline: none;"
        "  padding: 2px;"
        "  show-decoration-selected: none;"
        "}"
        "QAbstractItemView::item {"
        "  padding: 2px 10px;"
        "  border: none;"
        "  min-height: 14px;"
        "}"
        "QAbstractItemView::item:selected {"
        "  background-color: transparent;"
        "  color: #e2e8f0;"
        "}"
        "QAbstractItemView::item:hover {"
        "  background-color: #1e3a5f;"
        "  color: #e2e8f0;"
        "}"
    );

    m_joinRoomBtn = new QPushButton("加入", this);
    m_joinRoomBtn->setObjectName("joinRoomBtn");
    m_joinRoomBtn->setFixedSize(50, 28);
    m_joinRoomBtn->setEnabled(false);
    connect(m_joinRoomBtn, &QPushButton::clicked, this, &WebSocketPage::onJoinRoomClicked);

    connect(m_roomComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.isEmpty() || m_currentRoom.isEmpty()) return;
        if (text == m_currentRoom) {
            m_joinRoomBtn->setText("加入");
            m_joinRoomBtn->setEnabled(false);
        } else {
            m_joinRoomBtn->setText("切换");
            m_joinRoomBtn->setEnabled(true);
        }
    });

    // 在 connectButton 后面插入房间控件
    ui->requestBarLayout->insertWidget(1, m_roomComboBox);
    ui->requestBarLayout->insertWidget(2, m_joinRoomBtn);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &WebSocketPage::onContextMenuRequested);
    m_chatLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_chatLog->viewport(), &QWidget::customContextMenuRequested, this, [this](QPoint pos) {
        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
            "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
        );
        QAction clearAction("清空日志", this);
        connect(&clearAction, &QAction::triggered, this, [this]() {
            m_chatLog->clearMessages();
            m_logMessages.clear();
        });
        menu.addAction(&clearAction);
        menu.exec(m_chatLog->viewport()->mapToGlobal(pos));
    });
    ui->message->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->message->setAcceptDrops(false);
    ui->message->installEventFilter(this);
    this->setAcceptDrops(true);

    connect(ui->message, &QTextEdit::textChanged, this, &WebSocketPage::updateSendButtonState);

    connect(ui->tabMessage, &QPushButton::clicked, this, [this]() {
        ui->message->show();
        ui->tableView->hide();
        ui->selectFile->hide();
        ui->tabMessage->setStyleSheet(
            "QPushButton { background: transparent; color: #0ea5e9; border: none;"
            "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
        );
        ui->tabFiles->setStyleSheet(
            "QPushButton { background: transparent; color: #475569; border: none;"
            "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { color: #94a3b8; }"
        );
        updateSendButtonState();
    });
    connect(ui->tabFiles, &QPushButton::clicked, this, [this]() {
        ui->message->hide();
        ui->tableView->show();
        ui->selectFile->show();
        ui->tabFiles->setStyleSheet(
            "QPushButton { background: transparent; color: #0ea5e9; border: none;"
            "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
        );
        ui->tabMessage->setStyleSheet(
            "QPushButton { background: transparent; color: #475569; border: none;"
            "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { color: #94a3b8; }"
        );
        updateSendButtonState();
    });

    ui->tabMessage->setStyleSheet(
        "QPushButton { background: transparent; color: #0ea5e9; border: none;"
            "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
    );
    ui->tabFiles->setStyleSheet(
        "QPushButton { background: transparent; color: #475569; border: none;"
        "  padding: 6px 8px; font-size: 9pt; font-weight: bold; }"
        "QPushButton:hover { color: #94a3b8; }"
    );
    ui->tableView->hide();
    ui->selectFile->hide();

    m_fileModel = new QStandardItemModel(this);

    // 连接 ConfigService 的房间列表信号
    if (SettingsDialog::configService()) {
        connect(SettingsDialog::configService(), &ConfigService::roomListReceived,
                this, &WebSocketPage::onRoomListReceived);
    }

    // 初始化 URL
    m_url = SettingsDialog::wsUrl();
    ui->location->setText(m_url);
}

WebSocketPage::~WebSocketPage() {
    if (m_client) {
        m_client->disconnectFromServer();
        m_client->deleteLater();
    }
    delete ui;
}

bool WebSocketPage::isConnected() const {
    return m_client && m_client->isConnected();
}

void WebSocketPage::refreshUrlFromSettings() {
    m_url = SettingsDialog::wsUrl();
    ui->location->setText(m_url);
}

void WebSocketPage::on_connectButton_clicked() {
    if (m_client && m_client->isConnected()) {
        m_client->disconnectFromServer();
        return;
    }

    if (m_client) {
        m_client->disconnectFromServer();
        m_client->deleteLater();
    }

    QString url = ui->location->text().trimmed();
    if (url.isEmpty()) url = SettingsDialog::wsUrl();
    m_url = url;

    WebSocketConfig config;

    const QString chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    int tokenLen = SettingsDialog::wsJwtTokenLength();
    QString token;
    token.reserve(tokenLen);
    for (int i = 0; i < tokenLen; i++) {
        token.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
    }
    config.jwtToken = token;

    config.pingIntervalMs = SettingsDialog::wsPingIntervalMs();
    config.reconnectIntervalMs = SettingsDialog::wsReconnectIntervalMs();
    config.maxReconnectAttempts = SettingsDialog::wsMaxReconnectAttempts();

    m_client = new QtWebSocketClient(url, config, this);

    connect(m_client, &QtWebSocketClient::connected, this, &WebSocketPage::onConnected);
    connect(m_client, &QtWebSocketClient::disconnected, this, &WebSocketPage::onDisconnected);
    connect(m_client, &QtWebSocketClient::messageReceived, this, &WebSocketPage::onMessageReceived);
    connect(m_client, &QtWebSocketClient::errorOccurred, this, &WebSocketPage::onErrorOccurred);
    connect(m_client, &QtWebSocketClient::reconnecting, this, &WebSocketPage::onReconnecting);
    connect(m_client, &QtWebSocketClient::chunkComplete, this, [this]() {
        appendLog("文件发送完成", "sent");
    });
    m_connecting = true;
    ui->connectButton->setEnabled(false);
    ui->connectButton->setText("连接中...");
    ui->connectButton->setStyleSheet(
        "QPushButton { background-color: #36383d; color: #555; border: none;"
        "  border-radius: 6px; padding: 6px 20px; font-size: 9pt; font-weight: bold; }"
    );
    ui->sendButton->setEnabled(false);
    ui->location->setEnabled(false);
    ui->message->setEnabled(false);
    ui->statusLabel->setProperty("connected", false);
    ui->statusLabel->setText("  ● 连接中...  ");
    ui->statusLabel->setStyleSheet("color: #f59e0b; font-size: 9pt; font-weight: bold;");
    m_client->connectToServer();
}

void WebSocketPage::onConnected() {
    m_connecting = false;

    // 自动加入房间：优先使用下拉框选中的房间，否则使用默认房间
    QString roomToJoin = m_roomComboBox->currentText();
    if (roomToJoin.isEmpty()) {
        roomToJoin = SettingsDialog::wsDefaultRoomId();
    }
    if (!roomToJoin.isEmpty()) {
        QJsonObject joinMsg;
        joinMsg["type"] = "join";
        joinMsg["roomId"] = roomToJoin;
        m_client->sendRawText(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));
    }

    ui->message->setEnabled(true);
    updateButtonStates(true);
    ui->statusLabel->setProperty("connected", true);
    updateStatusStyle();

    emit statusChanged(true);
}

void WebSocketPage::onDisconnected() {
    appendLog("已断开连接", "system");
    m_connecting = false;

    ui->message->setEnabled(true);
    updateButtonStates(false);
    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();
    m_currentRoom.clear();
    m_joinRoomBtn->setText("加入");
    m_joinRoomBtn->setEnabled(false);

    emit statusChanged(false);
}

void WebSocketPage::onMessageReceived(const QString &message) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        if (type == "joined") {
            QString roomId = obj["roomId"].toString();
            m_currentRoom = roomId;
            appendLog("已加入房间: " + roomId, "system");
            m_joinRoomBtn->setText("加入");
            m_joinRoomBtn->setEnabled(false);
            return;
        }
        if (type == "message" && obj.contains("content")) {
            QString content = obj["content"].toString();
            appendLog(content, "info");
            emit messageReceived(content);
            return;
        }
        if (type == "room_list") {
            QJsonArray roomsArray = obj["rooms"].toArray();
            QStringList rooms;
            for (const auto &room : roomsArray) {
                rooms.append(room.toString());
            }
            onRoomListReceived(rooms);
            return;
        }
        if (type == "chunk_ack") {
            if (m_client) m_client->handleChunkAck();
            return;
        }
        if (type == "chunk_progress") {
            int received = obj["received"].toInt();
            int total = obj["total"].toInt();
            appendLog(QString("分片进度: %1/%2").arg(received).arg(total), "system");
            if (m_client) m_client->handleChunkAck();
            return;
        }
        if (type == "file" && obj.contains("content")) {
            appendLog("文件: " + obj["content"].toString(), "info");
            return;
        }
        if (obj.contains("msg")) {
            appendLog(obj["msg"].toString(), "system");
            return;
        }
    }

    appendLog(message, "system");
    emit messageReceived(message);
}

void WebSocketPage::onErrorOccurred(const QString &error) {
    appendLog("✗ " + error, "system");
    m_connecting = false;
    ui->message->setEnabled(true);
    updateButtonStates(false);
    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();
}

void WebSocketPage::onReconnecting(int attempt, int maxAttempts) {
    appendLog(QString("⟳ 重连中... (%1/%2)").arg(attempt).arg(maxAttempts), "system");
}

void WebSocketPage::on_sendButton_clicked() {
    if (!m_client || !m_client->isConnected()) {
        appendLog("未连接，无法发送", "system");
        return;
    }

    if (ui->tableView->isVisible() && m_fileModel->rowCount() > 0) {
        for (int i = 0; i < m_fileModel->rowCount(); i++) {
            auto *item = m_fileModel->item(i, 0);
            if (item) {
                m_client->sendImage(item->text());
                appendLog("发送图片: " + QFileInfo(item->text()).fileName(), "sent");
            }
        }
        m_fileModel->clear();
    } else {
        auto message = ui->message->toPlainText().trimmed();
        if (message.isEmpty()) return;
        m_client->sendMessage(message);
        appendLog(message, "sent");
        ui->message->clear();
    }
}

void WebSocketPage::on_selectFile_clicked() {
    const auto files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                     QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                                     "所有文件 (*.*)");
    if (!files.isEmpty()) {
        addFiles(files);
    }
}

void WebSocketPage::addFiles(const QStringList &files) {
    for (const auto &file: files) {
        auto *item = new QStandardItem(file);
        item->setData(file, Qt::UserRole);
        m_fileModel->appendRow(item);
    }

    ui->tableView->setModel(m_fileModel);
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setShowGrid(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setDefaultSectionSize(32);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    updateSendButtonState();
}

void WebSocketPage::onContextMenuRequested(QPoint pos) {
    QModelIndex index = ui->tableView->indexAt(pos);
    if (!index.isValid()) return;
    m_contextRow = index.row();

    QMenu contextMenu(this);
    QAction actionDelete("删除", this);
    connect(&actionDelete, &QAction::triggered, this, &WebSocketPage::onActionDelete);
    contextMenu.addAction(&actionDelete);

    QAction actionReplace("更换", this);
    connect(&actionReplace, &QAction::triggered, this, &WebSocketPage::onActionReplace);
    contextMenu.addAction(&actionReplace);

    contextMenu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

void WebSocketPage::onActionDelete() {
    if (m_contextRow >= 0 && m_contextRow < m_fileModel->rowCount()) {
        m_fileModel->removeRow(m_contextRow);
        m_contextRow = -1;
        updateSendButtonState();
    }
}

void WebSocketPage::onActionReplace() {
    if (m_contextRow < 0 || m_contextRow >= m_fileModel->rowCount()) return;

    auto file = QFileDialog::getOpenFileName(this, QStringLiteral("更换文件"),
                                             QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                             "所有文件 (*.*)");
    if (file.isEmpty()) return;

    auto *item = m_fileModel->item(m_contextRow);
    if (item) {
        item->setText(file);
        item->setData(file, Qt::UserRole);
    }
    m_contextRow = -1;
}

void WebSocketPage::appendLog(const QString &message, const QString &type) {
    ChatMessage msg;
    msg.text = message;
    msg.type = type;
    m_logMessages.append(msg);

    m_chatLog->appendMessage(message, type);

    QString color = "#94a3b8";
    if (type == "sent") color = "#0ea5e9";
    else if (type == "system") color = "#f59e0b";
    emit logAppended(QString("<span style='color:%1;'>%2</span>").arg(color, message.toHtmlEscaped()));
}

void WebSocketPage::updateButtonStates(bool connected) {
    ui->connectButton->setEnabled(true);
    ui->location->setEnabled(!connected);
    if (connected) {
        ui->connectButton->setText("断开");
        ui->connectButton->setStyleSheet(
            "QPushButton { background-color: #ef4444; color: #ffffff; border: none;"
            "  border-radius: 6px; padding: 6px 20px; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { background-color: #f87171; }"
            "QPushButton:pressed { background-color: #dc2626; }"
        );
    } else {
        ui->connectButton->setText("连接");
        ui->connectButton->setStyleSheet(
            "QPushButton { background-color: #0ea5e9; color: #ffffff; border: none;"
            "  border-radius: 6px; padding: 6px 20px; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { background-color: #38bdf8; }"
            "QPushButton:pressed { background-color: #0284c7; }"
        );
        m_currentRoom.clear();
        m_joinRoomBtn->setText("加入");
        m_joinRoomBtn->setEnabled(false);
    }
    updateSendButtonState();
}

void WebSocketPage::updateSendButtonState() {
    bool connected = m_client && m_client->isConnected();
    bool canSend = false;
    if (ui->tableView->isVisible()) {
        canSend = connected && m_fileModel && m_fileModel->rowCount() > 0;
    } else {
        canSend = connected && !ui->message->toPlainText().trimmed().isEmpty();
    }
    ui->sendButton->setEnabled(canSend);
}

void WebSocketPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void WebSocketPage::dropEvent(QDropEvent *event) {
    QStringList files;
    for (const QUrl &url: event->mimeData()->urls()) {
        files.append(url.toLocalFile());
    }
    addFiles(files);
}

bool WebSocketPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->message && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ControlModifier)) {
            on_sendButton_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void WebSocketPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_starBg) {
        m_starBg->resize(size());
    }
}

void WebSocketPage::onRoomListReceived(const QStringList &rooms) {
    m_roomComboBox->clear();
    m_roomComboBox->addItems(rooms);

    // 设置默认选中房间
    QString defaultRoom = SettingsDialog::wsDefaultRoomId();
    int index = m_roomComboBox->findText(defaultRoom);
    if (index >= 0) {
        m_roomComboBox->setCurrentIndex(index);
    }
    {
        QFile lf(QCoreApplication::applicationDirPath() + "/ws_debug.log");
        if (lf.open(QIODevice::Append | QIODevice::Text))
            lf.write(QString("%1 [WSPage] rooms:%2 default:%3 idx:%4\n")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                .arg(rooms.join(",")).arg(defaultRoom).arg(index).toUtf8());
    }

    // 如果当前房间已知，根据选中房间更新按钮状态
    if (!m_currentRoom.isEmpty()) {
        QString selected = m_roomComboBox->currentText();
        if (selected == m_currentRoom) {
            m_joinRoomBtn->setText("加入");
            m_joinRoomBtn->setEnabled(false);
        } else {
            m_joinRoomBtn->setText("切换");
            m_joinRoomBtn->setEnabled(true);
        }
    } else {
        m_joinRoomBtn->setEnabled(true);
    }
}

void WebSocketPage::onJoinRoomClicked() {
    if (!m_client || !m_client->isConnected()) return;

    QString roomId = m_roomComboBox->currentText();
    if (roomId.isEmpty()) return;

    // 如果当前在其他房间，先退出
    if (!m_currentRoom.isEmpty() && m_currentRoom != roomId) {
        QJsonObject leaveMsg;
        leaveMsg["type"] = "leave";
        leaveMsg["roomId"] = m_currentRoom;
        m_client->sendRawText(QJsonDocument(leaveMsg).toJson(QJsonDocument::Compact));
    }

    QJsonObject joinMsg;
    joinMsg["type"] = "join";
    joinMsg["roomId"] = roomId;
    m_client->sendRawText(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));
}

void WebSocketPage::applyApiFoxStyle() {
    setStyleSheet(
        "* { font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif; }"
        "QWidget { background: transparent; }"

        "#connectButton {"
        "  background-color: #0ea5e9; color: #ffffff; border: none;"
        "  border-radius: 6px; padding: 6px 20px; font-size: 9pt; font-weight: bold;"
        "}"
        "#connectButton:hover { background-color: #38bdf8; }"
        "#connectButton:pressed { background-color: #0284c7; }"
        "#connectButton:disabled { background-color: #36383d; color: #555; }"

        "QLineEdit {"
        "  background: transparent; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  padding: 8px 12px; font-size: 10pt; selection-background-color: #0ea5e9;"
        "}"
        "QLineEdit:focus { border: 1px solid #0ea5e9; }"
        "QLineEdit:disabled { background: transparent; color: #555; border: 1px solid #26282c; }"

        "QTextEdit {"
        "  background: transparent; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  padding: 8px; font-size: 10pt; selection-background-color: #0ea5e9;"
        "}"
        "QTextEdit:focus { border: 1px solid #0ea5e9; }"
        "QTextEdit:disabled { background-color: #26282c; color: #555; border: 1px solid #26282c; }"

        "QTableView {"
        "  background-color: transparent; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  gridline-color: #2a2c30;"
        "  selection-background-color: rgba(14, 165, 233, 0.25);"
        "  selection-color: #e2e8f0; outline: none;"
        "}"
        "QTableView:focus { border: 1px solid #0ea5e9; }"
        "QTableView::item { padding: 4px 8px; border-bottom: 1px solid #2a2c30; }"
        "QTableView::item:selected { background-color: rgba(14, 165, 233, 0.2); border-left: 3px solid #0ea5e9; }"
        "QTableView::item:hover { background-color: rgba(14, 165, 233, 0.1); }"
        "QHeaderView::section {"
        "  background-color: #26282c; color: #94a3b8;"
        "  border: none; border-bottom: 1px solid #36383d;"
        "  padding: 6px 8px; font-size: 9pt; font-weight: bold;"
        "}"

        "#tabMessage, #tabFiles {"
        "  background: transparent; color: #475569; border: none;"
        "  border-bottom: 2px solid transparent; padding: 6px 8px;"
        "  font-size: 9pt; font-weight: bold; border-radius: 0;"
        "}"
        "#tabMessage:hover, #tabFiles:hover { color: #94a3b8; }"

        "#selectFile {"
        "  background-color: transparent; color: #64748b;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  padding: 3px 10px; font-size: 8pt;"
        "}"
        "#selectFile:hover { color: #e2e8f0; border-color: #64748b; }"

        "#sendButton {"
        "  background-color: #0ea5e9; color: #ffffff; border: none;"
        "  border-radius: 6px; padding: 6px 24px; font-size: 9pt; font-weight: bold;"
        "}"
        "#sendButton:hover { background-color: #38bdf8; }"
        "#sendButton:pressed { background-color: #0284c7; }"
        "#sendButton:disabled { background-color: #36383d; color: #555; }"

        "#roomComboBox {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252830, stop:1 #1e2128);"
        "  color: #e2e8f0;"
        "  border: 1px solid #3a3f4b;"
        "  border-radius: 8px;"
        "  padding: 6px 12px;"
        "  font-size: 9pt;"
        "  min-width: 100px;"
        "}"
        "#roomComboBox:hover {"
        "  border-color: #4a9eff;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a3040, stop:1 #232830);"
        "}"
        "#roomComboBox:disabled {"
        "  background: #151820;"
        "  color: #4a5568;"
        "  border-color: #2d333b;"
        "}"
        "#roomComboBox::drop-down {"
        "  border: none;"
        "  width: 32px;"
        "  background: transparent;"
        "}"
        "#roomComboBox::down-arrow {"
        "  image: url(:/icons/arrow_down.svg);"
        "  width: 12px; height: 12px;"
        "}"
        "#roomComboBox::down-arrow:on {"
        "  image: url(:/icons/arrow_up.svg);"
        "}"

        "#joinRoomBtn {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #34d399, stop:1 #10b981);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-size: 9pt; font-weight: bold;"
        "}"
        "#joinRoomBtn:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4ade80, stop:1 #34d399);"
        "}"
        "#joinRoomBtn:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #059669, stop:1 #047857);"
        "}"
        "#joinRoomBtn:disabled {"
        "  background: #36383d; color: #555;"
        "}"

        "#requestBar { background: transparent; }"
        "#statusLabel { font-size: 9pt; }"

        "QSplitter::handle { background: transparent; }"
        "QSplitter::handle:horizontal { width: 0px; }"
        "QSplitter::handle:vertical { height: 0px; }"

        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }"
        "QScrollBar::handle:horizontal { background-color: #36383d; border-radius: 4px; min-width: 20px; }"
        "QScrollBar::handle:horizontal:hover { background-color: #475569; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

        "QMenu {"
        "  background-color: #2a2c30; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px; padding: 4px;"
        "}"
        "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
    );
}

void WebSocketPage::updateStatusStyle() {
    bool connected = ui->statusLabel->property("connected").toBool();
    if (connected) {
        ui->statusLabel->setText("  ● Connected  ");
        ui->statusLabel->setStyleSheet("color: #34d399; font-size: 9pt; font-weight: bold;");
    } else {
        ui->statusLabel->setText("  ● Disconnected  ");
        ui->statusLabel->setStyleSheet("color: #64748b; font-size: 9pt;");
    }
}
