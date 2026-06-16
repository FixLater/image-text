#ifndef WEBSOCKETPAGE_H
#define WEBSOCKETPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QVector>
#include "ChatLogWidget.h"

class QtWebSocketClient;

struct TabState {
    QString url;
    QString messageText;
    QtWebSocketClient *client = nullptr;
    QStandardItemModel *fileModel = nullptr;
    QStringList logEntries;
    QVector<ChatMessage> logMessages;
    bool showFiles = false;
    bool connecting = false;
};

namespace Ui {
    class WebSocketPage;
}

class WebSocketPage : public QWidget {
    Q_OBJECT

public:
    explicit WebSocketPage(QWidget *parent = nullptr);
    ~WebSocketPage() override;

    int tabCount() const;
    int activeTabIndex() const;
    QString tabTitle(int index) const;
    void addTab(const QString &url = "ws://127.0.0.1:8200/websocket");
    void switchTab(int index);
    void closeTab(int index);
    bool isConnected() const;
    void connectToCurrentTab();
    QString tabLogHtml(int index) const;
    void refreshUrlFromSettings();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void tabsChanged();
    void statusChanged(bool connected);
    void logAppended(int tabIndex, const QString &html);
    void messageReceived(const QString &message);

private slots:
    void on_connectButton_clicked();
    void on_sendButton_clicked();
    void on_selectFile_clicked();
    void onContextMenuRequested(QPoint pos);
    void onActionDelete();
    void onActionReplace();

private:
    Ui::WebSocketPage *ui;
    int m_contextRow;
    ChatLogWidget *m_chatLog = nullptr;

    QVector<TabState> m_tabs;
    int m_activeTabIndex = -1;

    void updateUIState();

    void appendLog(const QString &message, const QString &type = "info", int tabIndex = -1);
    int findTabIndexForClient(QObject *client) const;
    void updateButtonStates(bool connected);
    void addFiles(const QStringList &files);
    void applyApiFoxStyle();
    void updateStatusStyle();
    void updateSendButtonState();

    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QString &message);
    void onErrorOccurred(const QString &error);
    void onReconnecting(int attempt, int maxAttempts);
};

#endif // WEBSOCKETPAGE_H
