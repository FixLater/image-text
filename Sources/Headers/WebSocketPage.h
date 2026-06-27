#ifndef WEBSOCKETPAGE_H
#define WEBSOCKETPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QComboBox>
#include <QPushButton>
#include "ChatLogWidget.h"

class QtWebSocketClient;
class StarBackground;

namespace Ui {
    class WebSocketPage;
}

class WebSocketPage : public QWidget {
    Q_OBJECT

public:
    explicit WebSocketPage(QWidget *parent = nullptr);
    ~WebSocketPage() override;

    bool isConnected() const;
    void refreshUrlFromSettings();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void statusChanged(bool connected);
    void messageReceived(const QString &message);
    void logAppended(const QString &html);

private slots:
    void on_connectButton_clicked();
    void on_sendButton_clicked();
    void on_selectFile_clicked();
    void onContextMenuRequested(QPoint pos);
    void onActionDelete();
    void onActionReplace();

private:
    Ui::WebSocketPage *ui;
    int m_contextRow = -1;
    ChatLogWidget *m_chatLog = nullptr;
    StarBackground *m_starBg = nullptr;
    QComboBox *m_roomComboBox = nullptr;
    QPushButton *m_joinRoomBtn = nullptr;
    QString m_currentRoom;

    QtWebSocketClient *m_client = nullptr;
    QString m_url;
    QString m_messageText;
    QStandardItemModel *m_fileModel = nullptr;
    QVector<ChatMessage> m_logMessages;
    bool m_connecting = false;
    bool m_roomListInitialized = false;

    void appendLog(const QString &message, const QString &type = "info");
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

    void onRoomListReceived(const QStringList &rooms);
    void onJoinRoomClicked();
};

#endif // WEBSOCKETPAGE_H
