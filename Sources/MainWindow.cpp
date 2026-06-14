#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DashboardPage.h"
#include "WebSocketPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          m_activeTabIndex(-1) {
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
    ui->tabAdd->setFixedSize(32, 28);

    connect(ui->backBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->homeBtn, &QPushButton::clicked, this, &MainWindow::navigateBack);
    connect(ui->tabAdd, &QPushButton::clicked, this, &MainWindow::onTabAddClicked);

    applyTitleBarStyle();
    initPages();

    ui->tabBarWidget->hide();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::initPages() {
    m_dashboardPage = new DashboardPage(this);
    m_websocketPage = new WebSocketPage(this);

    ui->stackedWidget->addWidget(m_dashboardPage);
    ui->stackedWidget->addWidget(m_websocketPage);
    ui->stackedWidget->setCurrentIndex(0);

    connect(m_dashboardPage, &DashboardPage::moduleClicked, this, &MainWindow::onModuleClicked);
    connect(m_websocketPage, &WebSocketPage::tabsChanged, this, &MainWindow::rebuildTabBar);

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
        updateBreadcrumb("Dashboard › WebSocket");
        ui->tabBarWidget->show();
        rebuildTabBar();
    }

    ui->backBtn->show();
    ui->homeBtn->show();
    ui->breadcrumbSeparator1->show();
}

void MainWindow::navigateBack() {
    ui->stackedWidget->setCurrentWidget(m_dashboardPage);
    updateBreadcrumb("Dashboard");

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
                "#tabWidget { background-color: #1a1b1e; border-radius: 6px 6px 0 0; }"
            );
            tabWidget->setContentsMargins(4, 4, 4, 0);
            textLabel->setStyleSheet("color: #e2e8f0; font-size: 9pt; background: transparent; border: none;");
            closeBtn->setStyleSheet(
                "QPushButton { color: #94a3b8; background: transparent; border: none; font-size: 10pt; border-radius: 3px; } QPushButton:hover { background-color: #36383d; color: #e2e8f0; }");
        } else {
            tabWidget->setStyleSheet(
                "#tabWidget { background-color: #222326; border-radius: 6px 6px 0 0; }"
            );
            tabWidget->setContentsMargins(0, 4, 0, 0);
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
            connect(&closeCurrent, &QAction::triggered, this, [this, i]() { onTabCloseClicked(i); });
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

        connect(closeBtn, &QPushButton::clicked, this, [this, i]() { onTabCloseClicked(i); });
        tabWidget->installEventFilter(this);

        layout->insertWidget(tabAddIndex + i, tabWidget);
    }

    ui->tabAdd->setStyleSheet(
        "QPushButton { background: transparent; color: #374151; border: none;"
        "  font-size: 14pt; font-weight: bold; padding: 0 8px; border-radius: 4px; }"
        "QPushButton:hover { color: #d1d5db; background-color: #1f2937; }"
    );
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
        "#tabBarWidget { background-color: #222326; }"

        "#backBtn, #homeBtn {"
        "  background: transparent; color: #94a3b8; border: none; font-size: 11pt;"
        "  border-radius: 4px; padding: 4px 8px;"
        "}"
        "#backBtn:hover, #homeBtn:hover { background-color: #36383d; color: #e2e8f0; }"

        "#breadcrumbSeparator1 {"
        "  color: #475569; font-size: 11pt; background: transparent; border: none;"
        "  padding: 0 4px;"
        "}"

        "#breadcrumbLabel {"
        "  color: #94a3b8; font-size: 9pt; background: transparent; border: none;"
        "}"

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
