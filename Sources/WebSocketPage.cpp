#include "WebSocketPage.h"
#include "ui_WebSocketPage.h"
#include "QtWebSocketClient.h"
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
#include "TableItemDelegate.h"

WebSocketPage::WebSocketPage(QWidget *parent) : QWidget(parent),
                                                ui(new Ui::WebSocketPage),
                                                m_contextRow(-1) {
    ui->setupUi(this);
    applyApiFoxStyle();

    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &WebSocketPage::onContextMenuRequested);
    ui->log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->log, &QTextBrowser::customContextMenuRequested, this, [this](QPoint pos) {
        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
            "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
        );
        QAction clearAction("清空日志", this);
        connect(&clearAction, &QAction::triggered, this, [this]() {
            ui->log->clear();
            if (m_activeTabIndex >= 0) {
                m_tabs[m_activeTabIndex].logEntries.clear();
            }
        });
        menu.addAction(&clearAction);
        menu.exec(ui->log->viewport()->mapToGlobal(pos));
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

    addTab("ws://127.0.0.1:8200/websocket");
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
    state.url = url;
    state.fileModel = new QStandardItemModel(this);
    m_tabs.append(state);

    switchTab(m_tabs.size() - 1);
    emit tabsChanged();
}

void WebSocketPage::connectToCurrentTab() {
    on_connectButton_clicked();
}

QString WebSocketPage::tabLogHtml(int index) const {
    if (index < 0 || index >= m_tabs.size()) return {};
    QString html;
    for (const auto &entry : m_tabs[index].logEntries) {
        if (!html.isEmpty()) html += "<br>";
        html += entry;
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

    ui->log->clear();
    for (const auto &entry : tab.logEntries) {
        ui->log->append(entry);
    }

    ui->message->setText(tab.messageText);

    bool hasClient = tab.client && tab.client->isConnected();
    bool isConnecting = tab.connecting;
    ui->connectButton->setEnabled(!hasClient && !isConnecting);
    ui->disconnectButton->setEnabled(hasClient);
    ui->location->setEnabled(!hasClient && !isConnecting);
    ui->sendButton->setEnabled(hasClient);

    bool connected = hasClient;
    ui->statusLabel->setProperty("connected", connected);
    updateStatusStyle();

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
    ui->connectButton->setEnabled(!hasClient);
    ui->disconnectButton->setEnabled(hasClient);
    ui->location->setEnabled(!hasClient);
    updateSendButtonState();
}

void WebSocketPage::applyApiFoxStyle() {
    setStyleSheet(
        "* { font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif; }"
        "QWidget { background-color: #191a1c; color: #e0e0e0; }"

        "#connectButton {"
        "  background-color: #0ea5e9; color: #ffffff; border: none;"
        "  border-radius: 6px; padding: 6px 20px; font-size: 9pt; font-weight: bold;"
        "}"
        "#connectButton:hover { background-color: #38bdf8; }"
        "#connectButton:pressed { background-color: #0284c7; }"
        "#connectButton:disabled { background-color: #36383d; color: #555; }"
        "#disconnectButton {"
        "  background-color: #374151; color: #9ca3af; border: none;"
        "  border-radius: 6px; padding: 6px 20px; font-size: 9pt;"
        "}"
        "#disconnectButton:hover { background-color: #4b5563; color: #d1d5db; }"
        "#disconnectButton:pressed { background-color: #374151; }"
        "#disconnectButton:disabled { background-color: #36383d; color: #555; }"
        "#disconnectButton:enabled { background-color: #ef4444; color: #ffffff; }"
        "#disconnectButton:enabled:hover { background-color: #f87171; }"

        "QLineEdit {"
        "  background-color: #1f2023; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  padding: 8px 12px; font-size: 10pt; selection-background-color: #0ea5e9;"
        "}"
        "QLineEdit:focus { border: 1px solid #0ea5e9; }"
        "QLineEdit:disabled { background-color: #26282c; color: #555; border: 1px solid #26282c; }"

        "QTextEdit {"
        "  background-color: #1f2023; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  padding: 8px; font-size: 10pt; selection-background-color: #0ea5e9;"
        "}"
        "QTextEdit:focus { border: 1px solid #0ea5e9; }"

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

        "#requestBar { background-color: #191a1c; }"
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

    ui->log->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #1f2023; color: #cbd5e1;"
        "  border: 1px solid #36383d; border-radius: 6px;"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 9pt; padding: 8px 12px;"
        "}"
        "QTextBrowser:focus { border: 1px solid #0ea5e9; }"
    );

    ui->log->document()->setDefaultStyleSheet(
        ".timestamp { color: #334155; }"
        ".info { color: #000000; }"
        ".error { color: #f87171; }"
        ".success { color: #34d399; }"
        ".warning { color: #fbbf24; }"
        ".link { color: #38bdf8; text-decoration: underline; }"
        ".sent { color: #16a34a; }"
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
    if (tab.client) {
        tab.client->disconnectFromServer();
        tab.client->deleteLater();
    }

    QString url = ui->location->text().trimmed();
    if (url.isEmpty()) url = "ws://127.0.0.1:8200/websocket";
    tab.url = url;

    WebSocketConfig config;

    const QString chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString token;
    token.reserve(7);
    for (int i = 0; i < 7; i++) {
        token.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
    }
    config.jwtToken = token;

    config.pingIntervalMs = 30000;
    config.reconnectIntervalMs = 5000;
    config.maxReconnectAttempts = 500;

    tab.client = new QtWebSocketClient(url, config, this);

    connect(tab.client, &QtWebSocketClient::connected, this, &WebSocketPage::onConnected);
    connect(tab.client, &QtWebSocketClient::disconnected, this, &WebSocketPage::onDisconnected);
    connect(tab.client, &QtWebSocketClient::messageReceived, this, &WebSocketPage::onMessageReceived);
    connect(tab.client, &QtWebSocketClient::errorOccurred, this, &WebSocketPage::onErrorOccurred);
    connect(tab.client, &QtWebSocketClient::reconnecting, this, &WebSocketPage::onReconnecting);

    appendLog("连接中: " + url, "info");
    tab.connecting = true;
    ui->connectButton->setEnabled(false);
    tab.client->connectToServer();
}

void WebSocketPage::on_disconnectButton_clicked() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (tab.client) {
        tab.client->disconnectFromServer();
    }
}

void WebSocketPage::onConnected() {
    int tabIndex = findTabIndexForClient(sender());
    appendLog("已连接到服务器", "success", tabIndex);
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }
    if (tabIndex == m_activeTabIndex) {
        updateButtonStates(true);
        ui->statusLabel->setProperty("connected", true);
        updateStatusStyle();
    }
    emit tabsChanged();
    emit statusChanged(true);
}

void WebSocketPage::onDisconnected() {
    int tabIndex = findTabIndexForClient(sender());
    appendLog("已断开连接", "warning", tabIndex);
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }
    if (tabIndex == m_activeTabIndex) {
        updateButtonStates(false);
        ui->statusLabel->setProperty("connected", false);
        updateStatusStyle();
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
        if (type == "message" && obj.contains("content")) {
            appendLog("← " + obj["content"].toString(), "info", tabIndex);
            return;
        }
        if (obj.contains("msg")) {
            appendLog("← " + obj["msg"].toString(), "info", tabIndex);
            return;
        }
    }

    appendLog("← " + message, "info", tabIndex);
}

void WebSocketPage::onErrorOccurred(const QString &error) {
    int tabIndex = findTabIndexForClient(sender());
    appendLog("✗ " + error, "error", tabIndex);
    if (tabIndex >= 0 && tabIndex < m_tabs.size()) {
        m_tabs[tabIndex].connecting = false;
    }
    if (tabIndex == m_activeTabIndex) {
        ui->connectButton->setEnabled(true);
    }
}

void WebSocketPage::onReconnecting(int attempt, int maxAttempts) {
    int tabIndex = findTabIndexForClient(sender());
    appendLog(QString("⟳ 重连中... (%1/%2)").arg(attempt).arg(maxAttempts), "warning", tabIndex);
}

void WebSocketPage::on_sendButton_clicked() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];

    if (!tab.client || !tab.client->isConnected()) {
        appendLog("未连接，无法发送", "error");
        return;
    }

    if (ui->tableView->isVisible() && tab.fileModel->rowCount() > 0) {
        for (int i = 0; i < tab.fileModel->rowCount(); i++) {
            auto *item = tab.fileModel->item(i, 0);
            if (item) {
                tab.client->sendImage(item->text());
                appendLog("→ 发送图片: " + QFileInfo(item->text()).fileName(), "success");
            }
        }
    } else {
        auto message = ui->message->toPlainText().trimmed();
        if (message.isEmpty()) return;
        tab.client->sendMessage(message);
        appendLog("→ " + message, "sent");
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

    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");

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

    QString html = QString("<span class='timestamp'>%1</span> <span class='%2'>%3</span>")
            .arg(ts, type, escaped);

    m_tabs[tabIndex].logEntries.append(html);

    if (tabIndex == m_activeTabIndex) {
        ui->log->append(html);
        QTextCursor cursor = ui->log->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->log->setTextCursor(cursor);
    }

    emit logAppended(tabIndex, html);
}

void WebSocketPage::updateButtonStates(bool connected) {
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    ui->location->setEnabled(!connected);
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
