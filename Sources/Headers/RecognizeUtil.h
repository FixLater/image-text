#include "ImgTool.h"
#include "opencv2/opencv.hpp"
#include <QApplication>

class RecognizeUtil {
public:
    typedef struct {
        // 题号
        std::string quesNum;
        // 题块区域
        std::vector<std::vector<cv::Point>> matOfPoints;
        // 题块答案（长度为题选项个数，0为未选中项，1为选中项）
        std::vector<int> quesAnswers;
    } QuesRect;

    typedef struct {
        // 题号
        std::string quesNum;
        // 题块区域
        std::vector<cv::Rect> rects;
        // 题块答案（长度为题选项个数，0为未选中项，1为选中项）
        std::vector<int> quesAnswers;
    } QuesRect2;


public:
    RecognizeUtil();

    ~RecognizeUtil();

public:
    // 识别题号填涂区
    static QString recogExamNo(cv::Mat desc, int number_id);
};