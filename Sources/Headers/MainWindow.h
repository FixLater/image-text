#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QVector>
#include <QPushButton>

class QtWebSocketClient;

struct TabState {
    QString url;
    QString messageText;
    QtWebSocketClient *client = nullptr;
    QStandardItemModel *fileModel = nullptr;
    QString logHtml;
    bool showFiles = false;
};

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_sendButton_clicked();
    void on_clearButton_clicked();
    void on_selectFile_clicked();
    void onContextMenuRequested(QPoint pos);
    void onActionDelete();
    void onActionReplace();

private:
    Ui::MainWindow *ui;
    int m_contextRow;
    QPoint m_dragPosition;
    bool m_isDragging = false;

    QVector<TabState> m_tabs;
    int m_activeTabIndex = -1;

    void addNewTab(const QString &url = "ws://127.0.0.1:8200/websocket");
    void switchToTab(int index);
    void rebuildTabBar();
    void removeTab(int index);

    void appendLog(const QString &message, const QString &type = "info");
    void updateButtonStates(bool connected);
    void addFiles(const QStringList &files);
    void applyApiFoxStyle();
    void updateStatusStyle();
    bool isInTitleBarArea(const QPoint &pos) const;

    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QString &message);
    void onErrorOccurred(const QString &error);
    void onReconnecting(int attempt, int maxAttempts);
};

#endif // MAINWINDOW_H
