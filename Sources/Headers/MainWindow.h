#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>

class QtWebSocketClient;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_sendButton_clicked();
    void on_clearButton_clicked();
    void on_selectFile_clicked();
    void onContextMenuRequested(QPoint pos);
    void setTableValue(const QList<QString> &items);
    void doWork();

    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QString &message);
    void onErrorOccurred(const QString &error);
    void onReconnecting(int attempt, int maxAttempts);

private:
    Ui::MainWindow *ui;
    QStandardItemModel *model;
    QtWebSocketClient *client;

    void appendLog(const QString &text);
    void updateButtonStates(bool connected);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

#endif // MAINWINDOW_H
