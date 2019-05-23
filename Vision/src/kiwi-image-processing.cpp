/*
 * Copyright (C) 2019  Kamila Kowalska
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kiwi-image-processing.hpp"

//using namespace cv;

cv::Mat create_border_image(int width, int height)
{
    cv::Mat background(height, width, CV_8UC4, cv::Scalar(200,200,200));
    cv::Mat out = add_markers(background);
    return out;
}

cv::Mat add_markers(cv::Mat mat) noexcept
{
    uint16_t width = mat.cols;
    uint16_t height = mat.rows;

    cv::Mat marker1(5, 5, CV_8UC1, cv::Scalar(0));
    cv::Mat marker2(5, 5, CV_8UC1, cv::Scalar(0));
    cv::Mat marker3(5, 5, CV_8UC1, cv::Scalar(0));
    cv::Mat marker4(5, 5, CV_8UC1, cv::Scalar(0));
    cv::Mat marker1_r, marker2_r, marker3_r, marker4_r;

    marker1.at<uchar>(3,1) = 255;
    marker1.at<uchar>(1,2) = 255;

    marker2.at<uchar>(2,1) = 255;
    marker2.at<uchar>(2,3) = 255;
    marker2.at<uchar>(3,1) = 255;

    marker3.at<uchar>(3,1) = 255;
    marker3.at<uchar>(2,2) = 255;
    marker3.at<uchar>(3,2) = 255;

    marker4.at<uchar>(1,1) = 255;
    marker4.at<uchar>(3,3) = 255;
    marker4.at<uchar>(1,3) = 255;
    marker4.at<uchar>(3,2) = 255;

    cv::cvtColor(marker1, marker1, cv::COLOR_GRAY2BGRA);
    cv::cvtColor(marker2, marker2, cv::COLOR_GRAY2BGRA);
    cv::cvtColor(marker3, marker3, cv::COLOR_GRAY2BGRA);
    cv::cvtColor(marker4, marker4, cv::COLOR_GRAY2BGRA);

    cv::resize(marker1, marker1_r, cv::Size(100,100), 0, 0, cv::INTER_NEAREST);
    cv::resize(marker2, marker2_r, cv::Size(100,100), 0, 0, cv::INTER_NEAREST);
    cv::resize(marker3, marker3_r, cv::Size(100,100), 0, 0, cv::INTER_NEAREST);
    cv::resize(marker4, marker4_r, cv::Size(100,100), 0, 0, cv::INTER_NEAREST);

    cv::Mat background;
    copyMakeBorder(mat, background, 0, 0, 100, 100, cv::BORDER_CONSTANT, cv::Scalar(200,200,200) );
    marker1_r.copyTo(background(cv::Rect(0,0,100,100)));
    marker2_r.copyTo(background(cv::Rect(0, height-100, 100,100)));
    marker3_r.copyTo(background(cv::Rect(width+100, 0,100,100)));
    marker4_r.copyTo(background(cv::Rect(width+100, height-100, 100,100)));

    return background;
}

cv::Mat cleanMask(cv::Mat input) noexcept
{
    cv::Mat mask, out;
    cv::threshold(input, mask, 220.0, 255.0, cv::THRESH_BINARY);

    int erosion_size = 4;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT,
	cv::Size(2 * erosion_size + 1, 2 * erosion_size + 1),
	cv::Point(erosion_size, erosion_size));

    // Apply erosion, dilation on the image
    cv::erode(mask, out, element);
    cv::dilate(out, out, element);
    return out;
}

 void increase_brightness(cv::Mat mat) noexcept
 {
    for (int y = 0; y < mat.rows; ++y) {
        uchar* ptr = mat.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            int B = ptr[x * 3];
            int G = ptr[x * 3 + 1];
            int R = ptr[x * 3 + 2];

            float scale_B = 1 - (abs(B - 127)/127.0f);
            float scale_G = 1 - (abs(G - 127)/127.0f);
            float scale_R = 1 - (abs(R - 127)/127.0f);

            R += static_cast<int>(10*scale_R);
            G += static_cast<int>(10*scale_G);
            B += static_cast<int>(10*scale_B);

            if (R > 255) R = 255;
            if (G > 255) G = 255;
            if (B > 255) B = 255;

            ptr[x * 3] = static_cast<uchar>(B);
            ptr[x * 3 + 1] = static_cast<uchar>(G);
            ptr[x * 3 + 2] = static_cast<uchar>(R);
        }
    }
 }

void balance_white(cv::Mat mat) noexcept
{
	double discard_ratio = 0.05;
	int hists[3][256];
	memset(hists, 0, 3 * 256 * sizeof(int));

	for (int y = 0; y < mat.rows; ++y) {
		uchar* ptr = mat.ptr<uchar>(y);
		for (int x = 0; x < mat.cols; ++x) {
			for (int j = 0; j < 3; ++j) {
				hists[j][ptr[x * 3 + j]] += 1;
			}
		}
	}

	// cumulative histogram
	int total = mat.cols*mat.rows;
	int vmin[3], vmax[3];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 255; ++j) {
			hists[i][j + 1] += hists[i][j];
		}
		vmin[i] = 0;
		vmax[i] = 255;
		while (hists[i][vmin[i]] < discard_ratio * total)
			vmin[i] += 1;
		while (hists[i][vmax[i]] > (1 - discard_ratio) * total)
			vmax[i] -= 1;
		if (vmax[i] < 255 - 1)
			vmax[i] += 1;
	}


	for (int y = 0; y < mat.rows; ++y) {
		uchar* ptr = mat.ptr<uchar>(y);
		for (int x = 0; x < mat.cols; ++x) {
			for (int j = 0; j < 3; ++j) {
				int val = ptr[x * 3 + j];
				if (val < vmin[j])
					val = vmin[j];
				if (val > vmax[j])
					val = vmax[j];
				ptr[x * 3 + j] = static_cast<uchar>((val - vmin[j]) * 255.0 / (vmax[j] - vmin[j]));
			}
		}
	}
}

void normalize_image(cv::Mat in, cv::Mat out, cv::Scalar values, cv::Scalar goal, float strength) noexcept
{
    cv::Mat mat = in.clone();

    int difference_B = static_cast<int>(values(0) - goal(0));
    int difference_G = static_cast<int>(values(1) - goal(1));
    int difference_R = static_cast<int>(values(2) - goal(2));

    //std::cout << difference_B + difference_G + difference_R  << std::endl;
    if (difference_B + difference_G + difference_R < -60)
    {
        increase_brightness(mat);
        balance_white(mat);
        normalize_t(mat);
        return;
    }

    for (int y = 0; y < mat.rows; ++y) {
        uchar* ptr = mat.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            int B = ptr[x * 3];
            int G = ptr[x * 3 + 1];
            int R = ptr[x * 3 + 2];

            float scale_B = 1 - (abs(B - 127)/127.0f);
            float scale_G = 1 - (abs(G - 127)/127.0f);
            float scale_R = 1 - (abs(R - 127)/127.0f);

            R -= static_cast<int>(difference_R*scale_R*strength);
            G -= static_cast<int>(difference_G*scale_G*strength);
            B -= static_cast<int>(difference_B*scale_B*strength);

            if (R > 255) R = 255;
            if (G > 255) G = 255;
            if (B > 255) B = 255;

            if (R < 0) R = 0;
            if (G < 0) G = 0;
            if (B < 0) B = 0;

            ptr[x * 3] = static_cast<uchar>(B);
            ptr[x * 3 + 1] = static_cast<uchar>(G);
            ptr[x * 3 + 2] = static_cast<uchar>(R);
        }
    }

    out = mat.clone();
}

void normalize_t(cv::Mat original, cv::Scalar goalValues, cv::Point c1, cv::Point c2)
{
/*
    Provided the goal values, the image will be normalized by comparing them
    to the average color of a selected region.
*/
    cv::Rect cutoutRect(c1, c2);
    cv::Mat mat = original(cutoutRect);

    long total[3];
    total[0] = 0;
    total[1] = 0;
    total[2] = 0;

    int removed = 0;

    for (int y = 0; y < mat.rows; ++y) {
        uchar* ptr = mat.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            if(ptr[x * 3]+ptr[x * 3 + 1]+ptr[x * 3 + 2] < 9) removed++; //if black
            else
            {
                total[0] += ptr[x * 3 + 0];
                total[1] += ptr[x * 3 + 1];
                total[2] += ptr[x * 3 + 2];
            }
        }
    }

    int R = total[2]/(mat.rows*mat.cols - removed);
    int G = total[1]/(mat.rows*mat.cols - removed);
    int B = total[0]/(mat.rows*mat.cols - removed);

    float rat = (float)removed/(mat.rows*mat.cols); //ratio of discarded pixels

    if(rat > 0.9)
    {
        std::cerr << "Image too dark/wrong area selected." << std::endl;
        return;
    }

    float strength = 0.8f + (1-2*rat);
    normalize_image(original, original, cv::Scalar(B,G,R), goalValues, strength);
}

void normalize_t(cv::Mat original)
{
    /*
        Normalize the picture towards default values.
    */
    cv::Rect cutoutRect(c_1, c_2);
    cv::Mat mat = original(cutoutRect);

    long total[3];
    total[0] = 0;
    total[1] = 0;
    total[2] = 0;

    int removed = 0;

    for (int y = 0; y < mat.rows; ++y) {
        uchar* ptr = mat.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            if(ptr[x * 3]+ptr[x * 3 + 1]+ptr[x * 3 + 2] < 9) removed++; //if black
            else
            {
                total[0] += ptr[x * 3 + 0];
                total[1] += ptr[x * 3 + 1];
                total[2] += ptr[x * 3 + 2];
            }
        }
    }

    int R = total[2]/(mat.rows*mat.cols - removed);
    int G = total[1]/(mat.rows*mat.cols - removed);
    int B = total[0]/(mat.rows*mat.cols - removed);

    float rat = (float)removed/(mat.rows*mat.cols);
    float strength = 0.8f + (1-2*rat);

    //std::cout << "strength: " << strength << std::endl;
    //std::cout << "pixels not considered: " << rat*100 << "%. " << std::endl;

    normalize_image(original, original, cv::Scalar(B,G,R), cv::Scalar(GOAL_B, GOAL_G, GOAL_R), strength);
}

void normalize_t(cv::Mat mat, cv::Scalar difference, float strength)
{
    /*
        Normalize the picture towards known values with
        a chosen multiplier parameter.
    */
    cv::Mat out = mat.clone();

    int difference_R = static_cast<int>(difference(2));
    int difference_G = static_cast<int>(difference(1));
    int difference_B = static_cast<int>(difference(0));

    for (int y = 0; y < mat.rows; ++y) {
        uchar* ptr = mat.ptr<uchar>(y);
        uchar* ptr_out = out.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            int B = ptr[x * 3];
            int G = ptr[x * 3 + 1];
            int R = ptr[x * 3 + 2];

            float scale_B = 1 - (abs(B - 127)/127.0f);
            float scale_G = 1 - (abs(G - 127)/127.0f);
            float scale_R = 1 - (abs(R - 127)/127.0f);

            R -= static_cast<int>(difference_R*scale_R*strength);
            G -= static_cast<int>(difference_G*scale_G*strength);
            B -= static_cast<int>(difference_B*scale_B*strength);

            if (R > 255) R = 255;
            if (G > 255) G = 255;
            if (B > 255) B = 255;

            if (R < 0) R = 0;
            if (G < 0) G = 0;
            if (B < 0) B = 0;

            ptr_out[x * 3] = static_cast<uchar>(B);
            ptr_out[x * 3 + 1] = static_cast<uchar>(G);
            ptr_out[x * 3 + 2] = static_cast<uchar>(R);
        }
    }

    mat = out.clone();
}

bool isWithin(cv::Scalar hsv, cv::Scalar hsv_min, cv::Scalar hsv_max) noexcept
{
    if (hsv[0] < hsv_min[0]) return false;
    if (hsv[1] < hsv_min[1]) return false;
    if (hsv[2] < hsv_min[2]) return false;

    if (hsv[0] > hsv_max[0]) return false;
    if (hsv[1] > hsv_max[1]) return false;
    if (hsv[2] > hsv_max[2]) return false;

    return true;
}
