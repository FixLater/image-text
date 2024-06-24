//#include <QApplication>
//#include <QPushButton>
//#include "opencv2/opencv.hpp"
//#include "Sources/Headers/RecognizeUtil.h"
//
//using namespace cv;
//using namespace std;
//
//int main(int argc, char *argv[]) {
//
//    std::cout << "开始识别" << std::endl;
//    Mat emptyImg = imread(R"(D:\Hobby\c++\test_exam_no\example1.jpg)", 0);
//    int number = 8;
//    QString tExamNo = RecognizeUtil::recogExamNo(emptyImg, number);
//    std::cout << "结束识别：" << tExamNo.toUtf8().constData() << std::endl;
//    QApplication a(argc, argv);
//    QPushButton button(tExamNo, nullptr);
//    button.resize(200, 100);
//    button.show();
//    return QApplication::exec();
//}

#include <boost/asio.hpp>
#include <QApplication>
#include "MainWindow.h"
#include <iostream>
#include <QClipboard>
#include <regex>
#include "Sources/UnicodeUtil.cpp"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, [&](){
        std::string clipboardText = QApplication::clipboard()->text().toStdString();
        std::regex pattern("[A-F]:/.*");
        std::smatch match;
        if (std::regex_search(clipboardText, match, pattern)) {
            for (auto sub_match : match) {
                std::string file_path = sub_match.str();
//                w.client->sendFile(UnicodeUtil::gbk_to_utf8(file_path));
                w.client->sendFile(file_path);
            }
        } else {
            w.client->send(3, clipboardText);
            emit w.onMessageReceived("剪切板：" + clipboardText);
        }
    });
    return QApplication::exec();
}


