// StereoD3D.cpp : Defines the entry point for the application.
//


#include "common.h"
#include <fstream>
#include <ctime>
#include <opencv2\core\ocl.hpp>

#include <io.h>
#include <cstdlib>

#include <cmath>


//-----------------------------------------------------------------------------
//Define
//-----------------------------------------------------------------------------

#define WEBCAM_MODE false
#define WEBCAM_NUMBER 1

#define FRAMESKIP_NO 0

#define FILESAVE_MODE_EN false

#define FAST_MODE true

#define MORPH_STRUCT_SIZE_X 1
#define MORPH_STRUCT_SIZE_Y 2

#define READ_VIDEO_FOLDER "Input/"
#define READ_VIDEO_NAME "Jgag.avi"

#define TARGET_FPS 30.0

#define LEFT_WALK -1
#define SAME_WALK 0
#define RIGHT_WALK 1

#define WALK_DIRECTION_THRESHOLD 5


#define OPENCL_SUPPORT 1
#define MASK_MODE 0
#define WINDOWS_MODE 1
#define DEBUG_MODE 0
#define DEMO_MODE 1

#define MAX_BOX_X 40
#define MAX_BOX_Y 80

#define toggle(a) !a

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


//OpenCV Global Variables
char* videoFilename = READ_VIDEO_NAME;
char* SaveFilename = "Result.avi";
cv::VideoCapture capture;
cv::VideoCapture camera;

cv::Mat Current_Frame;
cv::Mat BackgroundMOG;
cv::Mat Silhouette_Final;
cv::Mat ContourImages;

cv::Mat Mask_Output;



cv::UMat CL_Current_Frame;
cv::UMat CL_Background_Frame;

int Rows, Cols;
int frame_no = 0;

int Walk_Direction = 0;
cv::Point Previous_Point;

LARGE_INTEGER Frequency;
LARGE_INTEGER Start_counter;
LARGE_INTEGER End_counter;

LARGE_INTEGER Lag1, Lag2, Lag3, Lag4, Lag5;

double frame_difference = 0;

int Direction_Tally[2] = { 0 };


using namespace std;
using namespace cv;

void hahaha(int a, void* b)
{

}


void InitOpenCVModules() //OPENCV �����͵��� �ʱ�ȭ
{
	/*----------------------*/
	//
	//
	//
	//
	/*-----------------------*/
	

	if (WEBCAM_MODE)
	{
		camera.open(WEBCAM_NUMBER);

		if (!camera.isOpened())  //�ҽ� ���� ����üũ
		{
			//error in opening the video input
			cerr << "Unable to open Camera Stream" << endl;
			exit(EXIT_FAILURE);
		}


		camera >> Current_Frame; //ī�޶� �ҽ����� Current Frame���� �����͸� �־��ش�.
	}

	else
	{
		ostringstream FilePathVideo;
		FilePathVideo  << READ_VIDEO_FOLDER << videoFilename;
		capture = VideoCapture(FilePathVideo.str()); //���Ͽ��� ���� ���� �Ҵ����ش�.

		if (!capture.isOpened())  //�ҽ� ���� ����üũ
		{
			//error in opening the video input
			cerr << "Unable to open video file: " << videoFilename << endl;
			exit(EXIT_FAILURE);
		}

		if (!capture.read(Current_Frame))
		{
			cerr << "Unable to read next frame." << endl;
			cerr << "Exiting..." << endl;
			exit(EXIT_FAILURE);
		}
	}

	imwrite("Reference.bmp", Current_Frame);


		//������ �ѹ��� Ȯ���� ��ģ��
		Rows = Current_Frame.rows;
		Cols = Current_Frame.cols;
		Silhouette_Final = Mat::zeros(Rows, Cols, CV_8UC1);

		Mask_Output = Mat::zeros(Rows, Cols, CV_8UC3);

		ContourImages = Mat::zeros(Rows, 2*Cols, CV_8UC3);

}


void InitWindows()
{
	InitContourWindow();
}

int main(int argc, char *argv[])
{
	

	//--------------------------------------
	//�ʱ�ȭ
	//--------------------------------------

	//OPENCV initialize
	InitOpenCVModules();

	//Windows initialize
	//InitWindows();

	//Variable Initialize
	Mat Morph_Element = getStructuringElement(MORPH_RECT, Size(2 * MORPH_STRUCT_SIZE_X + 1, 2 * MORPH_STRUCT_SIZE_Y + 1), Point(MORPH_STRUCT_SIZE_X, MORPH_STRUCT_SIZE_Y));
	
	vector<vector<Point>> VectorPointer;
	Mat ContourData;


	QueryPerformanceFrequency(&Frequency);

	frame_difference =  (double)Frequency.QuadPart / TARGET_FPS;

	QueryPerformanceCounter(&Start_counter); //�ʱ� Ÿ�̸�

	double frame_rate = 0.0;

	//-------------------------------------------------------------
	// ������ ó�� ���� ������
	//--------------------------------------------------------------

	while (1) //Frame Processing Loop Start
	{
		frame_no++; //������ �ѹ� ���

		QueryPerformanceCounter(&End_counter);

		//Framerate ����ȭ Start
		if ((End_counter.QuadPart - Start_counter.QuadPart) < frame_difference) //Framerate ������
		{
			continue;
		}

		else
		{
			frame_rate =  (double)Frequency.QuadPart / (double)(End_counter.QuadPart - Start_counter.QuadPart);
			QueryPerformanceCounter(&Start_counter);
		}
		//Framerate ����ȭ End



		if (WEBCAM_MODE) //��ķ ������� �ƴ����� �Ǻ��� ���ǹ�
		{
			camera >> Current_Frame;
		}

		else
		{
			for (int frameskip = 0; frameskip <= FRAMESKIP_NO; frameskip++)
			{
				if (!capture.read(Current_Frame))
				{
					cerr << "Unable to read next frame." << endl;
					cerr << "Exiting..." << endl;
					exit(EXIT_FAILURE);
				}
			}
		}

		
#if OPENCL_SUPPORT
		CL_Current_Frame = Current_Frame.getUMat(ACCESS_READ);
		FastBGSubtract();
#else
		FastBGSubtract_NonCL();
#endif
	
			

		static Mat Longest_Contour;

		ContourBasedFilter(&Longest_Contour, &Silhouette_Final);
		cv::morphologyEx(Longest_Contour, Longest_Contour, MORPH_DILATE, Morph_Element);
		


		//---------------------------------------------------------------//
		//     Preproccesing START (Feature Extraction)                  //
		//---------------------------------------------------------------//
		//variable ����
		Mat Contour_out_image_array;
		Mat Cutting_image_array;
		Mat Resampled_image_array;

		int i;
		int height = 0;   int width = 0;    int period = 0;
		double Bounding_box_ratio;

		vector<Point> contour_point_array;
		vector<Point> refer_point;
		vector<vector<Point> > Segment;

		if (!CheckEmpty(&Longest_Contour))
		{
		
			
			//Silhouette screen using contour length
			
			
			
			//Frame Process step
			Cutting_image_array = Cutting_silhouette_area(&Longest_Contour, &height, &width);
			Bounding_box_ratio = (double)height / (double)width;

			// Contour extraction
			Contour_out_image_array = Contour(&Cutting_image_array, &contour_point_array);

			// Resampling
			refer_point = Find_refer_point(contour_point_array);
			Segment = Resampling(&contour_point_array, &refer_point);
			Resampled_image_array = Draw_Resampling(Segment, Contour_out_image_array);

#if DEBUG_MODE

			resize(Contour_out_image_array, Contour_out_image_array, Size(2 * width, 2 * height), 0, 0, CV_INTER_NN);
			resize(Resampled_image_array, Resampled_image_array, Size(2 * width, 2 * height), 0, 0, CV_INTER_NN);

			//imshow("Cutting_image", Cutting_image_array);
			cv::imshow("Contour_image", Contour_out_image_array);
			cv::imshow("Resampling_image", Resampled_image_array);
#endif
		}

		//---------------------------------------------------------------//
		//     Preproccesing END (Feature Extraction)                    //
		//---------------------------------------------------------------//

		if (Direction_Tally[0] > WALK_DIRECTION_THRESHOLD)
			Walk_Direction = LEFT_WALK;
		
		if (Direction_Tally[1] > WALK_DIRECTION_THRESHOLD)
			Walk_Direction = RIGHT_WALK;
		
		//��� UI�� �÷�
		//cvtColor(Silhouette_Final, UI_Output, CV_GRAY2BGR);

#if MASK_MODE
		ImageMask(&Mask_Output, &Current_Frame, &Longest_Contour);
#else	
		Current_Frame.copyTo(Mask_Output);
#endif

		ostringstream FPS;

		//������ ����Ʈ ǥ��
		FPS << "FPS: " << frame_rate;
		cv::putText(Mask_Output, FPS.str(), Point(30, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255, 255, 255), 3, LINE_AA, false);
		cv::putText(Mask_Output, FPS.str(), Point(30, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(40, 40, 40), 2, LINE_AA, false);



		//�̹����� �����Ǵ� ����� rectangle ǥ��
		rectangle(Mask_Output, Previous_Point - maxSize / 2, Previous_Point + maxSize / 2, Scalar(128, 128, 128), 2, 8, 0);

		//�ȴ� ���⿡ ���� ǥ���� String�� �Է� �� �̹����� ǥ��
		if (Walk_Direction == LEFT_WALK)
			cv::putText(Mask_Output, "Left", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(0, 255, 0), 1, LINE_AA, false);
		else if (Walk_Direction == RIGHT_WALK)
			cv::putText(Mask_Output, "Right", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(255, 0, 0), 1, LINE_AA, false);
		else
			cv::putText(Mask_Output, "Stop", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(0, 0, 255), 1, LINE_AA, false);

		



		//������ ǥ��
		cv::putText(Mask_Output, "Team MMI-01", Point(Cols - 150, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255, 255, 255), 3, LINE_AA, false);
		cv::putText(Mask_Output, "Team MMI-01", Point(Cols - 150, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);



		//�ڽ��� ���ָ� ǥ��(�ν� ����� ���� ���� ���� ����)
#if DEMO_MODE
		cv::putText(Mask_Output, "Recognized as:", Point(Cols - 170, Rows - 60), FONT_HERSHEY_COMPLEX_SMALL, 0.9, Scalar(255, 255, 0), 1, LINE_AA, false);

		if ((maxP.y - minP.y) > 125)
		{
			cv::putText(Mask_Output, "Jae Sung Lee", Point(Cols - 170, Rows - 30), FONT_HERSHEY_COMPLEX_SMALL, 0.9, Scalar(255, 0, 255), 1, LINE_AA, false);
			cv::putText(Mask_Output, "Recognized ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(128, 255, 128), 3, LINE_AA, false);
			cv::putText(Mask_Output, "Recognized ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);
		}

		else
		{
			cv::putText(Mask_Output, "Searching.. ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(128, 128, 255), 3, LINE_AA, false);
			cv::putText(Mask_Output, "Searching.. ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);
		}
#endif
		//Demo End



		//���� ǥ��
		cv::putText(Mask_Output, "Press ESC to quit", Point(Cols / 2 - 80, Rows - 30), FONT_HERSHEY_PLAIN, 1.2, Scalar(255, 255, 255, 0.5), 1, LINE_AA, false);
		


		cv::imshow("Masked Final", Mask_Output);
				
		//â�� �˸´� ��ġ�� �̵�
		cv::moveWindow("Masked Final", 0, 0);

#if DEBUG_MODE
		cv::imshow("Input", Current_Frame);
		cv::moveWindow("Input", 0 + Cols, 0);

		//�Ƿ翧�� �߾ӿ� ��ġ�ϵ��� ���� ��ġ ����
		cv::moveWindow("Contour_image", 0 + (Cols - width)/2, 0 + Rows + 30);
		cv::moveWindow("Resampling_image", 0 + (3*Cols - width) / 2, 0 + Rows + 30);

#endif

		if (waitKey(1) == 27) //ESCŰ�� ���� 
			break;

		QueryPerformanceCounter(&End_counter);

	}
	



	return 0;
}