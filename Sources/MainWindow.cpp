#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DashboardPage.h"
#include "ModuleInfo.h"
#include "ModuleRegistry.h"
#include "SettingsDialog.h"
#include "ConfigService.h"
#include "StarBackground.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QMenu>
#include <QMouseEvent>
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
                                          ui(new Ui::MainWindow) {
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

    ui->minimizeBtn->setIcon(QIcon(":/icons/minimize.svg"));
    ui->minimizeBtn->setIconSize(QSize(16, 16));
    ui->maximizeBtn->setIcon(QIcon(":/icons/maximize.svg"));
    ui->maximizeBtn->setIconSize(QSize(16, 16));
    ui->closeBtn->setIcon(QIcon(":/icons/close.svg"));
    ui->closeBtn->setIconSize(QSize(16, 16));
    ui->settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    ui->settingsBtn->setIconSize(QSize(16, 16));

    ui->onlineLabel->setStyleSheet(
        "color: #64748b; font-size: 8pt; font-weight: bold; background: transparent; border: none;");
    ui->onlineLabel->setText("offline");
    ui->onlineLabel->setToolTip("http://127.0.0.1:8200");

    ui->backBtn->setIcon(QIcon(":/icons/back.svg"));
    ui->backBtn->setIconSize(QSize(14, 14));
    ui->homeBtn->setIcon(QIcon(":/icons/home.svg"));
    ui->homeBtn->setIconSize(QSize(14, 14));

    ui->breadcrumbLabel->setPixmap(QIcon(":/app_icon.jpg").pixmap(20, 20));
    ui->breadcrumbLabel->setContentsMargins(8, 2, 0, 0);
    ui->breadcrumbLabel->setAlignment(Qt::AlignCenter);

    connect(ui->backBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->homeBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);

    ui->viewToggleBtn->setFixedSize(30, 30);
    ui->viewToggleBtn->setCursor(Qt::PointingHandCursor);
    ui->viewToggleBtn->setIcon(QIcon(":/icons/sidebar.svg"));
    ui->viewToggleBtn->setIconSize(QSize(21, 21));
    ui->viewToggleBtn->setToolTip(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe4\xbe\xa7\xe8\xbe\xb9\xe6\xa0\x8f"));
    ui->viewToggleBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; margin-left: 8px; }"
        "QPushButton:hover { background-color: #36383d; }");
    connect(ui->viewToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    applyTitleBarStyle();
    initPages();
    setupLeftSidebarIcons();

    ui->tabBarWidget->hide();
    ui->tabAdd->hide();
}

void MainWindow::onSettingsClicked() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (auto *ws = m_pages.value("websocket")) {
            QMetaObject::invokeMethod(ws, "refreshUrlFromSettings");
        }
        if (auto *fs = m_pages.value("fileserver")) {
            QMetaObject::invokeMethod(fs, "refreshFromSettings");
        }

        const QString chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        int tokenLen = SettingsDialog::wsJwtTokenLength();
        QString token;
        token.reserve(tokenLen);
        for (int i = 0; i < tokenLen; i++) {
            token.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
        }
        SettingsDialog::configService()->startWebSocketSync(SettingsDialog::wsUrl(), token);
        SettingsDialog::configService()->loadAll();
    }
}

void MainWindow::setupSystemTray() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/app_icon.jpg"));
    m_trayIcon->setToolTip("lovely");

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }");

    QAction *showAction = m_trayMenu->addAction(QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80"));
    connect(showAction, &QAction::triggered, this, [this]() {
        setWindowFlags(windowFlags() & ~Qt::Tool);
        show();
        raise();
        activateWindow();
    });

    m_trayMenu->addSeparator();

    QAction *quitAction = m_trayMenu->addAction(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba"));
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
    m_trayIcon->setToolTip(QString::fromUtf8("\xe6\x96\xb0\xe6\xb6\x88\xe6\x81\xaf: ") + message.left(100));
    if (!isActiveWindow()) {
        if (!m_flashTimer->isActive()) {
            m_flashTimer->start();
        }
        m_trayIcon->showMessage(QString::fromUtf8("\xe6\x96\xb0\xe6\xb6\x88\xe6\x81\xaf"), message.left(200), QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    int closeAction = SettingsDialog::closeAction();
    if (closeAction == 0) {
        if (!SettingsDialog::exitWithoutReminder()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba\xe7\xa1\xae\xe8\xae\xa4"));
            msgBox.setText(QString::fromUtf8("\xe7\xa1\xae\xe5\xae\x9a\xe8\xa6\x81\xe9\x80\x80\xe5\x87\xba\xe7\xa8\x8b\xe5\xba\x8f\xe5\x90\x97\xef\xbc\x9f"));
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setStyleSheet(
                "QMessageBox { background-color: #191a1c; }"
                "QMessageBox QLabel { color: #e2e8f0; font-size: 9pt; }"
                "QPushButton { background-color: #374151; color: #e2e8f0; border: none;"
                "  border-radius: 4px; padding: 6px 20px; font-size: 9pt; }"
                "QPushButton:hover { background-color: #4b5563; }"
                "QPushButton:pressed { background-color: #374151; }");
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
    ui->stackedWidget->addWidget(m_dashboardPage);

    const auto &modules = ModuleRegistry::modules();
    for (const auto &mod : modules) {
        QWidget *page = mod.createPage();
        page->setParent(this);
        m_pages[mod.name] = page;
        ui->stackedWidget->addWidget(page);
        mod.connectSignals(page, m_dashboardPage);
    }

    ui->stackedWidget->setCurrentIndex(0);
    ui->stackedWidget->setStyleSheet("QStackedWidget { background: transparent; }");
    ui->centerContentWidget->setStyleSheet("#centerContentWidget { background: transparent; }");
    ui->middleWidget->setStyleSheet("#middleWidget { background: transparent; }");

    connect(m_dashboardPage, &DashboardPage::moduleClicked, this, &MainWindow::onModuleClicked);

    if (auto *ws = m_pages.value("websocket")) {
        connect(ws, SIGNAL(messageReceived(QString)), this, SLOT(onNewMessage(QString)));
    }

    if (SettingsDialog::configService()) {
        connect(SettingsDialog::configService(), &ConfigService::serverStatusChanged, this, [this](bool available) {
            if (available) {
                ui->onlineLabel->setText("online");
                ui->onlineLabel->setStyleSheet(
                    "color: #34d399; font-size: 8pt; font-weight: bold; background: transparent; border: none;");
            } else {
                ui->onlineLabel->setText("offline");
                ui->onlineLabel->setStyleSheet(
                    "color: #64748b; font-size: 8pt; font-weight: bold; background: transparent; border: none;");
            }
        });
        ui->onlineLabel->setToolTip("http://127.0.0.1:8200");

        const QString chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        int tokenLen = SettingsDialog::wsJwtTokenLength();
        QString token;
        token.reserve(tokenLen);
        for (int i = 0; i < tokenLen; i++) {
            token.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
        }
        SettingsDialog::configService()->startWebSocketSync(SettingsDialog::wsUrl(), token);
        SettingsDialog::configService()->loadAll();
    }

    if (SettingsDialog::fileServerAutoStart()) {
        if (auto *fs = m_pages.value("fileserver")) {
            QMetaObject::invokeMethod(fs, "onToggleServer");
        }
    }

    ui->backBtn->hide();
    ui->homeBtn->hide();
    ui->breadcrumbSeparator1->hide();
    ui->leftBarWidget->setMinimumWidth(0);
    ui->leftBarWidget->setMaximumWidth(0);
}

void MainWindow::setupLeftSidebarIcons() {
    const auto &modules = ModuleRegistry::modules();
    QPushButton *btns[] = { ui->leftBtn1, ui->leftBtn2, ui->leftBtn3 };
    ui->leftBtn4->hide();

    for (int i = 0; i < modules.size() && i < 3; ++i) {
        const auto &mod = modules[i];
        auto *btn = btns[i];
        btn->setIcon(QIcon(mod.sidebarIcon));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(mod.sidebarToolTip);
        btn->setFixedSize(28, 28);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; }"
            "QPushButton:hover { background-color: #36383d; }");

        connect(btn, &QPushButton::clicked, this, [this, name = mod.name, btn]() {
            if (btn->styleSheet().contains("rgba(14, 165, 233, 0.15)")) {
                updateSidebarSelection("");
                navigateBack();
            } else {
                navigateTo(name);
                updateSidebarSelection(name);
            }
        });
        m_sidebarButtons.append(btn);
    }
}

void MainWindow::onModuleClicked(const QString &moduleName) {
    if (!m_sidebarVisible) {
        m_sidebarVisible = true;
        ui->viewToggleBtn->setIcon(QIcon(":/icons/dashboard.svg"));
        ui->viewToggleBtn->setToolTip(QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f\xe4\xbe\xa7\xe8\xbe\xb9\xe6\xa0\x8f"));
        animateSidebar(true);
        for (auto *btn : m_sidebarButtons) btn->show();
    }
    navigateTo(moduleName);
    updateSidebarSelection(moduleName);
}

void MainWindow::updateSidebarSelection(const QString &active) {
    const auto &modules = ModuleRegistry::modules();
    for (int i = 0; i < m_sidebarButtons.size() && i < modules.size(); ++i) {
        bool selected = (modules[i].name == active);
        if (selected) {
            m_sidebarButtons[i]->setStyleSheet(
                "QPushButton { background-color: rgba(14, 165, 233, 0.15); border: none; border-radius: 6px; }"
                "QPushButton:hover { background-color: rgba(14, 165, 233, 0.25); }");
        } else {
            m_sidebarButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; border: none; border-radius: 6px; }"
                "QPushButton:hover { background-color: #36383d; }");
        }
    }
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

    m_sidebarAnimation = new QPropertyAnimation(ui->leftBarWidget, "maximumWidth");
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
        ui->viewToggleBtn->setToolTip(QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f\xe4\xbe\xa7\xe8\xbe\xb9\xe6\xa0\x8f"));
        animateSidebar(true);
        for (auto *btn : m_sidebarButtons) btn->show();
        updateSidebarSelection("");
    } else {
        ui->viewToggleBtn->setIcon(QIcon(":/icons/sidebar.svg"));
        ui->viewToggleBtn->setToolTip(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe4\xbe\xa7\xe8\xbe\xb9\xe6\xa0\x8f"));
        animateSidebar(false);
        for (auto *btn : m_sidebarButtons) btn->hide();
        updateSidebarSelection("");
        navigateBack();
    }
}

void MainWindow::navigateTo(const QString &moduleName) {
    if (m_pages.contains(moduleName)) {
        ui->stackedWidget->setCurrentWidget(m_pages[moduleName]);

        const auto &modules = ModuleRegistry::modules();
        for (const auto &mod : modules) {
            if (mod.name == moduleName) {
                updateBreadcrumb(mod.breadcrumb);
                break;
            }
        }
    }
}

void MainWindow::navigateBack() {
    ui->stackedWidget->setCurrentWidget(m_dashboardPage);
    updateBreadcrumb(QString::fromUtf8("\xe4\xbb\xaa\xe8\xa1\xa8\xe7\x9b\x98"));
    ui->backBtn->hide();
    ui->homeBtn->hide();
    ui->breadcrumbSeparator1->hide();
}

void MainWindow::updateBreadcrumb(const QString &path) {
    Q_UNUSED(path);
    ui->breadcrumbLabel->setText("");
    ui->breadcrumbLabel->setPixmap(QIcon(":/app_icon.jpg").pixmap(20, 20));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
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
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
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
                if (cursor.x < borderSize) { *result = HTTOPLEFT; return true; }
                if (cursor.x > winRect.right - borderSize) { *result = HTTOPRIGHT; return true; }
                *result = HTTOP; return true;
            }
            if (cursor.y > winRect.bottom - borderSize) {
                if (cursor.x < borderSize) { *result = HTBOTTOMLEFT; return true; }
                if (cursor.x > winRect.right - borderSize) { *result = HTBOTTOMRIGHT; return true; }
                *result = HTBOTTOM; return true;
            }
            if (cursor.x < borderSize) { *result = HTLEFT; return true; }
            if (cursor.x > winRect.right - borderSize) { *result = HTRIGHT; return true; }
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
