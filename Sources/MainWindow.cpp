#include "MainWindow.h"
#include "ui_MainWindow.h"
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
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif
#include "TableItemDelegate.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          m_contextRow(-1) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);

    connect(ui->minimizeBtn, &QPushButton::clicked, this, [this]() { showMinimized(); });
    connect(ui->maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            ui->maximizeBtn->setText("□");
        } else {
            showMaximized();
            ui->maximizeBtn->setText("❐");
        }
    });
    connect(ui->closeBtn, &QPushButton::clicked, this, &QWidget::close);

    ui->minimizeBtn->setFixedSize(36, 36);
    ui->maximizeBtn->setFixedSize(36, 36);
    ui->closeBtn->setFixedSize(36, 36);

    applyApiFoxStyle();

    ui->tabAdd->setFixedSize(32, 28);
    connect(ui->tabAdd, &QPushButton::clicked, this, [this]() { addNewTab(); });

    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &MainWindow::onContextMenuRequested);
    ui->message->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->message->setAcceptDrops(false);
    this->setAcceptDrops(true);

    connect(ui->message, &QTextEdit::textChanged, this, &MainWindow::updateSendButtonState);

    connect(ui->tabMessage, &QPushButton::clicked, this, [this]() {
        if (m_activeTabIndex < 0) return;
        ui->message->show();
        ui->tableView->hide();
        ui->selectFile->hide();
        ui->tabMessage->setStyleSheet(
            "QPushButton { background: transparent; color: #0ea5e9; border: none;"
            "  border-bottom: 2px solid #0ea5e9; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
        );
        ui->tabFiles->setStyleSheet(
            "QPushButton { background: transparent; color: #475569; border: none;"
            "  border-bottom: 2px solid transparent; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
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
            "  border-bottom: 2px solid #0ea5e9; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
        );
        ui->tabMessage->setStyleSheet(
            "QPushButton { background: transparent; color: #475569; border: none;"
            "  border-bottom: 2px solid transparent; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
            "QPushButton:hover { color: #94a3b8; }"
        );
        updateSendButtonState();
    });

    ui->tabMessage->setStyleSheet(
        "QPushButton { background: transparent; color: #0ea5e9; border: none;"
        "  border-bottom: 2px solid #0ea5e9; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
    );
    ui->tabFiles->setStyleSheet(
        "QPushButton { background: transparent; color: #475569; border: none;"
        "  border-bottom: 2px solid transparent; padding: 6px 16px; font-size: 9pt; font-weight: bold; }"
        "QPushButton:hover { color: #94a3b8; }"
    );
    ui->tableView->hide();
    ui->selectFile->hide();

    addNewTab("ws://127.0.0.1:8200/websocket");
}

MainWindow::~MainWindow() {
    for (const auto &tab: m_tabs) {
        if (tab.client) {
            tab.client->disconnectFromServer();
            tab.client->deleteLater();
        }
    }
    delete ui;
}

void MainWindow::addNewTab(const QString &url) {
    TabState state;
    state.url = url;
    state.fileModel = new QStandardItemModel(this);
    m_tabs.append(state);

    rebuildTabBar();
    switchToTab(m_tabs.size() - 1);
}

void MainWindow::switchToTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    if (m_activeTabIndex >= 0 && m_activeTabIndex < m_tabs.size()) {
        auto &oldTab = m_tabs[m_activeTabIndex];
        oldTab.url = ui->location->text();
        oldTab.messageText = ui->message->toPlainText();
        oldTab.showFiles = ui->tableView->isVisible();
        oldTab.logHtml = ui->log->toHtml();
    }

    m_activeTabIndex = index;
    auto &tab = m_tabs[index];

    ui->location->setText(tab.url);

    ui->log->clear();
    if (!tab.logHtml.isEmpty()) {
        ui->log->setHtml(tab.logHtml);
    }

    ui->message->setText(tab.messageText);

    bool hasClient = tab.client && tab.client->isConnected();
    ui->connectButton->setEnabled(!hasClient);
    ui->disconnectButton->setEnabled(hasClient);
    ui->location->setEnabled(!hasClient);
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

    rebuildTabBar();
}

void MainWindow::rebuildTabBar() {
    auto *layout = qobject_cast<QBoxLayout *>(ui->titleBarWidget->layout());
    if (!layout) return;

    while (layout->count() > 0) {
        auto *item = layout->takeAt(0);
        if (auto *w = item->widget()) {
            if (w != ui->minimizeBtn && w != ui->maximizeBtn && w != ui->closeBtn && w != ui->tabAdd) {
                w->deleteLater();
            }
        }
        delete item;
    }

    for (int i = 0; i < m_tabs.size(); i++) {
        auto *tabWidget = new QWidget(ui->titleBarWidget);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setCursor(Qt::PointingHandCursor);
        tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);

        auto *tabLayout = new QHBoxLayout(tabWidget);
        tabLayout->setContentsMargins(12, 4, 6, 4);
        tabLayout->setSpacing(6);

        auto *iconLabel = new QLabel("⚡", tabWidget);
        iconLabel->setStyleSheet("font-size: 10pt; background: transparent; border: none;");

        auto *textLabel = new QLabel(tabWidget);
        QString display = m_tabs[i].url;
        display.replace("ws://", "").replace("wss://", "");
        if (display.length() > 22) display = display.left(19) + "...";
        textLabel->setText(display);
        textLabel->setStyleSheet("font-size: 9pt; background: transparent; border: none;");
        textLabel->setMaximumWidth(160);

        auto *closeBtn = new QPushButton("×", tabWidget);
        closeBtn->setFixedSize(18, 18);
        closeBtn->setObjectName("tabCloseBtn");
        closeBtn->setCursor(Qt::PointingHandCursor);

        tabLayout->addWidget(iconLabel);
        tabLayout->addWidget(textLabel, 1);
        tabLayout->addWidget(closeBtn);

        bool active = (i == m_activeTabIndex);
        if (active) {
            tabWidget->setStyleSheet(
                "#tabWidget { background-color: #191a1c; border-radius: 6px 6px 0 0; }"
            );
            textLabel->setStyleSheet("color: #e2e8f0; font-size: 9pt; background: transparent; border: none;");
            closeBtn->setStyleSheet(
                "QPushButton { color: #94a3b8; background: transparent; border: none; font-size: 10pt; border-radius: 3px; } QPushButton:hover { background-color: #36383d; color: #e2e8f0; }");
        } else {
            tabWidget->setStyleSheet(
                "#tabWidget { background-color: #26282c; border-radius: 6px 6px 0 0; }"
                "#tabWidget:hover { background-color: #2a2c30; }"
            );
            textLabel->setStyleSheet("color: #6b7280; font-size: 9pt; background: transparent; border: none;");
            closeBtn->setStyleSheet("QPushButton { color: #555; background: transparent; border: none; font-size: 10pt; border-radius: 3px; } QPushButton:hover { background-color: #36383d; color: #e2e8f0; }");
        }

        connect(tabWidget, &QWidget::customContextMenuRequested, this, [this, i, tabWidget](QPoint pos) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
                "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
                "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
            );
            QAction closeCurrent("关闭当前标签", this);
            connect(&closeCurrent, &QAction::triggered, this, [this, i]() { removeTab(i); });
            menu.addAction(&closeCurrent);

            if (m_tabs.size() > 1) {
                QAction closeOthers("关闭其他标签", this);
                connect(&closeOthers, &QAction::triggered, this, [this, i]() {
                    for (int j = m_tabs.size() - 1; j >= 0; j--) {
                        if (j != i) removeTab(j);
                    }
                    switchToTab(qMin(i, m_tabs.size() - 1));
                });
                menu.addAction(&closeOthers);
            }
            menu.exec(tabWidget->mapToGlobal(pos));
        });

        connect(closeBtn, &QPushButton::clicked, this, [this, i]() { removeTab(i); });

        tabWidget->installEventFilter(this);

        layout->addWidget(tabWidget);
    }

    layout->addWidget(ui->tabAdd);
    layout->addStretch(1);
    layout->addWidget(ui->minimizeBtn);
    layout->addWidget(ui->maximizeBtn);
    layout->addWidget(ui->closeBtn);

    ui->tabAdd->setStyleSheet(
        "QPushButton { background: transparent; color: #374151; border: none;"
        "  font-size: 14pt; font-weight: bold; padding: 0 8px; border-radius: 4px; }"
        "QPushButton:hover { color: #d1d5db; background-color: #1f2937; }"
    );
}

void MainWindow::removeTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    auto &tab = m_tabs[index];
    if (tab.client) {
        tab.client->disconnectFromServer();
        tab.client->deleteLater();
    }

    m_tabs.removeAt(index);

    if (m_tabs.isEmpty()) {
        addNewTab();
        return;
    }

    if (m_activeTabIndex >= m_tabs.size()) {
        m_activeTabIndex = m_tabs.size() - 1;
    } else if (m_activeTabIndex > index) {
        m_activeTabIndex--;
    } else if (m_activeTabIndex == index) {
        m_activeTabIndex = qMin(index, m_tabs.size() - 1);
    }

    switchToTab(m_activeTabIndex);
}

void MainWindow::applyApiFoxStyle() {
    setStyleSheet(
        "* { font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif; }"
        "QMainWindow { background-color: #191a1c; }"
        "QWidget { color: #e0e0e0; }"

        "#titleBarWidget { background-color: #26282c; }"

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
        "  border-bottom: 2px solid transparent; padding: 6px 16px;"
        "  font-size: 9pt; font-weight: bold; border-radius: 0;"
        "}"
        "#tabMessage:hover, #tabFiles:hover { color: #94a3b8; }"

        "#logTabBar { background-color: #26282c; }"
        "#logLabel { color: #94a3b8; font-size: 9pt; font-weight: bold; }"

        "#clearButton {"
        "  background-color: transparent; color: #64748b;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  padding: 3px 10px; font-size: 8pt;"
        "}"
        "#clearButton:hover { color: #e2e8f0; border-color: #64748b; }"

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

        "#minimizeBtn, #maximizeBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 4px;"
        "}"
        "#minimizeBtn:hover, #maximizeBtn:hover { background-color: #36383d; color: #d1d5db; }"
        "#closeBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 4px;"
        "}"
        "#closeBtn:hover { background-color: #dc2626; color: #ffffff; }"

        "#requestBar { background-color: #191a1c; }"
        "#statusLabel { font-size: 9pt; }"

        "QSplitter::handle { background-color: #36383d; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
        "QSplitter::handle:hover { background-color: #0ea5e9; }"

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
        "  background-color: #1f2023; color: #cbd5e1; border: none;"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 9pt; padding: 8px 12px;"
        "}"
    );

    ui->log->document()->setDefaultStyleSheet(
        ".timestamp { color: #334155; }"
        ".info { color: #cbd5e1; }"
        ".error { color: #f87171; }"
        ".success { color: #34d399; }"
        ".warning { color: #fbbf24; }"
        ".link { color: #38bdf8; text-decoration: underline; }"
    );
}

void MainWindow::updateStatusStyle() {
    bool connected = ui->statusLabel->property("connected").toBool();
    if (connected) {
        ui->statusLabel->setText("  ● Connected  ");
        ui->statusLabel->setStyleSheet("color: #34d399; font-size: 9pt; font-weight: bold;");
    } else {
        ui->statusLabel->setText("  ● Disconnected  ");
        ui->statusLabel->setStyleSheet("color: #64748b; font-size: 9pt;");
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            LONG lp = static_cast<LONG>(msg->lParam);
            int xPos = GET_X_LPARAM(lp);
            int yPos = GET_Y_LPARAM(lp);

            POINT cursor = {xPos, yPos};
            ScreenToClient(msg->hwnd, &cursor);

            RECT winRect;
            GetClientRect(msg->hwnd, &winRect);

            const int borderSize = 6;
            if (cursor.y < borderSize) {
                if (cursor.x < borderSize) {
                    *result = HTTOPLEFT;
                    return true;
                }
                if (cursor.x > winRect.right - borderSize) {
                    *result = HTTOPRIGHT;
                    return true;
                }
                *result = HTTOP;
                return true;
            }
            if (cursor.y > winRect.bottom - borderSize) {
                if (cursor.x < borderSize) {
                    *result = HTBOTTOMLEFT;
                    return true;
                }
                if (cursor.x > winRect.right - borderSize) {
                    *result = HTBOTTOMRIGHT;
                    return true;
                }
                *result = HTBOTTOM;
                return true;
            }
            if (cursor.x < borderSize) {
                *result = HTLEFT;
                return true;
            }
            if (cursor.x > winRect.right - borderSize) {
                *result = HTRIGHT;
                return true;
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::isInTitleBarArea(const QPoint &pos) const {
    return pos.y() < (ui->titleBarWidget->y() + ui->titleBarWidget->height());
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && isInTitleBarArea(event->pos())) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - pos();
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_isDragging = false;
    QMainWindow::mouseReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            auto *tabWidget = qobject_cast<QWidget *>(obj);
            if (tabWidget && tabWidget->objectName() == "tabWidget") {
                auto *clickedChild = tabWidget->childAt(mouseEvent->pos());
                if (clickedChild && clickedChild->objectName() == "tabCloseBtn") {
                    return false;
                }

                auto *layout = qobject_cast<QBoxLayout *>(ui->titleBarWidget->layout());
                if (layout) {
                    int tabIndex = 0;
                    for (int j = 0; j < layout->count(); j++) {
                        auto *item = layout->itemAt(j);
                        if (!item || !item->widget()) continue;
                        if (item->widget()->objectName() != "tabWidget") continue;
                        if (item->widget() == tabWidget) {
                            switchToTab(tabIndex);
                            return false;
                        }
                        tabIndex++;
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::on_connectButton_clicked() {
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
    config.jwtToken = "puppeteer";
    config.pingIntervalMs = 30000;
    config.reconnectIntervalMs = 5000;
    config.maxReconnectAttempts = 500;

    tab.client = new QtWebSocketClient(url, config, this);

    connect(tab.client, &QtWebSocketClient::connected, this, &MainWindow::onConnected);
    connect(tab.client, &QtWebSocketClient::disconnected, this, &MainWindow::onDisconnected);
    connect(tab.client, &QtWebSocketClient::messageReceived, this, &MainWindow::onMessageReceived);
    connect(tab.client, &QtWebSocketClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(tab.client, &QtWebSocketClient::reconnecting, this, &MainWindow::onReconnecting);

    appendLog("连接中: " + url, "info");
    tab.client->connectToServer();
}

void MainWindow::on_disconnectButton_clicked() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (tab.client) {
        tab.client->disconnectFromServer();
    }
}

void MainWindow::onConnected() {
    appendLog("已连接到服务器", "success");
    updateButtonStates(true);
    ui->statusLabel->setProperty("connected", true);
    updateStatusStyle();
    rebuildTabBar();
}

void MainWindow::onDisconnected() {
    appendLog("已断开连接", "warning");
    updateButtonStates(false);
    ui->statusLabel->setProperty("connected", false);
    updateStatusStyle();
    rebuildTabBar();
}

void MainWindow::onMessageReceived(const QString &message) {
    appendLog("← " + message, "info");
}

void MainWindow::onErrorOccurred(const QString &error) {
    appendLog("✗ " + error, "error");
}

void MainWindow::onReconnecting(int attempt, int maxAttempts) {
    appendLog(QString("⟳ 重连中... (%1/%2)").arg(attempt).arg(maxAttempts), "warning");
}

void MainWindow::on_sendButton_clicked() {
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
        appendLog("→ " + message, "info");
        ui->message->clear();
    }
}

void MainWindow::on_clearButton_clicked() {
    ui->log->clear();
    if (m_activeTabIndex >= 0) {
        m_tabs[m_activeTabIndex].logHtml.clear();
    }
}

void MainWindow::on_selectFile_clicked() {
    const auto files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                     QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                                     "所有文件 (*.*)");
    if (!files.isEmpty()) {
        addFiles(files);
    }
}

void MainWindow::addFiles(const QStringList &files) {
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

void MainWindow::onContextMenuRequested(QPoint pos) {
    QModelIndex index = ui->tableView->indexAt(pos);
    if (!index.isValid()) return;
    m_contextRow = index.row();

    QMenu contextMenu(this);
    QAction actionDelete("删除", this);
    connect(&actionDelete, &QAction::triggered, this, &MainWindow::onActionDelete);
    contextMenu.addAction(&actionDelete);

    QAction actionReplace("更换", this);
    connect(&actionReplace, &QAction::triggered, this, &MainWindow::onActionReplace);
    contextMenu.addAction(&actionReplace);

    contextMenu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

void MainWindow::onActionDelete() {
    if (m_activeTabIndex < 0) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (m_contextRow >= 0 && m_contextRow < tab.fileModel->rowCount()) {
        tab.fileModel->removeRow(m_contextRow);
        m_contextRow = -1;
    }
}

void MainWindow::onActionReplace() {
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

void MainWindow::appendLog(const QString &message, const QString &type) {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;

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

    ui->log->append(html);
    m_tabs[m_activeTabIndex].logHtml = ui->log->toHtml();

    QTextCursor cursor = ui->log->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->log->setTextCursor(cursor);
}

void MainWindow::updateButtonStates(bool connected) {
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    ui->location->setEnabled(!connected);
    updateSendButtonState();
}

void MainWindow::updateSendButtonState() {
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

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList files;
    for (const QUrl &url: event->mimeData()->urls()) {
        files.append(url.toLocalFile());
    }
    addFiles(files);
}
