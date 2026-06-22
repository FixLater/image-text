#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DashboardPage.h"
#include "WebSocketPage.h"
#include "TranslationPage.h"
#include "FileServerPage.h"
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
#include <QMessageBox>
#include <QPainter>
#include <QIcon>
#include <QPropertyAnimation>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          m_activeTabIndex(-1) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_starBg = new StarBackground(ui->centralwidget);
    m_starBg->lower();
    m_starBg->resize(ui->centralwidget->size());

    setupSystemTray();

    connect(ui->minimizeBtn, &QPushButton::clicked, this, [this]() { showMinimized(); });
    connect(ui->maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            ui->maximizeBtn->setIcon(QIcon(":/icons/maximize.svg"));
        } else {
            showMaximized();
            ui->maximizeBtn->setIcon(QIcon(":/icons/restore.svg"));
        }
    });
    connect(ui->closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(ui->settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);

    ui->minimizeBtn->setFixedSize(36, 36);
    ui->maximizeBtn->setFixedSize(36, 36);
    ui->closeBtn->setFixedSize(36, 36);
    ui->settingsBtn->setFixedSize(36, 36);
    ui->tabAdd->setFixedSize(32, 24);

    ui->minimizeBtn->setIcon(QIcon(":/icons/minimize.svg"));
    ui->minimizeBtn->setIconSize(QSize(16, 16));
    ui->maximizeBtn->setIcon(QIcon(":/icons/maximize.svg"));
    ui->maximizeBtn->setIconSize(QSize(16, 16));
    ui->closeBtn->setIcon(QIcon(":/icons/close.svg"));
    ui->closeBtn->setIconSize(QSize(16, 16));
    ui->settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    ui->settingsBtn->setIconSize(QSize(16, 16));
    ui->backBtn->setIcon(QIcon(":/icons/back.svg"));
    ui->backBtn->setIconSize(QSize(14, 14));
    ui->homeBtn->setIcon(QIcon(":/icons/home.svg"));
    ui->homeBtn->setIconSize(QSize(14, 14));

    ui->breadcrumbLabel->setPixmap(QIcon(":/app_icon.jpg").pixmap(20, 20));
    ui->breadcrumbLabel->setContentsMargins(8, 2, 0, 0);
    ui->breadcrumbLabel->setAlignment(Qt::AlignCenter);

    connect(ui->backBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->homeBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->tabAdd, &QPushButton::clicked, this, &MainWindow::onTabAddClicked);

    ui->viewToggleBtn->setFixedSize(30, 30);
    ui->viewToggleBtn->setCursor(Qt::PointingHandCursor);
    ui->viewToggleBtn->setIcon(QIcon(":/icons/sidebar.svg"));
    ui->viewToggleBtn->setIconSize(QSize(21, 21));
    ui->viewToggleBtn->setToolTip("显示侧边栏");
    ui->viewToggleBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; margin-left: 8px; }"
        "QPushButton:hover { background-color: #36383d; }"
    );
    connect(ui->viewToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    applyTitleBarStyle();
    initPages();
    setupLeftSidebarIcons();

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
        m_trayIcon->setIcon(QIcon(":/app_icon.jpg"));

    m_trayIcon->setToolTip("lovely - WebSocket 客户端");

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
    );

    QAction *showAction = m_trayMenu->addAction("打开");
    connect(showAction, &QAction::triggered, this, [this]() {
        setWindowFlags(windowFlags() & ~Qt::Tool);
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
        setWindowFlags(windowFlags() & ~Qt::Tool);
        show();
        raise();
        activateWindow();
        if (m_flashTimer->isActive()) {
            m_flashTimer->stop();
    m_trayIcon->setIcon(QIcon(":/app_icon.jpg"));
        }
    }
}

void MainWindow::flashTrayIcon() {
    m_flashState = !m_flashState;
    if (m_flashState) {
            m_trayIcon->setIcon(QIcon(":/app_icon.jpg"));
    } else {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(239, 68, 68));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(2, 2, 12, 12);
        painter.end();
        m_trayIcon->setIcon(QIcon(pixmap));
    }
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
    int closeAction = SettingsDialog::closeAction();
    if (closeAction == 0) {
        if (!SettingsDialog::exitWithoutReminder()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("退出确认");
            msgBox.setText("确定要退出程序吗？");
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setStyleSheet(
                "QMessageBox { background-color: #191a1c; }"
                "QMessageBox QLabel { color: #e2e8f0; font-size: 9pt; }"
                "QPushButton { background-color: #374151; color: #e2e8f0; border: none;"
                "  border-radius: 4px; padding: 6px 20px; font-size: 9pt; }"
                "QPushButton:hover { background-color: #4b5563; }"
                "QPushButton:pressed { background-color: #374151; }"
            );
            auto ret = msgBox.exec();
            if (ret != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
        event->accept();
        return;
    }
    event->ignore();
    setWindowFlags(windowFlags() | Qt::Tool);
    hide();
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
    m_translationPage = new TranslationPage(this);
    m_fileServerPage = new FileServerPage(this);

    ui->stackedWidget->addWidget(m_dashboardPage);
    ui->stackedWidget->addWidget(m_websocketPage);
    ui->stackedWidget->addWidget(m_translationPage);
    ui->stackedWidget->addWidget(m_fileServerPage);
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
    connect(m_fileServerPage, &FileServerPage::statusChanged, this, [this](bool running) {
        m_dashboardPage->updateCardStatus("fileserver", running);
    });
    connect(m_dashboardPage, &DashboardPage::fileServerToggled, this, [this](bool start) {
        m_fileServerPage->onToggleServer();
    });

    if (SettingsDialog::fileServerAutoStart()) {
        m_fileServerPage->onToggleServer();
    }

    m_websocketPage->connectToCurrentTab();

    ui->backBtn->hide();
    ui->homeBtn->hide();
    ui->breadcrumbSeparator1->hide();
    ui->leftBarWidget->setMinimumWidth(0);
    ui->leftBarWidget->setMaximumWidth(0);
    ui->leftBtn1->hide();
    ui->leftBtn2->hide();
    ui->leftBtn3->hide();
}

void MainWindow::onModuleClicked(const QString &moduleName) {
    if (!m_sidebarVisible) {
        m_sidebarVisible = true;
        ui->viewToggleBtn->setIcon(QIcon(":/icons/dashboard.svg"));
        ui->viewToggleBtn->setToolTip("隐藏侧边栏");
        animateSidebar(true);
        ui->leftBtn1->show();
        ui->leftBtn2->show();
        ui->leftBtn3->show();
    }
    navigateTo(moduleName);
    updateSidebarSelection(moduleName);
}

void MainWindow::updateSidebarSelection(const QString &active) {
    auto applyStyle = [](QPushButton *btn, bool selected) {
        if (selected) {
            btn->setStyleSheet(
                "QPushButton { background-color: rgba(14, 165, 233, 0.15); border: none; border-radius: 6px; }"
                "QPushButton:hover { background-color: rgba(14, 165, 233, 0.25); }"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton { background: transparent; border: none; border-radius: 6px; }"
                "QPushButton:hover { background-color: #36383d; }"
            );
        }
    };
    applyStyle(ui->leftBtn1, active == "websocket");
    applyStyle(ui->leftBtn2, active == "translate");
    applyStyle(ui->leftBtn3, active == "fileserver");
}

void MainWindow::setupLeftSidebarIcons() {
    ui->leftBtn1->setIcon(QIcon(":/icons/websocket.svg"));
    ui->leftBtn1->setIconSize(QSize(18, 18));
    ui->leftBtn1->setToolTip("WebSocket");
    ui->leftBtn1->setFixedSize(28, 28);

    ui->leftBtn2->setIcon(QIcon(":/icons/translate.svg"));
    ui->leftBtn2->setIconSize(QSize(18, 18));
    ui->leftBtn2->setToolTip("翻译");
    ui->leftBtn2->setFixedSize(28, 28);

    ui->leftBtn3->setIcon(QIcon(":/icons/fileserver.svg"));
    ui->leftBtn3->setIconSize(QSize(18, 18));
    ui->leftBtn3->setToolTip("文件服务器");
    ui->leftBtn3->setFixedSize(28, 28);

    connect(ui->leftBtn1, &QPushButton::clicked, this, [this]() {
        if (ui->leftBtn1->styleSheet().contains("rgba(14, 165, 233, 0.15)")) {
            updateSidebarSelection("");
            navigateBack();
        } else {
            navigateTo("websocket");
            updateSidebarSelection("websocket");
        }
    });
    connect(ui->leftBtn2, &QPushButton::clicked, this, [this]() {
        if (ui->leftBtn2->styleSheet().contains("rgba(14, 165, 233, 0.15)")) {
            updateSidebarSelection("");
            navigateBack();
        } else {
            navigateTo("translate");
            updateSidebarSelection("translate");
        }
    });
    connect(ui->leftBtn3, &QPushButton::clicked, this, [this]() {
        if (ui->leftBtn3->styleSheet().contains("rgba(14, 165, 233, 0.15)")) {
            updateSidebarSelection("");
            navigateBack();
        } else {
            navigateTo("fileserver");
            updateSidebarSelection("fileserver");
        }
    });
}

int MainWindow::sidebarWidth() const {
    return ui->leftBarWidget->maximumWidth();
}

void MainWindow::setSidebarWidth(int width) {
    ui->leftBarWidget->setMinimumWidth(width);
    ui->leftBarWidget->setMaximumWidth(width);
}

void MainWindow::animateSidebar(bool show) {
    if (m_sidebarAnimation) {
        m_sidebarAnimation->stop();
        m_sidebarAnimation->deleteLater();
        m_sidebarAnimation = nullptr;
    }

    int startWidth = ui->leftBarWidget->maximumWidth();
    int endWidth = show ? 36 : 0;

    m_sidebarAnimation = new QPropertyAnimation(ui->leftBarWidget, "maximumWidth", this);
    m_sidebarAnimation->setDuration(200);
    m_sidebarAnimation->setStartValue(startWidth);
    m_sidebarAnimation->setEndValue(endWidth);
    m_sidebarAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_sidebarAnimation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        int w = value.toInt();
        ui->leftBarWidget->setMinimumWidth(w);
    });

    m_sidebarAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    connect(m_sidebarAnimation, &QPropertyAnimation::destroyed, this, [this]() {
        m_sidebarAnimation = nullptr;
    });
}

void MainWindow::toggleSidebar() {
    m_sidebarVisible = !m_sidebarVisible;

    if (m_sidebarVisible) {
        ui->viewToggleBtn->setIcon(QIcon(":/icons/dashboard.svg"));
        ui->viewToggleBtn->setToolTip("隐藏侧边栏");
        animateSidebar(true);
        ui->leftBtn1->show();
        ui->leftBtn2->show();
        ui->leftBtn3->show();
        updateSidebarSelection("");
    } else {
        ui->viewToggleBtn->setIcon(QIcon(":/icons/sidebar.svg"));
        ui->viewToggleBtn->setToolTip("显示侧边栏");
        animateSidebar(false);
        ui->leftBtn1->hide();
        ui->leftBtn2->hide();
        ui->leftBtn3->hide();
        updateSidebarSelection("");
        navigateBack();
    }
}

void MainWindow::navigateTo(const QString &moduleName) {
    if (moduleName == "websocket") {
        ui->stackedWidget->setCurrentWidget(m_websocketPage);
        updateBreadcrumb("仪表盘 › WebSocket");
        ui->tabBarWidget->show();
        rebuildTabBar();
    } else if (moduleName == "translate") {
        ui->stackedWidget->setCurrentWidget(m_translationPage);
        updateBreadcrumb("仪表盘 › 翻译");
        ui->tabBarWidget->hide();
    } else if (moduleName == "fileserver") {
        ui->stackedWidget->setCurrentWidget(m_fileServerPage);
        updateBreadcrumb("仪表盘 › 文件服务器");
        ui->tabBarWidget->hide();
    }
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
    Q_UNUSED(path);
    ui->breadcrumbLabel->setText("");
    ui->breadcrumbLabel->setPixmap(QIcon(":/app_icon.jpg").pixmap(20, 20));
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
        "#leftBarWidget { background-color: #26282c; }"
        "#tabBarWidget { background-color: #222326; border-radius: 0; }"

        "#minimizeBtn, #maximizeBtn, #closeBtn, #settingsBtn {"
        "  background: transparent; border: none; border-radius: 4px;"
        "}"
        "#minimizeBtn:hover, #maximizeBtn:hover, #settingsBtn:hover { background-color: #36383d; }"
        "#closeBtn:hover { background-color: #dc2626; }"

        "#leftBtn1, #leftBtn2 {"
        "  background: transparent; border: none; border-radius: 4px;"
        "}"
        "#leftBtn1:hover, #leftBtn2:hover { background-color: #36383d; }"

        "#backBtn, #homeBtn {"
        "  background: transparent; color: #94a3b8; border: none;"
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
