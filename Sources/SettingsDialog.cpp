#include "SettingsDialog.h"
#include "ConfigService.h"
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QMouseEvent>
#include <QIcon>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QTimer>

static ConfigService *s_configService = nullptr;

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent),
                                                  m_dragging(false) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(560, 480);
    setModal(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_Hover, false);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    auto *borderFrame = new QFrame(this);
    borderFrame->setObjectName("settingsBorderFrame");
    auto *frameLayout = new QVBoxLayout(borderFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    auto *titleBar = new QWidget(borderFrame);
    titleBar->setObjectName("settingsTitleBar");
    titleBar->setFixedHeight(36);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 0, 0);
    titleLayout->setSpacing(0);

    auto *titleLabel = new QLabel("设置", titleBar);
    titleLabel->setStyleSheet("color: #94a3b8; font-size: 9pt; background: transparent; border: none;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    m_serverStatusLabel = new QLabel("● 连接中", titleBar);
    m_serverStatusLabel->setStyleSheet("color: #f59e0b; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
    m_serverStatusLabel->setToolTip("http://127.0.0.1:8200");
    titleLayout->addWidget(m_serverStatusLabel);

    // 连接中动态省略号
    m_connectingTimer = new QTimer(this);
    connect(m_connectingTimer, &QTimer::timeout, this, [this]() {
        m_connectingDots = (m_connectingDots + 1) % 4;
        m_serverStatusLabel->setText(QString("● 连接中%1").arg(QString(m_connectingDots, '.')));
    });
    m_connectingTimer->start(500);

    auto *closeBtn = new QPushButton(titleBar);
    closeBtn->setObjectName("settingsCloseBtn");
    closeBtn->setFixedSize(36, 36);
    closeBtn->setIcon(QIcon(":/icons/close.svg"));
    closeBtn->setIconSize(QSize(16, 16));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);

    frameLayout->addWidget(titleBar);

    auto *hSeparator = new QFrame(borderFrame);
    hSeparator->setFrameShape(QFrame::HLine);
    hSeparator->setStyleSheet("color: #36383d; max-height: 1px;");
    frameLayout->addWidget(hSeparator);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_navList = new QListWidget(borderFrame);
    m_navList->setObjectName("settingsNavList");
    m_navList->setFixedWidth(140);
    m_navList->setSpacing(0);
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setFocusPolicy(Qt::NoFocus);
    contentLayout->addWidget(m_navList);

    m_stack = new QStackedWidget(borderFrame);
    m_stack->setObjectName("settingsStack");
    contentLayout->addWidget(m_stack, 1);

    frameLayout->addLayout(contentLayout, 1);

    buildNav();

    auto *btnBar = new QWidget(borderFrame);
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

    frameLayout->addWidget(btnBar);

    mainLayout->addWidget(borderFrame);

    applyDialogStyle();

    m_navList->setCurrentRow(0);

    if (configService()) {
        connect(configService(), &ConfigService::allConfigsLoaded, this, &SettingsDialog::onLoadFromServer);
        connect(configService(), &ConfigService::serverStatusChanged, this, [this](const bool available) {
            if (m_connectingTimer) m_connectingTimer->stop();
            if (available) {
                m_serverStatusLabel->setText("● 已连接");
                m_serverStatusLabel->setStyleSheet("color: #34d399; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
            } else {
                m_serverStatusLabel->setText("● 连接失败");
                m_serverStatusLabel->setStyleSheet("color: #ef4444; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
            }
        });
        connect(configService(), &ConfigService::reconnecting, this, [this](int attempt, int maxAttempts) {
            if (m_connectingTimer) m_connectingTimer->stop();
            m_serverStatusLabel->setText(QString("● 正在重连... (%1/%2)").arg(attempt).arg(maxAttempts));
            m_serverStatusLabel->setStyleSheet("color: #f59e0b; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
        });
        connect(configService(), &ConfigService::reconnectCountdown, this, [this](int secondsRemaining, int attempt, int maxAttempts) {
            if (m_connectingTimer) m_connectingTimer->stop();
            m_serverStatusLabel->setText(QString("● %1秒后重连 (%2/%3)").arg(secondsRemaining).arg(attempt).arg(maxAttempts));
            m_serverStatusLabel->setStyleSheet("color: #ef4444; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
        });
        connect(configService(), &ConfigService::roomListReceived, this, &SettingsDialog::onRoomListReceived);

        // 立即检查当前连接状态并更新 UI
        if (configService()->isServerAvailable()) {
            if (m_connectingTimer) m_connectingTimer->stop();
            m_serverStatusLabel->setText("● 已连接");
            m_serverStatusLabel->setStyleSheet("color: #34d399; font-size: 8pt; background: transparent; border: none; margin-right: 8px;");
        }

        configService()->loadAll();
    }
}

QSettings *SettingsDialog::settings() {
    static QSettings s(QCoreApplication::applicationDirPath() + "/lovely.ini", QSettings::IniFormat);
    return &s;
}

ConfigService *SettingsDialog::configService() {
    if (!s_configService) {
        s_configService = new ConfigService("http://127.0.0.1:8200", qApp);
    }
    return s_configService;
}

QList<ConfigModule> SettingsDialog::buildModules() {
    QList<ConfigModule> modules;

    ConfigModule general;
    general.name = "常规";
    general.icon = "⚙";
    general.fields = {
        {"general/exitWithoutReminder", "退出不再提醒", "关闭窗口时不再弹出确认提示", ConfigField::Bool, false},
        {"general/closeAction",         "关闭按钮行为", "点击关闭按钮时的操作",        ConfigField::Choice, 1, {"直接退出", "最小化到托盘"}},
    };
    modules.append(general);

    ConfigModule ws;
    ws.name = "WebSocket";
    ws.icon = "⚡";
    ws.fields = {
        {"ws/defaultUrl",       "默认地址",       "WebSocket 服务器地址",                ConfigField::String, "ws://127.0.0.1:8200/websocket"},
        {"ws/pingIntervalMs",   "心跳间隔 (ms)","心跳包发送间隔（毫秒）", ConfigField::Int,    30000},
        {"ws/reconnectIntervalMs","重连间隔 (ms)","重连等待时间（毫秒）",  ConfigField::Int,    5000},
        {"ws/maxReconnectAttempts","最大重连次数","最大重连尝试次数",  ConfigField::Int,    500},
        {"ws/jwtTokenLength",   "令牌长度",   "随机令牌字符数",            ConfigField::Int,    7},
        {"ws/autoJoinDefaultRoom", "默认加入房间", "连接成功后自动加入默认房间",       ConfigField::Bool,   true},
        {"ws/defaultRoomId",    "默认房间",           "下拉框默认选中的房间 ID",         ConfigField::DynamicChoice, ""},
    };
    modules.append(ws);

    ConfigModule translate;
    translate.name = "翻译";
    translate.icon = "🌐";
    translate.fields = {
        {"translate/sourceLang", "默认源语言", "翻译的源语言", ConfigField::Choice, 0, {"自动检测", "中文", "英文", "日文", "韩文", "法文", "德文", "西班牙文"}},
        {"translate/targetLang", "默认目标语言", "翻译的目标语言", ConfigField::Choice, 1, {"自动检测", "中文", "英文", "日文", "韩文", "法文", "德文", "西班牙文"}},
    };
    modules.append(translate);

    ConfigModule fileServer;
    fileServer.name = "文件服务器";
    fileServer.icon = "📁";
    fileServer.fields = {
        {"fileserver/port",           "端口号",   "HTTP 文件服务器监听端口", ConfigField::Int, 8080},
        {"fileserver/allowLan",        "允许局域网访问", "允许局域网内其他设备访问", ConfigField::Bool, true},
        {"fileserver/autoStart",      "默认启动HTTP服务器", "程序启动时自动启动文件服务器", ConfigField::Bool, false},
    };
    modules.append(fileServer);

    return modules;
}

void SettingsDialog::buildNav() {
    auto modules = buildModules();
    for (const auto &mod : modules) {
        auto *page = createModuleTab(mod);
        m_stack->addWidget(page);

        auto *item = new QListWidgetItem(mod.icon + "  " + mod.name);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(140, 40));
        m_navList->addItem(item);
    }
    connect(m_navList, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
}

QWidget *SettingsDialog::createModuleTab(const ConfigModule &module) {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget();
    container->setStyleSheet("background: transparent;");
    auto *form = new QFormLayout(container);
    form->setContentsMargins(16, 16, 20, 16);
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    for (const auto &field : module.fields) {
        QVariant current = settings()->value(field.key, field.defaultValue);

        if (field.type == ConfigField::String) {
            auto *edit = new QLineEdit(current.toString());
            edit->setObjectName("settingsField");
            edit->setMinimumWidth(280);
            edit->setProperty("settingsKey", field.key);
            form->addRow(field.label + ":", edit);
        } else if (field.type == ConfigField::Int) {
            auto *spin = new QSpinBox();
            spin->setObjectName("settingsField");
            spin->setRange(0, 999999);
            spin->setValue(current.toInt());
            spin->setProperty("settingsKey", field.key);
            form->addRow(field.label + ":", spin);
        } else if (field.type == ConfigField::Bool) {
            auto *check = new QCheckBox();
            check->setObjectName("settingsCheck");
            check->setChecked(current.toBool());
            check->setProperty("settingsKey", field.key);
            form->addRow(field.label + ":", check);
        } else if (field.type == ConfigField::Choice) {
            auto *groupWidget = new QWidget();
            groupWidget->setObjectName("settingsRadioGroup");
            groupWidget->setProperty("settingsKey", field.key);
            auto *radioLayout = new QVBoxLayout(groupWidget);
            radioLayout->setContentsMargins(0, 0, 0, 0);
            radioLayout->setSpacing(8);

            auto *btnGroup = new QButtonGroup(groupWidget);
            btnGroup->setExclusive(true);
            int currentIdx = current.toInt();

            for (int i = 0; i < field.choices.size(); ++i) {
                auto *radio = new QRadioButton(field.choices[i]);
                radio->setObjectName("settingsRadio");
                radio->setChecked(i == currentIdx);
                btnGroup->addButton(radio, i);
                radioLayout->addWidget(radio);
            }
            form->addRow(field.label + ":", groupWidget);
        } else if (field.type == ConfigField::DynamicChoice) {
            auto *combo = new QComboBox();
            combo->setObjectName("settingsField");
            combo->setMinimumWidth(280);
            combo->setProperty("settingsKey", field.key);
            combo->addItem(current.toString());
            combo->setStyleSheet(
                "QComboBox {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252830, stop:1 #1e2128);"
                "  color: #e2e8f0;"
                "  border: 1px solid #3a3f4b;"
                "  border-radius: 8px;"
                "  padding: 8px 14px;"
                "  font-size: 9pt;"
                "  min-width: 200px;"
                "}"
                "QComboBox:hover {"
                "  border-color: #4a9eff;"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a3040, stop:1 #232830);"
                "}"
                "QComboBox::drop-down {"
                "  border: none;"
                "  width: 36px;"
                "  background: transparent;"
                "}"
                "QComboBox::down-arrow {"
                "  image: url(:/icons/arrow_down.svg);"
                "  width: 12px; height: 12px;"
                "}"
                "QComboBox::down-arrow:on {"
                "  image: url(:/icons/arrow_up.svg);"
                "}"
            );
            combo->view()->setStyleSheet(
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
            form->addRow(field.label + ":", combo);

            // 保存 ws/defaultRoomId 的下拉框引用
            if (field.key == "ws/defaultRoomId") {
                m_defaultRoomCombo = combo;
            }
        }
    }

    scroll->setWidget(container);
    return scroll;
}

void SettingsDialog::onLoadFromServer(const QMap<QString, QString> &configs) {
    if (configs.isEmpty()) return;

    auto *s = settings();
    for (int t = 0; t < m_stack->count(); ++t) {
        auto *tab = m_stack->widget(t);
        if (!tab) continue;
        auto *scroll = qobject_cast<QScrollArea *>(tab);
        if (!scroll) continue;
        auto *container = scroll->widget();
        if (!container) continue;

        auto children = container->findChildren<QWidget *>();
        for (auto *w : children) {
            QString key = w->property("settingsKey").toString();
            if (key.isEmpty()) continue;
            if (!configs.contains(key)) continue;

            QString val = configs.value(key);
            if (auto *edit = qobject_cast<QLineEdit *>(w)) {
                edit->setText(val);
                s->setValue(key, val);
            } else if (auto *spin = qobject_cast<QSpinBox *>(w)) {
                spin->setValue(val.toInt());
                s->setValue(key, val.toInt());
            } else if (auto *check = qobject_cast<QCheckBox *>(w)) {
                check->setChecked(val == "true");
                s->setValue(key, val == "true");
            } else if (auto *combo = qobject_cast<QComboBox *>(w)) {
                int idx = combo->findText(val);
                if (idx >= 0) {
                    combo->setCurrentIndex(idx);
                } else {
                    combo->addItem(val);
                    combo->setCurrentText(val);
                }
                s->setValue(key, val);
            } else if (w->objectName() == "settingsRadioGroup") {
                auto *btnGroup = w->findChild<QButtonGroup *>();
                if (btnGroup) {
                    int idx = val.toInt();
                    if (auto *btn = btnGroup->button(idx)) btn->setChecked(true);
                    s->setValue(key, idx);
                }
            }
        }
    }
    s->sync();
}

void SettingsDialog::onSave() {
    auto *s = settings();
    QMap<QString, QString> serverConfigs;

    for (int t = 0; t < m_stack->count(); ++t) {
        auto *tab = m_stack->widget(t);
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
                serverConfigs[key] = edit->text();
            } else if (auto *spin = qobject_cast<QSpinBox *>(w)) {
                s->setValue(key, spin->value());
                serverConfigs[key] = QString::number(spin->value());
            } else if (auto *check = qobject_cast<QCheckBox *>(w)) {
                s->setValue(key, check->isChecked());
                serverConfigs[key] = check->isChecked() ? "true" : "false";
            } else if (auto *combo = qobject_cast<QComboBox *>(w)) {
                s->setValue(key, combo->currentText());
                serverConfigs[key] = combo->currentText();
            } else if (w->objectName() == "settingsRadioGroup") {
                auto *btnGroup = w->findChild<QButtonGroup *>();
                if (btnGroup) {
                    int id = btnGroup->checkedId();
                    s->setValue(key, id);
                    serverConfigs[key] = QString::number(id);
                }
            }
        }
    }
    s->sync();

    if (configService() && configService()->isServerAvailable()) {
        configService()->saveAll(serverConfigs);
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

bool SettingsDialog::wsAutoJoinDefaultRoom() {
    return settings()->value("ws/autoJoinDefaultRoom", true).toBool();
}

QString SettingsDialog::wsDefaultRoomId() {
    return settings()->value("ws/defaultRoomId", "").toString();
}

bool SettingsDialog::exitWithoutReminder() {
    return settings()->value("general/exitWithoutReminder", false).toBool();
}

int SettingsDialog::closeAction() {
    return settings()->value("general/closeAction", 1).toInt();
}

QString SettingsDialog::translateSourceLang() {
    int idx = settings()->value("translate/sourceLang", 0).toInt();
    QStringList langs = {"auto", "ZH", "EN", "JA", "KO", "FR", "DE", "ES"};
    return (idx >= 0 && idx < langs.size()) ? langs[idx] : "auto";
}

QString SettingsDialog::translateTargetLang() {
    int idx = settings()->value("translate/targetLang", 1).toInt();
    QStringList langs = {"auto", "ZH", "EN", "JA", "KO", "FR", "DE", "ES"};
    return (idx >= 0 && idx < langs.size()) ? langs[idx] : "EN";
}

int SettingsDialog::fileServerPort() {
    return settings()->value("fileserver/port", 8080).toInt();
}

bool SettingsDialog::fileServerAllowLan() {
    return settings()->value("fileserver/allowLan", true).toBool();
}

bool SettingsDialog::fileServerAutoStart() {
    return settings()->value("fileserver/autoStart", false).toBool();
}

void SettingsDialog::applyDialogStyle() {
    setStyleSheet(
        "QDialog { background-color: transparent; }"
        "#settingsBorderFrame { background-color: #191a1c; border: 1px solid #2d2e2f; border-radius: 12px; }"

        "#settingsTitleBar {"
        "  background-color: #191a1c;"
        "  border-top-left-radius: 11px; border-top-right-radius: 11px;"
        "}"
        "#settingsCloseBtn {"
        "  background: transparent; border: none;"
        "  border-radius: 0px; border-top-right-radius: 11px;"
        "}"
        "#settingsCloseBtn:hover { background-color: #dc2626; border-radius: 0px; border-top-right-radius: 11px; }"
        "#settingsBtnBar {"
        "  background-color: #191a1c;"
        "  border-bottom-left-radius: 11px; border-bottom-right-radius: 11px;"
        "}"

        "#settingsNavList {"
        "  background-color: #191a1c; border: none; outline: none;"
        "  padding: 0px;"
        "  border-top-left-radius: 11px; border-bottom-left-radius: 11px;"
        "}"
        "#settingsNavList::item {"
        "  color: #94a3b8; padding: 8px 12px 8px 16px; border: none; border-radius: 0px;"
        "  font-size: 9pt; margin: 0px; outline: none;"
        "}"
        "#settingsNavList::item:selected {"
        "  background-color: #26282c; color: #e2e8f0;"
        "}"
        "#settingsNavList::item:hover:!selected {"
        "  background-color: rgba(38, 40, 44, 128); color: #e2e8f0;"
        "}"
        "#settingsNavList QScrollBar { background: transparent; width: 0; }"

        "#settingsStack {"
        "  background: #191a1c; border: none;"
        "  border-top-right-radius: 11px; border-bottom-right-radius: 11px;"
        "}"

        "QFormLayout QLabel {"
        "  color: #94a3b8; font-size: 9pt;"
        "}"

        "QLineEdit#settingsField {"
        "  background-color: #26282c; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 9pt;"
        "}"
        "QLineEdit#settingsField:focus { border: 1px solid #0ea5e9; }"
        "QSpinBox#settingsField {"
        "  background-color: #26282c; color: #e2e8f0;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 9pt;"
        "}"
        "QSpinBox#settingsField:focus { border: 1px solid #0ea5e9; }"

        "#settingsCheck {"
        "  spacing: 8px;"
        "}"
        "#settingsCheck::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid #36383d; border-radius: 3px;"
        "  background-color: #191a1c;"
        "}"
        "#settingsCheck::indicator:checked {"
        "  background-color: #0ea5e9; border-color: #0ea5e9;"
        "  image: url(:/icons/check_white.svg);"
        "}"
        "#settingsCheck::indicator:hover { border-color: #36383d; }"

        "#settingsRadio {"
        "  spacing: 8px; color: #e2e8f0; font-size: 9pt;"
        "}"
        "#settingsRadio::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid #36383d; border-radius: 8px;"
        "  background-color: #191a1c;"
        "}"
        "#settingsRadio::indicator:checked {"
        "  background-color: #0ea5e9; border-color: #0ea5e9;"
        "  image: url(:/icons/check_white.svg);"
        "}"
        "#settingsRadio::indicator:hover { border-color: #36383d; }"

        "QComboBox {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252830, stop:1 #1e2128);"
        "  color: #e2e8f0;"
        "  border: 1px solid #3a3f4b;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  font-size: 9pt;"
        "  min-width: 200px;"
        "}"
        "QComboBox:hover {"
        "  border-color: #4a9eff;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a3040, stop:1 #232830);"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 36px;"
        "  background: transparent;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(:/icons/arrow_down.svg);"
        "  width: 12px; height: 12px;"
        "}"
        "QComboBox::down-arrow:on {"
        "  image: url(:/icons/arrow_up.svg);"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #252830;"
        "  color: #e2e8f0;"
        "  border: 1px solid #3a3f4b;"
        "  border-radius: 6px;"
        "  outline: none;"
        "  padding: 2px;"
        "  show-decoration-selected: none;"
        "}"
        "QComboBox::item {"
        "  padding: 4px 10px;"
        "  border: none;"
        "  min-height: 18px;"
        "}"
        "QComboBox::item:selected {"
        "  background-color: transparent;"
        "  color: #e2e8f0;"
        "}"
        "QComboBox::item:hover {"
        "  background-color: #1e3a5f;"
        "  color: #e2e8f0;"
        "}"

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

void SettingsDialog::onRoomListReceived(const QStringList &rooms) {
    if (!m_defaultRoomCombo) return;

    QString savedValue = settings()->value("ws/defaultRoomId", "").toString();
    QString current = m_defaultRoomCombo->currentText();
    m_defaultRoomCombo->clear();
    m_defaultRoomCombo->addItems(rooms);

    // 优先用设置中保存的值，其次用当前选中值
    QString toSelect = savedValue.isEmpty() ? current : savedValue;
    int index = m_defaultRoomCombo->findText(toSelect);
    if (index >= 0) {
        m_defaultRoomCombo->setCurrentIndex(index);
    }

    QFile lf(QCoreApplication::applicationDirPath() + "/ws_debug.log");
    if (lf.open(QIODevice::Append | QIODevice::Text))
        lf.write(QString("%1 [Settings] rooms:%2 saved:%3 current:%4 select:%5 idx:%6\n")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
            .arg(rooms.join(",")).arg(savedValue).arg(current).arg(toSelect).arg(index).toUtf8());
}
