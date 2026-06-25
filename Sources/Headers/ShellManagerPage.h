#ifndef SHELLMANAGERPAGE_H
#define SHELLMANAGERPAGE_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QVector>
#include <QScrollArea>

#include "ConPtyProcess.h"

struct ShellTabState {
    QString name;
    QString host;
    int port;
    QString username;
    QString password;
    bool isRemote;
    bool passwordSent = false;
    ConPtyProcess *pty = nullptr;
    QString outputBuffer;
    int inputStartPos = 0;
    QStringList commandHistory;
    int historyIndex = -1;
};

class ShellManagerPage : public QWidget {
    Q_OBJECT

public:
    explicit ShellManagerPage(QWidget *parent = nullptr);
    ~ShellManagerPage() override;

    int tabCount() const;
    int activeTabIndex() const;
    QString tabTitle(int index) const;
    void addTab(const QString &name = QString());
    void switchTab(int index);
    void closeTab(int index);
    void connectToCurrentTab();
    QString tabLogHtml(int index) const;
    bool isActive() const;

signals:
    void tabsChanged();
    void statusChanged(bool connected);
    void logAppended(int tabIndex, const QString &html);

private slots:
    void onNewConnectionClicked();
    void onConnectConfirmed(const QString &host, int port, const QString &user, const QString &pass);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void updateUIState();
    void updateBreadcrumb();
    void appendOutput(const QString &text);
    void setRunning(bool running);
    void saveTabState();
    void loadTabState();
    void startConnection();
    void showNewConnectionDialog();
    QString stripAnsi(const QString &text);

    QWidget *m_navBar;
    QLabel *m_breadcrumbLabel;

    QPlainTextEdit *m_terminal;

    QVector<ShellTabState> m_tabs;
    int m_activeTabIndex = -1;
    bool m_connected = false;
};

#endif // SHELLMANAGERPAGE_H
