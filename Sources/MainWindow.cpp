#include "MainWindow.h"
#include "Froms/ui_mainwindow.h"
#include <QUrlQuery>
#include <iostream>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStandardItem>
#include <QMenu>
#include "TableItemDelegate.cpp"

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow),
        model(new QStandardItemModel(this)) {
//    std::locale::global(std::locale("en_US.utf8"));
    ui->setupUi(this);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(onContextMenuRequested(QPoint)));
    ui->log->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->message->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->clearButton->setEnabled(false);
    ui->message->setAcceptDrops(false);  // 禁止 QTextEdit 接收拖拽事件
    this->setAcceptDrops(true);  // 允许 MainWindow 接收拖拽事件

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

void MainWindow::onMessageReceived(const std::string &message) {
    QString msg = QString::fromStdString(message);
    ui->log->append(msg);
    std::string plainText = ui->log->toPlainText().toStdString();
    ui->clearButton->setEnabled(true);
}

void MainWindow::on_sendButton_clicked() {
    auto *modelList = qobject_cast<QStandardItemModel *>(ui->tableView->model());
    if (modelList != nullptr) {
        for (int i = 0; i < modelList->rowCount(); i++) {
            QStandardItem * item = modelList->item(i, 0);  // 获取第 i 行，第 0 列的元素
            // 打开文件
            QString qstr = item->text();
            std::string str = std::string(qstr.toLocal8Bit().constData());
            client->sendFile(str);
        }
    } else {
        std::string message = ui->message->toPlainText().toStdString();
        if (message.empty()) {
            return;
        }
        emit onMessageReceived("客户端：" + message);
        client->send(1, message);
    }

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

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QList <QString> items;
    foreach(
    const QUrl &url, event->mimeData()->urls()) {
        QString fileName = url.toLocalFile();
        items.append(fileName);
    }
    setTableValue(items);
}

void MainWindow::on_selectFile_clicked() {

    const auto items = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                     QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                                     "所有文件 (*.*)");
    if (items.isEmpty()) {
        return;
    }
    int length = static_cast<int>(items.length());
    model = new QStandardItemModel(length, 1, this);  // 5 行 1 列

    setTableValue(items);
}

void MainWindow::setTableValue(const QList <QString> &items) {
    ui->tableView->setModel(model);
    ui->tableView->setStyleSheet("QTableView { border: none; }");
    // 隐藏表头和列序号
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setShowGrid(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setDefaultSectionSize(0);  // 设置行高
    ui->tableView->setItemDelegate(new TableItemDelegate(this));  // 设置单元格的代理

    int row = 0;  // 定义 row 变量
    for (const auto &item: items) {
        auto *newItem = new QStandardItem(item);  // 使用新的变量名
        newItem->setData(item, Qt::EditRole);
        model->setItem(row, 0, newItem);  // 第二个参数为0，表示只有一列
        row++;  // 更新 row 的值
    }
}

void MainWindow::onContextMenuRequested(QPoint pos)
{
    QMenu contextMenu(tr("Context menu"), this);
    contextMenu.setStyleSheet("QMenu { border: none; }");

    QModelIndex index = ui->tableView->indexAt(pos);
    if (index.isValid()) {
        QAction action1("删除", this);
        action1.setData(index.row());  // 将当前行的行号设置到 QAction 中
        connect(&action1, &QAction::triggered, this, &MainWindow::doWork);
        contextMenu.addAction(&action1);

        QAction action2("复制", this);
        action2.setData(index.row());  // 将当前行的行号设置到 QAction 中
        connect(&action2, &QAction::triggered, this, &MainWindow::doWork);
        contextMenu.addAction(&action2);

        contextMenu.exec(ui->tableView->viewport()->mapToGlobal(pos));
    }
}


void MainWindow::doWork()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        int row = action->data().toInt();  // 从 QAction 中获取当前行的行号
        // 删除指定行
        model->removeRow(row);
    }
}
