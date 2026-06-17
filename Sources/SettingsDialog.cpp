#include "SettingsDialog.h"
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QMouseEvent>
#include <QIcon>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent),
                                                  m_dragging(false) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(520, 480);
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *titleBar = new QWidget(this);
    titleBar->setObjectName("settingsTitleBar");
    titleBar->setFixedHeight(36);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 0, 0);
    titleLayout->setSpacing(0);

    auto *titleLabel = new QLabel("设置", titleBar);
    titleLabel->setStyleSheet("color: #94a3b8; font-size: 9pt; background: transparent; border: none;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto *closeBtn = new QPushButton(titleBar);
    closeBtn->setObjectName("settingsCloseBtn");
    closeBtn->setFixedSize(36, 36);
    closeBtn->setIcon(QIcon(":/icons/close.svg"));
    closeBtn->setIconSize(QSize(16, 16));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);

    mainLayout->addWidget(titleBar);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    buildTabs();
    mainLayout->addWidget(m_tabWidget, 1);

    auto *btnBar = new QWidget(this);
    btnBar->setObjectName("settingsBtnBar");
    btnBar->setFixedHeight(52);
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(16, 8, 16, 8);
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("取消", btnBar);
    cancelBtn->setObjectName("settingsCancelBtn");
    cancelBtn->setFixedSize(80, 30);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("保存", btnBar);
    saveBtn->setObjectName("settingsSaveBtn");
    saveBtn->setFixedSize(80, 30);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    btnLayout->addWidget(saveBtn);

    mainLayout->addWidget(btnBar);

    applyDialogStyle();
}

QSettings *SettingsDialog::settings() {
    static QSettings s("lovely", "Settings");
    return &s;
}

QList<ConfigModule> SettingsDialog::buildModules() {
    QList<ConfigModule> modules;

    ConfigModule ws;
    ws.name = "WebSocket";
    ws.icon = "⚡";
    ws.fields = {
        {"ws/defaultUrl",       "默认地址",       "WebSocket 服务器地址",                ConfigField::String, "ws://127.0.0.1:8200/websocket"},
        {"ws/pingIntervalMs",   "心跳间隔 (ms)","心跳包发送间隔（毫秒）", ConfigField::Int,    30000},
        {"ws/reconnectIntervalMs","重连间隔 (ms)","重连等待时间（毫秒）",  ConfigField::Int,    5000},
        {"ws/maxReconnectAttempts","最大重连次数","最大重连尝试次数",  ConfigField::Int,    500},
        {"ws/jwtTokenLength",   "令牌长度",   "随机令牌字符数",            ConfigField::Int,    7},
        {"ws/roomId",           "房间 ID",            "加入时发送的房间标识",            ConfigField::String, "log"},
    };
    modules.append(ws);

    return modules;
}

void SettingsDialog::buildTabs() {
    auto modules = buildModules();
    for (const auto &mod : modules) {
        auto *tab = createModuleTab(mod);
        m_tabWidget->addTab(tab, mod.icon + " " + mod.name);
    }
}

QWidget *SettingsDialog::createModuleTab(const ConfigModule &module) {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget();
    container->setStyleSheet("background: transparent;");
    auto *form = new QFormLayout(container);
    form->setContentsMargins(20, 16, 20, 16);
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    for (const auto &field : module.fields) {
        QVariant current = settings()->value(field.key, field.defaultValue);

        if (field.type == ConfigField::String) {
            auto *edit = new QLineEdit(current.toString());
            edit->setObjectName("settingsField");
            edit->setToolTip(field.tooltip);
            edit->setMinimumWidth(280);
            edit->setProperty("settingsKey", field.key);
            form->addRow(field.label + ":", edit);
        } else {
            auto *spin = new QSpinBox();
            spin->setObjectName("settingsField");
            spin->setToolTip(field.tooltip);
            spin->setRange(0, 999999);
            spin->setValue(current.toInt());
            spin->setProperty("settingsKey", field.key);
            form->addRow(field.label + ":", spin);
        }
    }

    scroll->setWidget(container);
    return scroll;
}

void SettingsDialog::onSave() {
    auto *s = settings();
    for (int t = 0; t < m_tabWidget->count(); ++t) {
        auto *tab = m_tabWidget->widget(t);
        if (!tab) continue;
        auto *scroll = qobject_cast<QScrollArea *>(tab);
        if (!scroll) continue;
        auto *container = scroll->widget();
        if (!container) continue;

        auto children = container->findChildren<QWidget *>();
        for (auto *w : children) {
            QString key = w->property("settingsKey").toString();
            if (key.isEmpty()) continue;

            if (auto *edit = qobject_cast<QLineEdit *>(w)) {
                s->setValue(key, edit->text());
            } else if (auto *spin = qobject_cast<QSpinBox *>(w)) {
                s->setValue(key, spin->value());
            }
        }
    }
    accept();
}

QString SettingsDialog::wsUrl() {
    return settings()->value("ws/defaultUrl", "ws://127.0.0.1:8200/websocket").toString();
}

int SettingsDialog::wsPingIntervalMs() {
    return settings()->value("ws/pingIntervalMs", 30000).toInt();
}

int SettingsDialog::wsReconnectIntervalMs() {
    return settings()->value("ws/reconnectIntervalMs", 5000).toInt();
}

int SettingsDialog::wsMaxReconnectAttempts() {
    return settings()->value("ws/maxReconnectAttempts", 500).toInt();
}

int SettingsDialog::wsJwtTokenLength() {
    return settings()->value("ws/jwtTokenLength", 7).toInt();
}

QString SettingsDialog::wsRoomId() {
    return settings()->value("ws/roomId", "log").toString();
}

void SettingsDialog::applyDialogStyle() {
    setStyleSheet(
        "QDialog, QWidget { background-color: #191a1c; color: #e0e0e0; }"

        "#settingsBtnBar { background-color: #191a1c; }"

        "QTabWidget::pane { border: none; background: #191a1c; top: -1px; }"
        "QTabBar::tab {"
        "  background: #222326; color: #6b7280;"
        "  padding: 8px 20px; margin-right: 1px;"
        "  border: none;"
        "  border-top-left-radius: 6px; border-top-right-radius: 6px;"
        "  font-size: 9pt;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #191a1c; color: #e2e8f0;"
        "  border-bottom: 2px solid #0ea5e9;"
        "}"
        "QTabBar::tab:hover:!selected { color: #94a3b8; }"

        "QFormLayout QLabel {"
        "  color: #94a3b8; font-size: 9pt;"
        "}"

        "#settingsField {"
        "  background-color: #26282c; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 9pt;"
        "}"
        "#settingsField:focus { border: 1px solid #0ea5e9; }"
        "#settingsField:hover { border-color: #475569; }"

        "#settingsSaveBtn {"
        "  background-color: #0ea5e9; color: #ffffff; border: none;"
        "  border-radius: 4px; font-size: 9pt; font-weight: bold;"
        "}"
        "#settingsSaveBtn:hover { background-color: #38bdf8; }"
        "#settingsSaveBtn:pressed { background-color: #0284c7; }"

        "#settingsCancelBtn {"
        "  background-color: #374151; color: #9ca3af; border: none;"
        "  border-radius: 4px; font-size: 9pt;"
        "}"
        "#settingsCancelBtn:hover { background-color: #4b5563; color: #d1d5db; }"

        "#settingsTitleBar { background-color: #26282c; }"
        "#settingsCloseBtn {"
        "  background: transparent; border: none;"
        "  border-radius: 4px;"
        "}"
        "#settingsCloseBtn:hover { background-color: #dc2626; }"

        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );
}

void SettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->position().y() < 36) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - pos();
    }
    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
    }
    QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
