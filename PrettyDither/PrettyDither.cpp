// PrettyDither.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <opencv2\opencv.hpp>
#include <opencv2\videoio.hpp>
#include <opencv2\highgui.hpp>

#include <iostream>
#include <tuple>
#include <string>

using namespace std;
using namespace cv;

tuple<uchar, uchar, float> closestColorsBW(uchar input);
tuple<Vec3b, Vec3b, float> closestColors(Vec3b in, Mat& palette);
Mat& ditherBW(Mat& I, Mat& index);
void ditherMultiBW(Mat& I, int i, int j, Mat& index);
void ditherMultiColor(Mat& I, int i, int j, Mat& index, Mat& palette);
void markEHColor(Mat& I, Vec3b color = Vec3b(0, 255, 0));

int main(int argc, char** argv)
{

#pragma region Palette
	//PICO8 palette
	Mat palette = Mat_<Vec3b>(1, 16);
	palette.at<Vec3b>(0, 0) = Vec3b(0, 0, 0);
	palette.at<Vec3b>(0, 1) = Vec3b(29, 43, 83);
	palette.at<Vec3b>(0, 2) = Vec3b(126, 37, 83);
	palette.at<Vec3b>(0, 3) = Vec3b(0, 135, 81);
	palette.at<Vec3b>(0, 4) = Vec3b(171, 82, 54);
	palette.at<Vec3b>(0, 5) = Vec3b(95, 87, 79);
	palette.at<Vec3b>(0, 6) = Vec3b(194, 195, 199);
	palette.at<Vec3b>(0, 7) = Vec3b(255, 241, 232);
	palette.at<Vec3b>(0, 8) = Vec3b(255, 0, 77);
	palette.at<Vec3b>(0, 9) = Vec3b(255, 163, 0);
	palette.at<Vec3b>(0, 10) = Vec3b(255, 236, 39);
	palette.at<Vec3b>(0, 11) = Vec3b(0, 228, 54);
	palette.at<Vec3b>(0, 12) = Vec3b(41, 173, 255);
	palette.at<Vec3b>(0, 13) = Vec3b(131, 118, 156);
	palette.at<Vec3b>(0, 14) = Vec3b(255, 119, 168);
	palette.at<Vec3b>(0, 15) = Vec3b(255, 204, 170);

	cvtColor(palette, palette, COLOR_RGB2BGR);
#pragma endregion

	//Dithering pattern matrix
	Mat index = (Mat_<float>(4, 4) <<
		0, 8, 2, 10,
		12, 4, 14, 6,
		3, 11, 1, 9,
		15, 7, 13, 5);

	MatIterator_<float> it, end;
	for (it = index.begin<float>(), end = index.end<float>(); it != end; it++)
	{
		*it = *it / 16;
	}

#pragma region Video
	VideoCapture video;
	video.open(2); // RealSense RGB

	namedWindow("Out", WINDOW_AUTOSIZE);

	Mat frame1, frame2;
	vector<Mat> channels;
	double time;
	String text;
	bool eqHist = false, showTimer = false, mainFlag = true;

	while (mainFlag && video.grab())
	{
		video.retrieve(frame1);

		if (eqHist)
		{
			// Equalize histogram only for Y (luminance)
			cvtColor(frame1, frame2, COLOR_BGR2YCrCb);
			split(frame2, channels);
			equalizeHist(channels[0], channels[0]);

			merge(channels, frame2);
			cvtColor(frame2, frame1, COLOR_YCrCb2BGR);
		}

		pyrDown(frame1, frame2);

		//cvtColor(frame1, frame2, COLOR_BGR2GRAY);
		//ditherBW(frame2, index);

		time = (double)getTickCount();

		parallel_for_(Range(0, frame2.rows*frame2.cols), [&](const Range& range) {
			for (int r = range.start; r < range.end; r++)
			{
				int i = r / frame2.cols;
				int j = r % frame2.cols;

				ditherMultiColor(frame2, i, j, index, palette);
			}
		});

		time = ((double)getTickCount() - time) * 1000 / getTickFrequency();

		cv::flip(frame2, frame1, 1);

		if (eqHist)
		{
			markEHColor(frame1);
		}

		cv::resize(frame1, frame2, Size(0, 0), 3.0, 3.0, INTER_NEAREST);

		if (showTimer)
		{
			text = to_string(time) + " ms";
			putText(frame2, text, Point(0, frame2.rows - 16), FONT_HERSHEY_SIMPLEX, 4, Scalar(255, 255, 255), 4, LINE_AA);
		}

		cv::imshow("Out", frame2);

		int key = waitKey(1);
		switch (key)
		{
			case 27: //ESC key
				mainFlag = false;
				break;

			case 'h':
				eqHist = !eqHist;
				break;

			case 't':
				showTimer = !showTimer;
				break;
		}
	}
#pragma endregion

	return 0;
}

///<summary>Finds if black or white is closer and relative distance</summary>
///<returns>Closest color, second closest color, distance to first</returns>
tuple<uchar, uchar, float> closestColorsBW(uchar input)
{
	if (input <= 127)
	{
		return tuple<uchar, uchar, float>(0, 255, (float)input / 255.0);
	}
	else
	{
		return tuple<uchar, uchar, float>(255, 0, 1 - (float)input / 255.0);
	}
}

///<summary>Finds closest colors in a palette</summary>
///<returns>Closest color, second closest color, distance to first</returns>
tuple<Vec3b, Vec3b, float> closestColors(Vec3b in, Mat & palette)
{
	int i1, i2; //index in palette
	int d1, d2, d; //distance to color squared

	i1 = 0;
	Vec3b pal = palette.at<Vec3b>(0, 0);
	d1 = (in[0] - pal[0])*(in[0] - pal[0]) + (in[1] - pal[1])*(in[1] - pal[1]) + (in[2] - pal[2])*(in[2] - pal[2]);

	i2 = 1;
	pal = palette.at<Vec3b>(0, 1);
	d2 = (in[0] - pal[0])*(in[0] - pal[0]) + (in[1] - pal[1])*(in[1] - pal[1]) + (in[2] - pal[2])*(in[2] - pal[2]);

	for (int i = 2; i < 16; i++)
	{
		pal = palette.at<Vec3b>(0, i);
		d = (in[0] - pal[0])*(in[0] - pal[0]) + (in[1] - pal[1])*(in[1] - pal[1]) + (in[2] - pal[2])*(in[2] - pal[2]);
		if (d < d1)
		{
			d2 = d1;
			i2 = i1;
			d1 = d;
			i1 = i;
		}
		else if (d < d2)
		{
			d2 = d;
			i2 = i;
		}
	}

	float d1true = (float)sqrt(d1);
	float d2true = (float)sqrt(d2);

	return tuple<Vec3b, Vec3b, float>(palette.at<Vec3b>(0, i1), palette.at<Vec3b>(0, i2), d1true / (d1true + d2true));
}

///<summary>Dithers the whole image to black and white</summary>
Mat& ditherBW(Mat& I, Mat& index)
{
	uchar closest, second;
	float dist;

	const int channels = I.channels();
	switch (channels)
	{
		case 1:
		{
			for (int i = 0; i < I.rows; ++i)
				for (int j = 0; j < I.cols; ++j)
				{
					tie(closest, second, dist) = closestColorsBW(I.at<uchar>(i, j));

					if (dist < index.at<float>(i % 4, j % 4))
						I.at<uchar>(i, j) = closest;
					else
						I.at<uchar>(i, j) = second;
				}
			break;
		}
		default:
			break;
	}

	return I;
}

///<summary>Replaces a single pixel with black or white</summary>
void ditherMultiBW(Mat & I, int i, int j, Mat & index)
{
	uchar closest, second;
	float dist;

	tie(closest, second, dist) = closestColorsBW(I.at<uchar>(i, j));

	if (dist < index.at<float>(i % 4, j % 4))
		I.at<uchar>(i, j) = closest;
	else
		I.at<uchar>(i, j) = second;
}

///<summary>Replaces a single pixel with a color from palette</summary>
void ditherMultiColor(Mat & I, int i, int j, Mat & index, Mat& palette)
{
	Vec3b closest, second;
	float dist;

	tie(closest, second, dist) = closestColors(I.at<Vec3b>(i, j), palette);

	if (dist < index.at<float>(i % 4, j % 4))
		I.at<Vec3b>(i, j) = closest;
	else
		I.at<Vec3b>(i, j) = second;
}

void markEHColor(Mat & I, Vec3b color)
{
	string shape = \
		"xxx.x.x\n"\
		"x...x.x\n"\
		"xxx.xxx\n"\
		"x...x.x\n"\
		"xxx.x.x\n";

	int offsetX = 1, offsetY = 1, len = shape.length(), x = 0, y = 0;

	for (int i = 0; i < len; i++)
	{
		switch (shape[i])
		{
			case 'x':
				I.at<Vec3b>(offsetY + y, offsetX + x) = color;
				x++;
				break;

			case '\n':
				y++;
				x = 0;
				break;

			default:
				x++;
				break;
		}
	}
}
