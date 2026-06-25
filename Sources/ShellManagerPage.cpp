#include "ShellManagerPage.h"
#include <QScrollBar>
#include <QDir>
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QTextCursor>
#include <QRegularExpression>
#include <QFont>

ShellManagerPage::ShellManagerPage(QWidget *parent)
    : QWidget(parent)
    , m_activeTabIndex(-1)
    , m_connected(false)
{
    setupUI();
    addTab("本地终端");
}

ShellManagerPage::~ShellManagerPage() {
    for (auto &tab : m_tabs) {
        if (tab.pty) {
            tab.pty->kill();
            delete tab.pty;
        }
    }
}

QString ShellManagerPage::stripAnsi(const QString &text) {
    static QRegularExpression ansiRegex("\\x1b\\[[?!]?[0-9;]*[A-Za-z]|\\x1b\\][^\x07]*\x07|\\x1b\\([AB012]");
    QString result = text;
    result.remove(ansiRegex);
    result.replace("\r\n", "\n");
    result.replace("\r", "");
    return result;
}

void ShellManagerPage::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_navBar = new QWidget();
    m_navBar->setObjectName("navBar");
    m_navBar->setFixedHeight(36);
    m_navBar->setStyleSheet("#navBar { background-color: #1e2128; border-bottom: 1px solid #36383d; }");
    auto *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(8, 0, 8, 0);
    navLayout->setSpacing(2);

    auto *newConnBtn = new QPushButton("＋");
    newConnBtn->setFixedSize(28, 28);
    newConnBtn->setCursor(Qt::PointingHandCursor);
    newConnBtn->setToolTip("新建连接");
    newConnBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; border-radius: 4px; font-size: 12pt; }"
        "QPushButton:hover { background-color: #36383d; color: #e2e8f0; }"
    );
    connect(newConnBtn, &QPushButton::clicked, this, &ShellManagerPage::onNewConnectionClicked);
    navLayout->addWidget(newConnBtn);

    auto *sep = new QLabel("|");
    sep->setStyleSheet("color: #36383d; font-size: 9pt; background: transparent; border: none; padding: 0 2px;");
    navLayout->addWidget(sep);

    m_breadcrumbLabel = new QLabel();
    m_breadcrumbLabel->setStyleSheet(
        "color: #94a3b8; font-size: 9pt; background: transparent; border: none; padding-left: 4px;"
    );
    navLayout->addWidget(m_breadcrumbLabel, 1);

    mainLayout->addWidget(m_navBar);

    m_terminal = new QPlainTextEdit();
    m_terminal->setReadOnly(false);
    m_terminal->setContextMenuPolicy(Qt::NoContextMenu);
    m_terminal->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_terminal->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_terminal->setWordWrapMode(QTextOption::NoWrap);
    m_terminal->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #0c0c0c; color: #cccccc; border: none;"
        "  font-family: 'Cascadia Code', 'Consolas', 'Courier New', monospace;"
        "  font-size: 10pt; padding: 8px 12px;"
        "  selection-background-color: #264f78;"
        "}"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );
    m_terminal->installEventFilter(this);
    mainLayout->addWidget(m_terminal, 1);
}

bool ShellManagerPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_terminal && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size())
            return false;

        auto &tab = m_tabs[m_activeTabIndex];
        QTextCursor cursor = m_terminal->textCursor();
        int currentPos = cursor.position();
        int inputStart = tab.inputStartPos;

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            cursor.movePosition(QTextCursor::End);
            QString inputText = m_terminal->toPlainText().mid(inputStart);
            cursor.insertText("\n");
            m_terminal->setTextCursor(cursor);

            if (!inputText.isEmpty()) {
                tab.commandHistory.append(inputText);
                tab.historyIndex = tab.commandHistory.size();
                if (tab.pty && tab.pty->isRunning()) {
                    tab.pty->write((inputText + "\n").toUtf8());
                } else {
                    startConnection();
                }
            }
            tab.inputStartPos = m_terminal->toPlainText().size();
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Up) {
            if (!tab.commandHistory.isEmpty() && tab.historyIndex > 0) {
                tab.historyIndex--;
                QString oldCmd = tab.commandHistory[tab.historyIndex];
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursor.position() - inputStart);
                cursor.removeSelectedText();
                cursor.insertText(oldCmd);
                m_terminal->setTextCursor(cursor);
            }
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Down) {
            if (tab.historyIndex < tab.commandHistory.size() - 1) {
                tab.historyIndex++;
                QString oldCmd = tab.commandHistory[tab.historyIndex];
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursor.position() - inputStart);
                cursor.removeSelectedText();
                cursor.insertText(oldCmd);
                m_terminal->setTextCursor(cursor);
            } else {
                tab.historyIndex = tab.commandHistory.size();
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursor.position() - inputStart);
                cursor.removeSelectedText();
                m_terminal->setTextCursor(cursor);
            }
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Left) {
            if (currentPos <= inputStart) return true;
        }
        else if (keyEvent->key() == Qt::Key_Home) {
            cursor.setPosition(inputStart);
            m_terminal->setTextCursor(cursor);
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Backspace) {
            if (currentPos <= inputStart) return true;
        }
        else {
            if (currentPos < inputStart) {
                cursor.movePosition(QTextCursor::End);
                m_terminal->setTextCursor(cursor);
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

int ShellManagerPage::tabCount() const { return m_tabs.size(); }
int ShellManagerPage::activeTabIndex() const { return m_activeTabIndex; }
bool ShellManagerPage::isActive() const { return m_connected; }

QString ShellManagerPage::tabTitle(int index) const {
    if (index < 0 || index >= m_tabs.size()) return {};
    const auto &tab = m_tabs[index];
    if (tab.isRemote) return QString("%1@%2").arg(tab.username, tab.host);
    return tab.name;
}

QString ShellManagerPage::tabLogHtml(int index) const {
    if (index < 0 || index >= m_tabs.size()) return {};
    return m_tabs[index].outputBuffer;
}

void ShellManagerPage::addTab(const QString &name) {
    ShellTabState state;
    state.name = name.isEmpty() ? QString("终端 %1").arg(m_tabs.size() + 1) : name;
    state.host = "123.57.80.217";
    state.port = 22;
    state.username = "root";
    state.isRemote = false;
    state.pty = nullptr;
    state.inputStartPos = 0;

    m_tabs.append(state);
    switchTab(m_tabs.size() - 1);
    emit tabsChanged();
}

void ShellManagerPage::switchTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;
    if (m_activeTabIndex >= 0 && m_activeTabIndex < m_tabs.size()) saveTabState();
    m_activeTabIndex = index;
    loadTabState();
    updateUIState();
    emit tabsChanged();
}

void ShellManagerPage::closeTab(int index) {
    if (index < 0 || index >= m_tabs.size()) return;

    auto &tab = m_tabs[index];
    if (tab.pty) {
        tab.pty->kill();
        delete tab.pty;
        tab.pty = nullptr;
    }

    m_tabs.removeAt(index);
    if (m_tabs.isEmpty()) { addTab(); return; }

    if (m_activeTabIndex >= m_tabs.size()) m_activeTabIndex = m_tabs.size() - 1;
    else if (m_activeTabIndex > index) m_activeTabIndex--;
    else if (m_activeTabIndex == index) m_activeTabIndex = qMin(index, m_tabs.size() - 1);

    switchTab(m_activeTabIndex);
}

void ShellManagerPage::connectToCurrentTab() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    auto &tab = m_tabs[m_activeTabIndex];
    if (tab.pty && tab.pty->isRunning()) return;
    startConnection();
}

void ShellManagerPage::saveTabState() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    m_tabs[m_activeTabIndex].outputBuffer = m_terminal->toPlainText();
}

void ShellManagerPage::loadTabState() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    const auto &tab = m_tabs[m_activeTabIndex];

    if (!tab.outputBuffer.isEmpty()) {
        m_terminal->setPlainText(tab.outputBuffer);
        QTextCursor cursor = m_terminal->textCursor();
        cursor.setPosition(tab.inputStartPos);
        m_terminal->setTextCursor(cursor);
    } else {
        m_terminal->clear();
    }

    updateBreadcrumb();
    bool running = tab.pty && tab.pty->isRunning();
    setRunning(running);
}

void ShellManagerPage::updateUIState() {
    m_terminal->setEnabled(!m_tabs.isEmpty());
}

void ShellManagerPage::updateBreadcrumb() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) {
        m_breadcrumbLabel->setText("");
        return;
    }
    const auto &tab = m_tabs[m_activeTabIndex];
    QString arrow = " <span style='color: #475569;'>›</span> ";

    if (tab.isRemote) {
        m_breadcrumbLabel->setText(
            QString("<span style='color: #0ea5e9;'>ssh</span>%1"
                    "<span style='color: #0ea5e9;'>%2</span>%3"
                    "<span style='color: #e2e8f0;'>%4</span>")
                .arg(arrow, tab.username, arrow, tab.host));
    } else {
        m_breadcrumbLabel->setText(
            QString("<span style='color: #0ea5e9;'>local</span>%1"
                    "<span style='color: #e2e8f0;'>%2</span>")
                .arg(arrow, tab.name));
    }
}

void ShellManagerPage::appendOutput(const QString &text) {
    QString clean = stripAnsi(text);
    if (clean.isEmpty()) return;

    QTextCursor cursor = m_terminal->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(clean);
    m_terminal->setTextCursor(cursor);

    if (m_activeTabIndex >= 0 && m_activeTabIndex < m_tabs.size()) {
        auto &tab = m_tabs[m_activeTabIndex];
        tab.outputBuffer = m_terminal->toPlainText();
        tab.inputStartPos = m_terminal->toPlainText().size();
    }

    m_terminal->ensureCursorVisible();
}

void ShellManagerPage::setRunning(bool running) {
    m_connected = running;
    emit statusChanged(running);
}

void ShellManagerPage::startConnection() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) return;
    auto &tab = m_tabs[m_activeTabIndex];

    if (tab.pty && tab.pty->isRunning()) return;

    m_terminal->clear();
    tab.inputStartPos = 0;
    tab.outputBuffer.clear();

    tab.pty = new ConPtyProcess(this);

    connect(tab.pty, &ConPtyProcess::readyRead, this, [this](const QByteArray &data) {
        int idx = -1;
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].pty == sender()) { idx = i; break; }
        }
        if (idx < 0) return;

        if (m_activeTabIndex == idx) {
            QString text = QString::fromUtf8(data);
            appendOutput(text);

            auto &tab = m_tabs[idx];
            if (tab.isRemote && !tab.password.isEmpty() && !tab.passwordSent) {
                static QRegularExpression passPrompt("(password|密码)\\s*[:：]", QRegularExpression::CaseInsensitiveOption);
                if (passPrompt.match(text).hasMatch()) {
                    tab.passwordSent = true;
                    tab.pty->write((tab.password + "\n").toUtf8());
                }
            }
        }
    });

    connect(tab.pty, &ConPtyProcess::finished, this, [this](int exitCode) {
        int idx = -1;
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].pty == sender()) { idx = i; break; }
        }
        if (idx < 0) return;

        if (m_activeTabIndex == idx) {
            appendOutput(QString("\n[Process exited with code %1]\n").arg(exitCode));
            setRunning(false);
        }
    });

    connect(tab.pty, &ConPtyProcess::errorOccurred, this, [this](const QString &err) {
        int idx = -1;
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].pty == sender()) { idx = i; break; }
        }
        if (idx < 0) return;

        if (m_activeTabIndex == idx) {
            appendOutput("[Error] " + err + "\n");
            setRunning(false);
        }
    });

    bool ok;
    if (tab.isRemote) {
        ok = tab.pty->start("ssh", {
            "-o", "StrictHostKeyChecking=no",
            "-p", QString::number(tab.port),
            QString("%1@%2").arg(tab.username, tab.host)
        });
    } else {
#ifdef Q_OS_WIN
        ok = tab.pty->start("cmd.exe", {"/k"});
#else
        ok = tab.pty->start("/bin/bash", {"-i"});
#endif
    }

    if (ok) {
        setRunning(true);
        updateBreadcrumb();
        m_terminal->setFocus();
    }
}

void ShellManagerPage::showNewConnectionDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("新建连接");
    dialog.setMinimumWidth(380);
    dialog.setStyleSheet(
        "QDialog { background-color: #1e2128; }"
        "QLabel { color: #94a3b8; font-size: 9pt; background: transparent; border: none; }"
        "QLineEdit { background-color: #0f1117; color: #e2e8f0; border: 1px solid #36383d;"
        "  border-radius: 4px; font-size: 9pt; padding: 6px 8px; }"
        "QLineEdit:focus { border-color: #0ea5e9; }"
        "QPushButton { font-size: 9pt; padding: 6px 16px; border-radius: 4px; border: none; }"
    );

    auto *layout = new QFormLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 16, 20, 16);

    auto *titleLabel = new QLabel("SSH 连接配置");
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; color: #e2e8f0; margin-bottom: 8px;");
    layout->addRow(titleLabel);

    auto *hostEdit = new QLineEdit("123.57.80.217");
    hostEdit->setPlaceholderText("IP 地址");
    layout->addRow("主机:", hostEdit);

    auto *portEdit = new QLineEdit("22");
    portEdit->setMaximumWidth(80);
    layout->addRow("端口:", portEdit);

    auto *userEdit = new QLineEdit("root");
    layout->addRow("用户名:", userEdit);

    auto *passEdit = new QLineEdit("Lovely0814.");
    passEdit->setPlaceholderText("密码（可选，SSH密钥免密）");
    passEdit->setEchoMode(QLineEdit::Password);
    layout->addRow("密码:", passEdit);

    auto *nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("标签显示名称");
    layout->addRow("名称:", nameEdit);

    auto *btnBox = new QDialogButtonBox();
    auto *cancelBtn = btnBox->addButton("取消", QDialogButtonBox::RejectRole);
    cancelBtn->setStyleSheet("QPushButton { background-color: #374151; color: #94a3b8; }"
                             "QPushButton:hover { background-color: #4b5563; color: #e2e8f0; }");
    auto *okBtn = btnBox->addButton("连接", QDialogButtonBox::AcceptRole);
    okBtn->setStyleSheet("QPushButton { background-color: #0ea5e9; color: white; }"
                         "QPushButton:hover { background-color: #38bdf8; }");
    layout->addRow(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString host = hostEdit->text().trimmed();
        int port = portEdit->text().toInt();
        QString user = userEdit->text().trimmed();
        QString pass = passEdit->text();
        QString name = nameEdit->text().trimmed();

        if (host.isEmpty() || user.isEmpty()) {
            QMessageBox::warning(this, "错误", "主机和用户名不能为空");
            return;
        }
        onConnectConfirmed(host, port, user, pass);
    }
}

void ShellManagerPage::onNewConnectionClicked() {
    showNewConnectionDialog();
}

void ShellManagerPage::onConnectConfirmed(const QString &host, int port, const QString &user, const QString &pass) {
    ShellTabState state;
    state.name = QString("%1@%2").arg(user, host);
    state.host = host;
    state.port = port;
    state.username = user;
    state.password = pass;
    state.isRemote = true;
    state.pty = nullptr;
    state.inputStartPos = 0;

    m_tabs.append(state);
    switchTab(m_tabs.size() - 1);
    emit tabsChanged();
    startConnection();
}
