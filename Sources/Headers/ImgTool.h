
#include "opencv2/opencv.hpp"
#include <QApplication>

namespace imgtool {
    //gamma变换
    cv::Mat gamma(cv::Mat& src, float fGamma);

    //注意img是白底黑字的灰度图，以0.2为判断标准
    double gray_empty(cv::Mat img);

    //计算灰度图的直方图
    std::vector<int> calcGrayHist(cv::Mat image);
}
