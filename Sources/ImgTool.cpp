//
// Created by Administrator on 2024/4/9.
//

#include "headers/ImgTool.h"
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

//gamma变换
Mat imgtool::gamma(Mat& src, float fGamma)
{
    unsigned char lut[256];
    for (int i = 0; i < 256; i++)
    {
        lut[i] = saturate_cast<uchar>(pow((float)(i / 255.0), fGamma) * 255.0f);
    }
    Mat dst = src.clone();
    const int channels = dst.channels();
    MatIterator_<uchar> it, end;
    for (it = dst.begin<uchar>(), end = dst.end<uchar>(); it != end; it++)
        *it = lut[(*it)];
    return dst;
}

//注意img是白底黑字的灰度图，以0.2为判断标准
double imgtool::gray_empty(Mat img) {
    Mat img_clone = img.clone();
    Mat gaimg = gamma(img_clone, 2.5);

    Mat bin;
    Mat mean_img;
    medianBlur(gaimg, mean_img, 3);
    vector<int> hist = calcGrayHist(mean_img);
    int max_val = distance(hist.begin(), max_element(hist.begin(), hist.end()));
    Mat img_scale;
    convertScaleAbs(gaimg, img_scale, 255 * 1.0 / max_val);


    threshold(img_scale, bin, 0, 255, ThresholdTypes::THRESH_OTSU);
    copyMakeBorder(bin, bin, 20, 20, 20, 20, BORDER_CONSTANT, Scalar(255));
    Mat gf;
    GaussianBlur(bin, gf, Size(51, 21), 0, 0);

    double min_value;
    minMaxLoc(gf, &min_value, 0, 0);
    double ratio = abs(255 - min_value) / 255;

    //判断去边后是不是什么分量都没有了或者本身就是全黑的，是的话就判空
    //vector<uchar> two_pix{ gf_bin.at<uchar>(0, 0) };
    //for (int i = 0; i < gf_bin.rows; i++)
    //{
    //	bool jump_for = false;
    //	uchar*pgf = gf_bin.ptr<uchar>(i);
    //	for (int j = 0; j < gf_bin.cols; j++) {
    //		if (two_pix[0] != pgf[j])
    //		{
    //			two_pix.push_back(pgf[j]);
    //			jump_for = true;
    //			break;
    //		}
    //	}
    //	if (jump_for)
    //		break;
    //}
    //if (two_pix.size() == 1)
    //	ratio = 0;
    return ratio;
}

vector<int> imgtool::calcGrayHist(Mat image)
{
    vector<int> vec(256, 0);
    int rows = image.rows;
    int cols = image.cols;
    for (int i = 0; i < rows; i++)
    {
        auto* pimage = image.ptr<uchar>(i);
        for (int j = 0; j < cols; j++)
        {
//            vec[pimage[i, j]]++;
        }
    }
    return vec;
}