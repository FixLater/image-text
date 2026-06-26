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
#include <QRegularExpression>
#include <QTextCursor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "TableItemDelegate.h"
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
    m_roomComboBox->setPlaceholderText("房间列表");
    m_roomComboBox->setEnabled(false);
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

    auto *joinRoomBtn = new QPushButton("加入", this);
    joinRoomBtn->setObjectName("joinRoomBtn");
    joinRoomBtn->setFixedSize(50, 28);
    joinRoomBtn->setEnabled(false);
    connect(joinRoomBtn, &QPushButton::clicked, this, &WebSocketPage::onJoinRoomClicked);

    // 在 connectButton 后面插入房间控件
    ui->requestBarLayout->insertWidget(1, m_roomComboBox);
    ui->requestBarLayout->insertWidget(2, joinRoomBtn);

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
            if (m_activeTabIndex >= 0) {
                m_tabs[m_activeTabIndex].logEntries.clear();
                m_tabs[m_activeTabIndex].logMessages.clear();
            }
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
        if (m_activeTabIndex < 0) return;
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
        if (m_activeTabIndex < 0) return;
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

    // 连接 ConfigService 的房间列表信号
    if (SettingsDialog::configService()) {
        connect(SettingsDialog::configService(), &ConfigService::roomListReceived,
                this, &WebSocketPage::onRoomListReceived);
    }

    addTab(SettingsDialog::wsUrl());
}

WebSocketPage::~WebSocketPage() {
    for (const auto &tab: m_tabs) {
        if (tab.client) {
            tab.client->disconnectFromServer();
            tab.client->deleteLater();
        }
    }
    delete ui;
}

int WebSocketPage::tabCount() const {
    return m_tabs.size();
}

int WebSocketPage::activeTabIndex() const {
    return m_activeTabIndex;
}

QString WebSocketPage::tabTitle(int index) const {
    if (index < 0 || index >= m_tabs.size()) return {};
    QString display = m_tabs[index].url;
    display.replace("ws://", "").replace("wss://", "");
    if (display.length() > 22) display = display.left(19) + "...";
    return display;
}

bool WebSocketPage::isConnected() const {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return false;
    auto &tab = m_tabs[m_activeTabIndex];
    return tab.client && tab.client->isConnected();
}

void WebSocketPage::addTab(const QString &url) {
    TabState state;
    state.url = url.isEmpty() ? SettingsDialog::wsUrl() : url;
    state.fileModel = new QStandardItemModel(this);
    m_tabs.append(state);

    switchTab(m_tabs.size() - 1);
    emit tabsChanged();
}

void WebSocketPage::connectToCurrentTab() {
    on_connectButton_clicked();
}

void WebSocketPage::refreshUrlFromSettings() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    QString url = SettingsDialog::wsUrl();
    m_tabs[m_activeTabIndex].url = url;
    ui->location->setText(url);
}

QString WebSocketPage::tabLogHtml(int index) const {
    if (index < 0 || index >= m_tabs.size()) return {};
    QString html;
    for (const auto &msg : m_tabs[index].logMessages) {
        QString escaped = msg.text;
        escaped.replace("&", "&amp;");
        escaped.replace("<", "&lt;");
        escaped.replace(">", "&gt;");
        QString cls = (msg.type == "sent") ? "sent" : (msg.type == "info" ? "info" : "sys-info");
        html += QString("<span class='%1'>%2</span><br>").arg(cls, escaped);
    }
    return html;
}

void WebSocketPage::switchTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    if (m_activeTabIndex >= 0 && m_activeTabIndex < m_tabs.size()) {
        auto &oldTab = m_tabs[m_activeTabIndex];
        oldTab.url = ui->location->text();
        oldTab.messageText = ui->message->toPlainText();
        oldTab.showFiles = ui->tableView->isVisible();
    }

    m_activeTabIndex = index;
    auto &tab = m_tabs[index];

    ui->location->setText(tab.url);

    m_chatLog->clearMessages();
    for (const auto &msg : tab.logMessages) {
        m_chatLog->appendMessage(msg.text, msg.type);
    }

    ui->message->setText(tab.messageText);

    bool hasClient = tab.client && tab.client->isConnected();
    bool isConnecting = tab.connecting;
    ui->connectButton->setEnabled(!isConnecting);
    ui->location->setEnabled(!hasClient && !isConnecting);
    ui->sendButton->setEnabled(hasClient);
    ui->message->setEnabled(hasClient);

    if (isConnecting) {
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
    } else {
        updateButtonStates(hasClient);
    }

    if (tab.showFiles) {
        ui->message->hide();
        ui->tableView->show();
        ui->selectFile->show();
    } else {
        ui->message->show();
        ui->tableView->hide();
        ui->selectFile->hide();
    }

    if (tab.fileModel->rowCount() > 0) {
        ui->tableView->setModel(tab.fileModel);
        ui->tableView->horizontalHeader()->hide();
        ui->tableView->verticalHeader()->hide();
        ui->tableView->setShowGrid(false);
        ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui->tableView->verticalHeader()->setDefaultSectionSize(32);
        ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->tableView->setItemDelegate(new TableItemDelegate(ui->tableView));
    } else {
        ui->tableView->setModel(nullptr);
    }

    emit tabsChanged();
}

void WebSocketPage::closeTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    auto &tab = m_tabs[index];
    if (tab.client) {
        tab.client->disconnectFromServer();
        tab.client->deleteLater();
    }
    if (tab.fileModel) {
        tab.fileModel->deleteLater();
    }

    m_tabs.removeAt(index);

    if (m_tabs.isEmpty()) {
        addTab();
        return;
    }

    if (m_activeTabIndex >= m_tabs.size()) {
        m_activeTabIndex = m_tabs.size() - 1;
    } else if (m_activeTabIndex > index) {
        m_activeTabIndex--;
    } else if (m_activeTabIndex == index) {
        m_activeTabIndex = qMin(index, m_tabs.size() - 1);
    }

    switchTab(m_activeTabIndex);
}

void WebSocketPage::updateUIState() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    auto &tab = m_tabs[m_activeTabIndex];
    bool hasClient = tab.client && tab.client->isConnected();
    ui->connectButton->setEnabled(true);
    ui->location->setEnabled(!hasClient);
    updateButtonStates(hasClient);
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
        "  background-color: #1f2023; color: #e2e8f0;"
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
        "#roomComboBox QAbstractItemView {"
        "  background-color: #252830;"
        "  color: #e2e8f0;"
        "  border: 1px solid #3a3f4b;"
        "  border-radius: 6px;"
        "  outline: none;"
        "  padding: 2px;"
        "  show-decoration-selected: none;"
        "}"
        "#roomComboBox::item {"
        "  padding: 4px 10px;"
        "  border: none;"
        "  min-height: 18px;"
        "}"
        "#roomComboBox::item:selected {"
        "  background-color: transparent;"
        "  color: #e2e8f0;"
        "}"
        "#roomComboBox::item:hover {"
        "  background-color: #1e3a5f;"
        "  color: #e2e8f0;"
        "}"

        "#joinRoomBtn {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #34d399, stop:1 #10b981);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-size: 9pt;"
        "  font-weight: bold;"
        "  padding: 6px 14px;"
        "}"
        "#joinRoomBtn:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4ade80, stop:1 #34d399);"
        "}"
        "#joinRoomBtn:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #059669, stop:1 #047857);"
        "}"
        "#joinRoomBtn:disabled {"
        "  background: #36383d;"
        "  color: #555;"
        "}"
        "#joinRoomBtn:pressed { background-color: #059669; }"
        "#joinRoomBtn:disabled { background-color: #36383d; color: #555; }"
        "#joinRoomBtn:disabled { background-color: #36383d; color: #555; }"

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

void WebSocketPage::on_connectButton_clicked() {
    if (m_activeTabIndex < 0) return;

    auto &tab = m_tabs[m_activeTabIndex];

    if (tab.client && tab.client->isConnected()) {
        tab.client->disconnectFromServer();
        return;
    }

    if (tab.client) {
        tab.client->disconnectFromServer();
        tab.client->deleteLater();
    }

    QString url = ui->location->text().trimmed();
    if (url.isEmpty()) url = SettingsDialog::wsUrl();
    tab.url = url;

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
    config.autoJoinRoom = SettingsDialog::wsAutoJoinDefaultRoom();

    tab.client = new QtWebSocketClient(url, config, this);

    connect(tab.client, &QtWebSocketClient::connected, this, &WebSocketPage::onConnected);
    connect(tab.client, &QtWebSocketClient::disconnected, this, &WebSocketPage::onDisconnected);
    connect(tab.client, &QtWebSocketClient::messageReceived, this, &WebSocketPage::onMessageReceived);
    connect(tab.client, &QtWebSocketClient::errorOccurred, this, &WebSocketPage::onErrorOccurred);
    connect(tab.client, &QtWebSocketClient::reconnecting, this, &WebSocketPage::onReconnecting);
    tab.connecting = true;
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
    tab.client->connectToServer();
}

void WebSocketPage::onConnected() {
    int tabIndex = findTabIndexForClient(sender());
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }

    // 房间列表由 ConfigService (config_room 连接) 统一获取，通过信号同步到此处

    if (tabIndex == m_activeTabIndex) {
        ui->message->setEnabled(true);
        updateButtonStates(true);
        ui->statusLabel->setProperty("connected", true);
        updateStatusStyle();
    }
    emit tabsChanged();
    emit statusChanged(true);
}

void WebSocketPage::onDisconnected() {
    int tabIndex = findTabIndexForClient(sender());
    appendLog("已断开连接", "system", tabIndex);
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }
    if (tabIndex == m_activeTabIndex) {
        ui->message->setEnabled(true);
        updateButtonStates(false);
        ui->statusLabel->setProperty("connected", false);
        updateStatusStyle();
        m_roomComboBox->clear();
        m_roomComboBox->setEnabled(false);
    }
    emit tabsChanged();
    emit statusChanged(false);
}

void WebSocketPage::onMessageReceived(const QString &message) {
    int tabIndex = findTabIndexForClient(sender());

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        if (type == "joined") {
            if (tabIndex == m_activeTabIndex) {
                appendLog("已加入房间: " + obj["roomId"].toString(), "system", tabIndex);
            }
            return;
        }
        if (type == "message" && obj.contains("content")) {
            QString content = obj["content"].toString();
            appendLog(content, "info", tabIndex);
            emit messageReceived(content);
            return;
        }
        if (type == "room_list") {
            QJsonArray roomsArray = obj["rooms"].toArray();
            QStringList rooms;
            for (const auto &room : roomsArray) {
                rooms.append(room.toString());
            }
            if (tabIndex == m_activeTabIndex) {
                onRoomListReceived(rooms);
            }
            return;
        }
        if (obj.contains("msg")) {
            appendLog(obj["msg"].toString(), "system", tabIndex);
            return;
        }
    }

    appendLog(message, "system", tabIndex);
    emit messageReceived(message);
}

void WebSocketPage::onErrorOccurred(const QString &error) {
    int tabIndex = findTabIndexForClient(sender());
    appendLog("✗ " + error, "system", tabIndex);
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }
    if (tabIndex == m_activeTabIndex) {
        ui->message->setEnabled(true);
        updateButtonStates(false);
        ui->statusLabel->setProperty("connected", false);
        updateStatusStyle();
    }
}

void WebSocketPage::onReconnecting(int attempt, int maxAttempts) {
    int tabIndex = findTabIndexForClient(sender());
    appendLog(QString("⟳ 重连中... (%1/%2)").arg(attempt).arg(maxAttempts), "system", tabIndex);
}

void WebSocketPage::on_sendButton_clicked() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];

    if (!tab.client || !tab.client->isConnected()) {
        appendLog("未连接，无法发送", "system");
        return;
    }

    if (ui->tableView->isVisible() && tab.fileModel->rowCount() > 0) {
        for (int i = 0; i < tab.fileModel->rowCount(); i++) {
            auto *item = tab.fileModel->item(i, 0);
            if (item) {
                tab.client->sendImage(item->text());
                appendLog("发送图片: " + QFileInfo(item->text()).fileName(), "sent");
            }
        }
    } else {
        auto message = ui->message->toPlainText().trimmed();
        if (message.isEmpty()) return;
        tab.client->sendMessage(message);
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
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];

    for (const auto &file: files) {
        auto *item = new QStandardItem(file);
        item->setData(file, Qt::UserRole);
        tab.fileModel->appendRow(item);
    }

    ui->tableView->setModel(tab.fileModel);
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setShowGrid(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setDefaultSectionSize(32);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setItemDelegate(new TableItemDelegate(ui->tableView));
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
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (m_contextRow >= 0 && m_contextRow < tab.fileModel->rowCount()) {
        tab.fileModel->removeRow(m_contextRow);
        m_contextRow = -1;
    }
}

void WebSocketPage::onActionReplace() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (m_contextRow < 0 || m_contextRow >= tab.fileModel->rowCount()) return;

    auto file = QFileDialog::getOpenFileName(this, QStringLiteral("更换文件"),
                                             QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                             "所有文件 (*.*)");
    if (file.isEmpty()) return;

    auto *item = tab.fileModel->item(m_contextRow);
    if (item) {
        item->setText(file);
        item->setData(file, Qt::UserRole);
    }
    m_contextRow = -1;
}

int WebSocketPage::findTabIndexForClient(QObject *client) const {
    for (int i = 0; i < m_tabs.size(); i++) {
        if (m_tabs[i].client == client) return i;
    }
    return -1;
}

void WebSocketPage::appendLog(const QString &message, const QString &type, int tabIndex) {
    if (tabIndex < 0) tabIndex = m_activeTabIndex;
    if (tabIndex < 0 || tabIndex >= m_tabs.size()) return;

    ChatMessage msg;
    msg.text = message;
    msg.type = type;
    m_tabs[tabIndex].logMessages.append(msg);

    QString escaped = message;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");

    QRegularExpression urlRegex("(https?://[^\\s<>\"]+)");
    QRegularExpressionMatchIterator it = urlRegex.globalMatch(escaped);
    QStringList urls;
    while (it.hasNext()) { urls << it.next().captured(0); }
    for (const QString &url: urls) {
        escaped.replace(url, QString("<a href=\"%1\" class=\"link\">%1</a>").arg(url));
    }

    QString html;

    if (type == "sent") {
        html = QString(
            "<table width='100%' cellpadding='0' cellspacing='0' border='0'>"
            "<tr>"
            "<td width='*' align='right' valign='top' style='padding: 3px 8px 3px 50px;'>"
            "<table cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td style='background-color: #3b82f6; color: #ffffff; padding: 8px 12px; "
            "font-size: 9pt; font-family: Microsoft YaHei UI, Segoe UI, sans-serif; "
            "line-height: 1.45;'>%1</td></tr>"
            "</table></td>"
            "<td width='38' valign='top' style='padding: 3px 0;'>"
            "<table cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td width='32' height='32' style='background-color: #3b82f6; color: #ffffff; "
            "text-align: center; font-size: 12pt; font-weight: bold; "
            "font-family: Microsoft YaHei UI, Segoe UI, sans-serif;'>S</td></tr>"
            "</table></td>"
            "</tr></table>"
        ).arg(escaped);
    } else if (type == "info") {
        html = QString(
            "<table width='100%' cellpadding='0' cellspacing='0' border='0'>"
            "<tr>"
            "<td width='38' valign='top' style='padding: 3px 0;'>"
            "<table cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td width='32' height='32' style='background-color: #10b981; color: #ffffff; "
            "text-align: center; font-size: 12pt; font-weight: bold; "
            "font-family: Microsoft YaHei UI, Segoe UI, sans-serif;'>R</td></tr>"
            "</table></td>"
            "<td width='*' valign='top' style='padding: 3px 8px 3px 8px;'>"
            "<table cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td style='background-color: #2a2d32; color: #e2e8f0; padding: 8px 12px; "
            "font-size: 9pt; font-family: Microsoft YaHei UI, Segoe UI, sans-serif; "
            "line-height: 1.45;'>%2</td></tr>"
            "</table></td>"
            "</tr></table>"
        ).arg(escaped);
    } else {
        html = QString(
            "<table width='100%' cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td align='center' style='padding: 5px 0;'>"
            "<table cellpadding='0' cellspacing='0' border='0'>"
            "<tr><td style='background-color: #252830; color: #6b7280; padding: 3px 14px; "
            "font-size: 7pt; font-family: Microsoft YaHei UI, Segoe UI, sans-serif;'>%1</td></tr>"
            "</table></td></tr></table>"
        ).arg(escaped);
    }

    m_tabs[tabIndex].logEntries.append(html);

    if (tabIndex == m_activeTabIndex) {
        m_chatLog->appendMessage(message, type);
    }

    emit logAppended(tabIndex, html);
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
        m_roomComboBox->clear();
        m_roomComboBox->setEnabled(false);
        auto *joinBtn = findChild<QPushButton *>("joinRoomBtn");
        if (joinBtn) {
            joinBtn->setEnabled(false);
        }
    }
    updateSendButtonState();
}

void WebSocketPage::updateSendButtonState() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) {
        ui->sendButton->setEnabled(false);
        return;
    }
    auto &tab = m_tabs[m_activeTabIndex];
    bool connected = tab.client && tab.client->isConnected();
    bool hasMessage = !ui->message->toPlainText().trimmed().isEmpty();
    bool hasFiles = tab.fileModel && tab.fileModel->rowCount() > 0;
    bool canSend = connected && (hasMessage || hasFiles);
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

void WebSocketPage::requestRoomList() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (!tab.client || !tab.client->isConnected()) return;

    QJsonObject msg;
    msg["type"] = "room_list";
    msg["roomId"] = "config_room";
    tab.client->sendRawText(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void WebSocketPage::onRoomListReceived(const QStringList &rooms) {
    m_roomComboBox->clear();
    m_roomComboBox->addItems(rooms);
    m_roomComboBox->setEnabled(true);

    // 设置默认选中房间
    QString defaultRoom = SettingsDialog::wsDefaultRoomId();
    int index = m_roomComboBox->findText(defaultRoom);
    if (index >= 0) {
        m_roomComboBox->setCurrentIndex(index);
    }

    // 启用加入按钮
    auto *joinBtn = findChild<QPushButton *>("joinRoomBtn");
    if (joinBtn) {
        joinBtn->setEnabled(true);
    }
}

void WebSocketPage::onJoinRoomClicked() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (!tab.client || !tab.client->isConnected()) return;

    QString roomId = m_roomComboBox->currentText();
    if (roomId.isEmpty()) return;

    QJsonObject joinMsg;
    joinMsg["type"] = "join";
    joinMsg["roomId"] = roomId;
    tab.client->sendRawText(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));
}
