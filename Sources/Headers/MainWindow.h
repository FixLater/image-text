#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QObject>
#include <QStandardItemModel>
#include "WebSocketClient.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void onMessageReceived(const std::string &message);
    WebSocketClient *client = nullptr;

private slots:

    void onContextMenuRequested(QPoint pos);
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_sendButton_clicked();
    void on_clearButton_clicked();
    void on_selectFile_clicked();
    void setTableValue(const QList<QString>& items);
    void doWork();

private:
    Ui::MainWindow *ui;
    QStandardItemModel *model;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

#endif