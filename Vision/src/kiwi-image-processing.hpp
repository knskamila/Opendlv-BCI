#ifndef KIWI-IMAGE-PROCESSING_HPP_INCLUDED
#define KIWI-IMAGE-PROCESSING_HPP_INCLUDED

#include <opencv2/opencv.hpp>
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
#include <opencv2/core.hpp>

#include <iostream>
#include <string>

#define GOAL_R 99
#define GOAL_G 63
#define GOAL_B 42

#define c_1 cv::Point(285,364)
#define c_2 cv::Point(347,400)

bool isWithin(cv::Scalar, cv::Scalar, cv::Scalar) noexcept;
cv::Mat cleanMask(cv::Mat) noexcept;
void balance_white(cv::Mat) noexcept;
void increase_brightness(cv::Mat) noexcept;
void normalize_t(cv::Mat, cv::Scalar, cv::Point, cv::Point);
void normalize_t(cv::Mat, cv::Scalar, float);
void normalize_t(cv::Mat);
void normalize_image(cv::Mat, cv::Mat, cv::Scalar, cv::Scalar, float) noexcept;
cv::Mat add_markers(cv::Mat) noexcept;
cv::Mat create_border_image(int, int);


#endif // KIWI-IMAGE-PROCESSING_HPP_INCLUDED
