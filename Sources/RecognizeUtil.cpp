//
// Created by Administrator on 2024/4/9.
//
#include "opencv2/opencv.hpp"
#include <numeric>
#include "headers/RecognizeUtil.h"

using namespace std;
using namespace cv;

QString RecognizeUtil::recogExamNo(Mat desc, int number_id) {
    //开始计时
    //模拟的横行区域
    //mat = mat(rect_valid);
    Mat mat = desc.clone();
    Mat mat_org = desc.clone();
    QString examNo;
    //rectangle(mat, Rect(0, 0, mat.cols, mat.rows), Scalar(255), 1);
    int image_height = mat.rows, image_width = mat.cols;

    mat = imgtool::gamma(mat, 3);
    normalize(mat, mat, 0, 255, NORM_MINMAX, CV_8UC1);

    double grey = imgtool::gray_empty(mat);
    if (grey < 0.2)
    {
        cout << "Your identity area is empty, the measurement is:" << to_string(grey) ;
        return "";
    }


    //只用灰度信息，把选项选出来
    Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
    morphologyEx(mat, mat, MORPH_OPEN, element);
    morphologyEx(mat, mat, MORPH_DILATE, element);

    //假装服务器转了学生考号位数过来
    //int number_id = 9;


    Mat mat_neg = ~mat.clone();
    Mat mat_org_neg = ~mat_org.clone();
    //vector<vector<Rect>> vecs(10);
    vector<vector<int>> fillings(10);
    //vector<vector<int>> fillings_org(10);
    Mat fillings_mat(10, number_id, CV_32SC1, Scalar(0));
    double step_y = image_height *1.0 / 10, step_x = image_width * 1.0 / number_id;
    for (int i = 0; i < 10; i++) {
        int*p = fillings_mat.ptr<int>(i);
        for (int j = 0; j < number_id; j++) {
            Rect tmp_rect = Rect(Point2d(j*step_x, i*step_y), Point2d((j + 1)*step_x, (i + 1)*step_y));
            //vecs[i].push_back(tmp_rect);
            int filling = cv::sum(mat_neg(tmp_rect))[0];
            //int filling_org = cv::sum(mat_org_neg(tmp_rect))[0];
            fillings[i].push_back(filling);
            //fillings_org[i].push_back(filling_org);
            p[j] = filling;
            ////显示rect
            //rectangle(mat, tmp_rect, Scalar(125), 1);
            //waitKey(1);
        }
    }

    //对tr_fillings进行转置
    vector<vector<int>> tr_fillings(fillings[0].size(), vector<int>(fillings.size()));
    for (size_t i = 0; i < fillings.size(); ++i)
        for (size_t j = 0; j < fillings[0].size(); ++j)
            tr_fillings[j][i] = fillings[i][j];


    Mat fillings_mat_gray;
    normalize(fillings_mat, fillings_mat_gray, 255, 0, NORM_INF, CV_8UC1);
    Mat fillings_mat_bin;
    threshold(fillings_mat_gray, fillings_mat_bin, 0, 255, THRESH_OTSU);

    vector<int> identifications;
    Mat tr_fillings_mat_bin = fillings_mat_bin.t();
    bool missed = false, more = false;
    for (int i = 0; i < tr_fillings_mat_bin.rows; i++) {
        uchar*puchar = tr_fillings_mat_bin.ptr<uchar>(i);

        vector<int> hit(10, 0);

        for (int j = 0; j < tr_fillings_mat_bin.cols; j++) {
            if (puchar[j] == 255)
                hit[j] = 1;
        }
        int total_hit = std::accumulate(hit.begin(), hit.end(), 0);
        int total_hit_count = 0;
        //在一个数字漏掉时候的处理
        if (total_hit == 0) {
            total_hit_count++;
            if (total_hit_count < 2) {
                auto maxit = std::max_element(tr_fillings[i].begin(), tr_fillings[i].end());
                int second_largest = 0;
                int second_index = 0;
                for (int index = 0; index < tr_fillings[i].size(); index++) {
                    if (tr_fillings[i][index] > second_largest) {
                        if (tr_fillings[i][index] == *maxit) {
                            continue;
                        }
                        second_largest = tr_fillings[i][index];
                        second_index = index;
                    }
                }
                double ratio = (second_largest * 1.0) / (*maxit * 1.0);
                if (ratio < 0.8) {
                    int max_pos = std::distance(tr_fillings[i].begin(), max_element(tr_fillings[i].begin(), tr_fillings[i].end()));
                    identifications.push_back(max_pos);

                }

            }
            else
            {
                return "Missed one or more digtals maybe!";
            }
        }
        else if (total_hit == 1) {
            identifications.push_back(std::distance(hit.begin(), find(hit.begin(), hit.end(), 1)));
            //cout << std::distance(hit.begin(), find(hit.begin(), hit.end(), 1)) << endl;
        }
        else {
            //这是有两个或两个以上的选项被填涂的情况
            vector<int> shoot;
            for (int index = 0; index < hit.size(); index++)
            {
                if (hit[index] == 1)
                {
                    shoot.push_back(tr_fillings[i][index]);
                }
            }
            auto it = minmax_element(shoot.begin(), shoot.end());
            if ((*it.first * 1.0) / (*it.second * 1.0) < 0.9) {
                int max_pos = distance(tr_fillings[i].begin(), max_element(tr_fillings[i].begin(), tr_fillings[i].end()));
                identifications.push_back(max_pos);
                //cout << "max:" << max_pos << endl;
                //waitKey(1);
            }
            else
            {
                return "Choosed two digtals in one column!";
            }
        }
    }

    for (int i : identifications)
    {
        examNo.append(to_string(i));
    }

    return examNo;
}
