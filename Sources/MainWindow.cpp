#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "QtWebSocketClient.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStandardItem>
#include <QMenu>
#include "TableItemDelegate.h"

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow),
        model(new QStandardItemModel(this)),
        client(nullptr) {

    ui->setupUi(this);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &MainWindow::onContextMenuRequested);
    ui->log->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->message->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->message->setAcceptDrops(false);
    this->setAcceptDrops(true);
    updateButtonStates(false);
}

MainWindow::~MainWindow() {
    if (client) {
        client->disconnectFromServer();
        client->deleteLater();
    }
    delete ui;
}

void MainWindow::on_connectButton_clicked() {
    if (client) {
        client->disconnectFromServer();
        client->deleteLater();
    }

    WebSocketConfig config;
    config.jwtToken = "puppeteer";
    config.reconnectIntervalMs = 5000;
    config.maxReconnectAttempts = 500;

    client = new QtWebSocketClient(ui->location->text(), config, this);

    connect(client, &QtWebSocketClient::connected, this, &MainWindow::onConnected);
    connect(client, &QtWebSocketClient::disconnected, this, &MainWindow::onDisconnected);
    connect(client, &QtWebSocketClient::messageReceived, this, &MainWindow::onMessageReceived);
    connect(client, &QtWebSocketClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(client, &QtWebSocketClient::reconnecting, this, &MainWindow::onReconnecting);

    appendLog("连接中: " + ui->location->text());
    client->connectToServer();
}

void MainWindow::on_disconnectButton_clicked() {
    if (client) {
        client->disconnectFromServer();
    }
}

void MainWindow::onConnected() {
    appendLog("已连接到服务器");
    updateButtonStates(true);
}

void MainWindow::onDisconnected() {
    appendLog("已断开连接");
    updateButtonStates(false);
}

void MainWindow::onMessageReceived(const QString &message) {
    appendLog("服务端: " + message);
}

void MainWindow::onErrorOccurred(const QString &error) {
    appendLog("错误: " + error);
}

void MainWindow::onReconnecting(int attempt, int maxAttempts) {
    appendLog(QString("重连中... (%1/%2)").arg(attempt).arg(maxAttempts));
}

void MainWindow::on_sendButton_clicked() {
    if (!client || !client->isConnected()) {
        appendLog("未连接，无法发送");
        return;
    }

    auto *modelList = qobject_cast<QStandardItemModel *>(ui->tableView->model());
    if (modelList && modelList->rowCount() > 0) {
        for (int i = 0; i < modelList->rowCount(); i++) {
            auto *item = modelList->item(i, 0);
            if (item) {
                client->sendImage(item->text());
                appendLog("发送图片: " + item->text());
            }
        }
    } else {
        auto message = ui->message->toPlainText().trimmed();
        if (message.isEmpty()) {
            return;
        }
        client->sendMessage(message);
        appendLog("客户端: " + message);
        ui->message->clear();
    }
}

void MainWindow::on_clearButton_clicked() {
    ui->log->clear();
    ui->clearButton->setEnabled(false);
}

void MainWindow::on_selectFile_clicked() {
    const auto items = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                     QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                                     "所有文件 (*.*)");
    if (items.isEmpty()) {
        return;
    }
    delete model;
    model = new QStandardItemModel(static_cast<int>(items.length()), 1, this);
    setTableValue(items);
}

void MainWindow::setTableValue(const QList<QString> &items) {
    ui->tableView->setModel(model);
    ui->tableView->setStyleSheet("QTableView { border: none; }");
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setShowGrid(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setDefaultSectionSize(0);
    ui->tableView->setItemDelegate(new TableItemDelegate(ui->tableView));

    int row = 0;
    for (const auto &item : items) {
        auto *newItem = new QStandardItem(item);
        newItem->setData(item, Qt::EditRole);
        model->setItem(row, 0, newItem);
        row++;
    }
}

void MainWindow::onContextMenuRequested(QPoint pos) {
    QMenu contextMenu(tr("Context menu"), this);
    contextMenu.setStyleSheet("QMenu { border: none; }");

    QModelIndex index = ui->tableView->indexAt(pos);
    if (index.isValid()) {
        QAction action1("删除", this);
        action1.setData(index.row());
        connect(&action1, &QAction::triggered, this, &MainWindow::doWork);
        contextMenu.addAction(&action1);

        QAction action2("复制", this);
        action2.setData(index.row());
        connect(&action2, &QAction::triggered, this, &MainWindow::doWork);
        contextMenu.addAction(&action2);

        contextMenu.exec(ui->tableView->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::doWork() {
    auto *action = qobject_cast<QAction *>(sender());
    if (action) {
        int row = action->data().toInt();
        model->removeRow(row);
    }
}

void MainWindow::appendLog(const QString &text) {
    ui->log->append(text);
    ui->clearButton->setEnabled(true);
}

void MainWindow::updateButtonStates(bool connected) {
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    ui->sendButton->setEnabled(connected);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QList<QString> items;
    for (const QUrl &url : event->mimeData()->urls()) {
        items.append(url.toLocalFile());
    }
    setTableValue(items);
}
