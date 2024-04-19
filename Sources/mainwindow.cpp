#include "mainwindow.h"
#include "Froms/ui_mainwindow.h"
#include <QUrlQuery>
#include <iostream>

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow)  // Initialize the WebSocketClient object
{
    ui->setupUi(this);
    ui->log->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->clearButton->setEnabled(false);
//    connect(client, &WebSocketClient::connected, client, &WebSocketClient::messageToSend);
//    connect(client, &WebSocketClient::messageToSend, this, &MainWindow::onMessageToSend);
}

MainWindow::~MainWindow() {
    delete client;  // Delete the WebSocketClient object
    delete ui;
}

void MainWindow::on_connectButton_clicked() {
    std::string url = ui->location->text().toStdString();
    client = new WebSocketClient(url);
    connect(client, &WebSocketClient::messageReceived, this, &MainWindow::onMessageReceived);
    connect(client, &WebSocketClient::disConnect, this, &MainWindow::on_disconnectButton_clicked);
    bool success = client->connected();  // Call the connected method of the WebSocketClient object
    if (success) {
        client->do_read();
        emit onMessageReceived("连接成功");
        ui->connectButton->setEnabled(false);
        ui->disconnectButton->setEnabled(true);
        ui->sendButton->setEnabled(true);
        ui->message->setEnabled(true);
    }
}

void MainWindow::onMessageReceived(const std::string& message) {
    QString msg = QString::fromStdString(message);
    ui->log->append(msg);
    std::string plainText = ui->log->toPlainText().toStdString();
    ui->clearButton->setEnabled(true);
}

void MainWindow::on_sendButton_clicked() {
    std::string message = ui->message->text().toStdString();
    if (message.empty()) {
        return;
    }
    emit onMessageReceived("客户端：" + message);
    client->send(1, message);
}

void MainWindow::on_disconnectButton_clicked() {
    bool success = client->close();
    if (success) {
        ui->connectButton->setEnabled(true);
        ui->disconnectButton->setEnabled(false);
        ui->sendButton->setEnabled(false);
        ui->message->setEnabled(false);
    }
    emit onMessageReceived("断开连接");
}

void MainWindow::on_clearButton_clicked() {
    ui->log->clear();
    ui->clearButton->setEnabled(false);
}

