#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DashboardPage.h"
#include "WebSocketPage.h"
#include "SettingsDialog.h"
#include "StarBackground.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QPainter>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          m_activeTabIndex(-1) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_starBg = new StarBackground(ui->centralwidget);
    m_starBg->lower();
    m_starBg->resize(ui->centralwidget->size());

    setupSystemTray();

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
    connect(ui->settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);

    ui->minimizeBtn->setFixedSize(36, 36);
    ui->maximizeBtn->setFixedSize(36, 36);
    ui->closeBtn->setFixedSize(36, 36);
    ui->settingsBtn->setFixedSize(36, 36);
    ui->tabAdd->setFixedSize(32, 24);

    connect(ui->backBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->homeBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->tabAdd, &QPushButton::clicked, this, &MainWindow::onTabAddClicked);

    applyTitleBarStyle();
    initPages();

    ui->tabBarWidget->hide();
}

void MainWindow::onSettingsClicked() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_websocketPage->refreshUrlFromSettings();
    }
}

void MainWindow::setupSystemTray() {
    m_trayIcon = new QSystemTrayIcon(this);

    QPixmap normalPixmap(16, 16);
    normalPixmap.fill(Qt::transparent);
    QPainter normalPainter(&normalPixmap);
    normalPainter.setRenderHint(QPainter::Antialiasing);
    normalPainter.setBrush(QColor(14, 165, 233));
    normalPainter.setPen(Qt::NoPen);
    normalPainter.drawEllipse(2, 2, 12, 12);
    normalPainter.end();
    m_trayIcon->setIcon(QIcon(normalPixmap));

    m_trayIcon->setToolTip("ImageText - WebSocket 客户端");

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
    );

    QAction *showAction = m_trayMenu->addAction("打开");
    connect(showAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });

    m_trayMenu->addSeparator();

    QAction *quitAction = m_trayMenu->addAction("退出");
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_trayIcon->hide();
        qApp->quit();
    });

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(500);
    connect(m_flashTimer, &QTimer::timeout, this, &MainWindow::flashTrayIcon);

    m_trayIcon->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        show();
        raise();
        activateWindow();
        if (m_flashTimer->isActive()) {
            m_flashTimer->stop();
            QPixmap normalPixmap(16, 16);
            normalPixmap.fill(Qt::transparent);
            QPainter painter(&normalPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(14, 165, 233));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(2, 2, 12, 12);
            painter.end();
            m_trayIcon->setIcon(QIcon(normalPixmap));
        }
    }
}

void MainWindow::flashTrayIcon() {
    m_flashState = !m_flashState;
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(m_flashState ? QColor(239, 68, 68) : QColor(14, 165, 233));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, 12, 12);
    painter.end();
    m_trayIcon->setIcon(QIcon(pixmap));
}

void MainWindow::onNewMessage(const QString &message) {
    m_lastMessage = message;
    m_trayIcon->setToolTip("新消息: " + message.left(100));
    if (!isActiveWindow()) {
        if (!m_flashTimer->isActive()) {
            m_flashTimer->start();
        }
        m_trayIcon->showMessage("新消息", message.left(200), QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
    m_trayIcon->showMessage("ImageText", "程序已最小化到托盘，右键托盘图标可退出。",
                           QSystemTrayIcon::Information, 2000);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_starBg) {
        m_starBg->resize(ui->centralwidget->size());
    }
}

void MainWindow::initPages() {
    m_dashboardPage = new DashboardPage(this);
    m_websocketPage = new WebSocketPage(this);

    ui->stackedWidget->addWidget(m_dashboardPage);
    ui->stackedWidget->addWidget(m_websocketPage);
    ui->stackedWidget->setCurrentIndex(0);

    connect(m_dashboardPage, &DashboardPage::moduleClicked, this, &MainWindow::onModuleClicked);
    connect(m_websocketPage, &WebSocketPage::tabsChanged, this, &MainWindow::rebuildTabBar);
    connect(m_websocketPage, &WebSocketPage::statusChanged, this, [this](bool connected) {
        m_dashboardPage->updateCardStatus("websocket", connected);
    });
    connect(m_websocketPage, &WebSocketPage::logAppended, this, [this](int tabIndex, const QString &html) {
        m_dashboardPage->appendCardLog("websocket", tabIndex, html);
    });
    connect(m_websocketPage, &WebSocketPage::messageReceived, this, &MainWindow::onNewMessage);
    connect(m_dashboardPage, &DashboardPage::cardPrevClicked, this, [this](const QString &name) {
        if (name == "websocket" && m_websocketPage) {
            int idx = m_websocketPage->activeTabIndex() - 1;
            if (idx >= 0) {
                m_websocketPage->switchTab(idx);
                updateDashboardTabInfo();
            }
        }
    });
    connect(m_dashboardPage, &DashboardPage::cardNextClicked, this, [this](const QString &name) {
        if (name == "websocket" && m_websocketPage) {
            int idx = m_websocketPage->activeTabIndex() + 1;
            if (idx < m_websocketPage->tabCount()) {
                m_websocketPage->switchTab(idx);
                updateDashboardTabInfo();
            }
        }
    });

    m_websocketPage->connectToCurrentTab();

    ui->backBtn->hide();
    ui->homeBtn->hide();
    ui->breadcrumbSeparator1->hide();
}

void MainWindow::onModuleClicked(const QString &moduleName) {
    if (moduleName == "websocket") {
        navigateTo("websocket");
    }
}

void MainWindow::navigateTo(const QString &moduleName) {
    if (moduleName == "websocket") {
        ui->stackedWidget->setCurrentWidget(m_websocketPage);
        updateBreadcrumb("仪表盘 › WebSocket");
        ui->tabBarWidget->show();
        rebuildTabBar();
    }

    ui->backBtn->show();
    ui->homeBtn->show();
    ui->breadcrumbSeparator1->show();
}

void MainWindow::navigateBack() {
    ui->stackedWidget->setCurrentWidget(m_dashboardPage);
    updateBreadcrumb("仪表盘");

    ui->backBtn->hide();
    ui->homeBtn->hide();
    ui->breadcrumbSeparator1->hide();
    ui->tabBarWidget->hide();

    // Clear tab bar
    auto *layout = qobject_cast<QBoxLayout *>(ui->tabBarWidget->layout());
    if (layout) {
        for (int i = layout->count() - 1; i >= 0; --i) {
            auto *item = layout->itemAt(i);
            if (!item || !item->widget()) continue;
            if (item->widget()->objectName() == "tabWidget") {
                auto *w = layout->takeAt(i)->widget();
                w->deleteLater();
            }
        }
    }
}

void MainWindow::updateBreadcrumb(const QString &path) {
    ui->breadcrumbLabel->setText(path);
}

void MainWindow::showTabBar(bool show) {
    ui->tabAdd->setVisible(show);
}

void MainWindow::updateDashboardTabInfo() {
    if (!m_websocketPage) return;
    int count = m_websocketPage->tabCount();
    int active = m_websocketPage->activeTabIndex();
    if (active < 0 && count > 0) active = 0;
    m_dashboardPage->updateCardTabInfo("websocket", active, count);
    QString logHtml = m_websocketPage->tabLogHtml(active);
    m_dashboardPage->setCardLogFromTab("websocket", logHtml);
}

void MainWindow::onTabAddClicked() {
    if (!m_websocketPage) return;
    m_websocketPage->addTab();
}

void MainWindow::onTabClicked(int index) {
    if (!m_websocketPage) return;
    m_websocketPage->switchTab(index);
    m_activeTabIndex = index;
}

void MainWindow::onTabCloseClicked(int index) {
    if (!m_websocketPage) return;
    m_websocketPage->closeTab(index);
    m_activeTabIndex = m_websocketPage->activeTabIndex();
}

void MainWindow::rebuildTabBar() {
    auto *layout = qobject_cast<QBoxLayout *>(ui->tabBarWidget->layout());
    if (!layout) return;

    // Remove existing tab widgets
    for (int i = layout->count() - 1; i >= 0; --i) {
        auto *item = layout->itemAt(i);
        if (!item || !item->widget()) continue;
        if (item->widget()->objectName() == "tabWidget") {
            auto *w = layout->takeAt(i)->widget();
            w->deleteLater();
        }
    }

    if (!m_websocketPage || ui->stackedWidget->currentWidget() != m_websocketPage) return;

    int count = m_websocketPage->tabCount();
    int active = m_websocketPage->activeTabIndex();
    if (active < 0 && count > 0) active = 0;

    // Find the position of tabAdd in the layout
    int tabAddIndex = -1;
    for (int i = 0; i < layout->count(); i++) {
        if (layout->itemAt(i)->widget() == ui->tabAdd) {
            tabAddIndex = i;
            break;
        }
    }
    if (tabAddIndex == -1) return;

    // Insert tabs before tabAdd
    for (int i = 0; i < count; i++) {
        auto *tabWidget = new QWidget(ui->tabBarWidget);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setCursor(Qt::PointingHandCursor);
        tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        tabWidget->setMaximumWidth(200);

        auto *tabLayout = new QHBoxLayout(tabWidget);
        tabLayout->setContentsMargins(0, 4, 6, 4);
        tabLayout->setSpacing(6);

        auto *iconLabel = new QLabel("⚡", tabWidget);
        iconLabel->setStyleSheet("font-size: 10pt; background: transparent; border: none;");

        auto *textLabel = new QLabel(m_websocketPage->tabTitle(i), tabWidget);
        textLabel->setStyleSheet("font-size: 9pt; background: transparent; border: none;");
        textLabel->setMaximumWidth(120);

        auto *closeBtn = new QPushButton("×", tabWidget);
        closeBtn->setFixedSize(18, 18);
        closeBtn->setObjectName("tabCloseBtn");
        closeBtn->setCursor(Qt::PointingHandCursor);

        tabLayout->addWidget(iconLabel);
        tabLayout->addWidget(textLabel, 1);
        tabLayout->addWidget(closeBtn);

        bool isActive = (i == active);
        if (isActive) {
            tabWidget->setStyleSheet(
                "#tabWidget { background-color: #1a1b1e; border-radius: 0; }"
            );
            tabWidget->setContentsMargins(4, 4, 4, 0);
            textLabel->setStyleSheet("color: #e2e8f0; font-size: 9pt; background: transparent; border: none;");
            closeBtn->setStyleSheet(
                "QPushButton { color: #94a3b8; background: transparent; border: none; font-size: 10pt; border-radius: 3px; padding: 1px 0 3px 0; } QPushButton:hover { background-color: #36383d; color: #e2e8f0; }");
        } else {
            tabWidget->setStyleSheet(
                "#tabWidget { background-color: #222326; border-radius: 0; }"
            );
            tabWidget->setContentsMargins(0, 4, 0, 0);
            textLabel->setStyleSheet("color: #6b7280; font-size: 9pt; background: transparent; border: none;");
            closeBtn->setStyleSheet("QPushButton { color: #555; background: transparent; border: none; font-size: 10pt; border-radius: 3px; padding: 1px 0 3px 0; } QPushButton:hover { background-color: #36383d; color: #e2e8f0; }");
        }

        connect(tabWidget, &QWidget::customContextMenuRequested, this, [this, i, tabWidget](QPoint pos) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
                "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
                "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
            );
            QAction closeCurrent("关闭当前标签", this);
            connect(&closeCurrent, &QAction::triggered, this, [this, i, tabWidget]() {
                if (m_websocketPage && m_websocketPage->tabCount() <= 1) {
                    QToolTip::showText(tabWidget->mapToGlobal(QPoint(tabWidget->width() / 2, 0)),
                                       "最少保留一个标签", tabWidget);
                    return;
                }
                onTabCloseClicked(i);
            });
            menu.addAction(&closeCurrent);

            if (m_websocketPage->tabCount() > 1) {
                QAction closeOthers("关闭其他标签", this);
                connect(&closeOthers, &QAction::triggered, this, [this, i]() {
                    for (int j = m_websocketPage->tabCount() - 1; j >= 0; j--) {
                        if (j != i) m_websocketPage->closeTab(j);
                    }
                    m_websocketPage->switchTab(qMin(i, m_websocketPage->tabCount() - 1));
                });
                menu.addAction(&closeOthers);
            }
            menu.exec(tabWidget->mapToGlobal(pos));
        });

        connect(closeBtn, &QPushButton::clicked, this, [this, i, closeBtn]() {
            if (m_websocketPage && m_websocketPage->tabCount() <= 1) {
                QToolTip::showText(closeBtn->mapToGlobal(QPoint(closeBtn->width() / 2, 0)),
                                   "最少保留一个标签", closeBtn);
                return;
            }
            onTabCloseClicked(i);
        });
        tabWidget->installEventFilter(this);

        layout->insertWidget(tabAddIndex + i, tabWidget);
    }

    ui->tabAdd->setStyleSheet(
        "QPushButton { background: transparent; color: #374151; border: none;"
        "  font-size: 14pt; font-weight: bold; padding: 0 8px; border-radius: 4px; }"
        "QPushButton:hover { color: #d1d5db; background-color: #1f2937; }"
    );

    updateDashboardTabInfo();
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

                auto *layout = qobject_cast<QBoxLayout *>(ui->tabBarWidget->layout());
                if (layout) {
                    int tabIndex = 0;
                    for (int j = 0; j < layout->count(); j++) {
                        auto *item = layout->itemAt(j);
                        if (!item || !item->widget()) continue;
                        if (item->widget()->objectName() != "tabWidget") continue;
                        if (item->widget() == tabWidget) {
                            onTabClicked(tabIndex);
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

bool MainWindow::isInTitleBarArea(const QPoint &pos) const {
    return pos.y() < (ui->titleBarWidget->y() + ui->titleBarWidget->height());
}

void MainWindow::applyTitleBarStyle() {
    setStyleSheet(
        "* { font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif; }"
        "QMainWindow { background-color: #191a1c; }"
        "QWidget { color: #e0e0e0; }"

        "#titleBarWidget { background-color: #26282c; }"
        "#tabBarWidget { background-color: #222326; border-radius: 0; }"

        "#backBtn, #homeBtn {"
        "  background: transparent; color: #94a3b8; border: none; font-size: 11pt;"
        "  border-radius: 4px; padding: 3px 4px 5px 8px;"
        "}"
        "#backBtn:hover, #homeBtn:hover { background-color: #36383d; color: #e2e8f0; }"

        "#breadcrumbSeparator1 {"
        "  color: #475569; font-size: 11pt; background: transparent; border: none;"
        "  padding: 3px 4px 5px 8px;"
        "}"

        "#breadcrumbLabel {"
        "  color: #94a3b8; font-size: 9pt; background: transparent; border: none;"
        "}"

        "#minimizeBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 2px 4px 6px 4px;"
        "}"
        "#maximizeBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 0px 4px 8px 4px;"
        "}"

        "#minimizeBtn:hover, #maximizeBtn:hover { background-color: #36383d; color: #d1d5db; }"
        "#settingsBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 2px 4px 6px 4px;"
        "}"
        "#settingsBtn:hover { background-color: #36383d; color: #0ea5e9; }"
        "#closeBtn {"
        "  background: transparent; color: #6b7280; border: none; font-size: 12pt;"
        "  border-radius: 4px; padding: 2px 4px 6px 4px;"
        "}"
        "#closeBtn:hover { background-color: #dc2626; color: #ffffff; }"

        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }"
        "QScrollBar::handle:horizontal { background-color: #36383d; border-radius: 4px; min-width: 20px; }"
        "QScrollBar::handle:horizontal:hover { background-color: #475569; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    );
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
