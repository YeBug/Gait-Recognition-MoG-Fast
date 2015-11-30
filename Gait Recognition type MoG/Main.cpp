// StereoD3D.cpp : Defines the entry point for the application.
//
#include "Global Variables.h"
#include "opencv_inc.h"

#include "common.h"
#include <fstream>
#include <ctime>

#include <io.h>
#include <cstdlib>

#include <cmath>

#include "Preprocess_PMS.h"


//setw��
#include <iomanip>

//-----------------------------------------------------------------------------
//Define
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//���α׷� ����
//
//WEBCAM_MODE		: config ������ �����ϴ� ��� �����ϴ� ��ķ ��� ������
//WEBCAM_NUMBER		: ����� ��ķ ��ȣ
//
//FRAMESKIP_ENABLE	: �������� target framerate�� �ȳ��� ���, ������ ��ŵ�� ų������
//
//MORPH_STRUCT_SIZE_X	: Longest Contour�� ����ϴ� morphology element�� ������ ����
//MORPH_STRUCT_SIZE_Y	:
//
//WALK_DIRECTION_THRESHOLD : �ȴ� ������ �����Ҷ�, ���⼺ ������ ��ġ��
//CONTOUR_CONDITION_THRESHOLD : ������ ���� ���� ������ ���̰� ������ ��, �󸶳� ���������� �Ǹ� ��ȿ�� �����ͷ� ó���� ���ΰ��� ������ ��
//
//MIN_DETECT_CONTOUR : ������ ������ ��ȿ�� ���� (dead zone) ����
//MAX_DETECT_CONTOUR :
//
//READ_VIDEO_FOLDER : config ������ ��� ���Ҷ� ���� ������ ��� 
//READ_VIDEO_NAME :
//-----------------------------------------------------------------------------

#define WEBCAM_MODE false
#define WEBCAM_NUMBER 1

#define FRAMESKIP_ENABLE false


#define MORPH_STRUCT_SIZE_X 3
#define MORPH_STRUCT_SIZE_Y 3

#define TARGET_FPS 30.0

#define WALK_DIRECTION_THRESHOLD 5
#define CONTOUR_CONDITION_THRESHOLD 0

#define MIN_DETECT_CONTOUR 300
#define MAX_DETECT_CONTOUR 2000

#define READ_VIDEO_FOLDER "Input/"
#define READ_VIDEO_NAME "Jgag.avi"


//-----------------------------------------------------------------------------
//�����Ϸ� ����
//
//OPENCL_SUPPORT: BGSubtraction�� OpenCL ��뿩��
//MASK_MODE		: ��� �Ƿ翧�� �Է� Mask�� ����Ͽ�, �߶��� �Է� �̹����� ������� ���
//DEBUG_MODE	: ���â�� Debug�� �Ķ���͵��� ǥ��
//DEMO_MODE		: ��� ����� �̸� ������ ������ ǥ��(UI Demo�� �ɼ�)
//
//INPUT_IS_SILHOUETTE : �Է� ������ �̹� �Ƿ翧�� ���, BGSubtraction�� ����
//
//USE_CONFIG_FILE: ������ config.txt���� �о������ ���θ� ����
//
//-----------------------------------------------------------------------------

#define OPENCL_SUPPORT 0
#define MASK_MODE 0
#define DEBUG_MODE 1
#define DEMO_MODE 1
#define USE_GUI_BUTTON 1

#define INPUT_IS_SILHOUETTE 0

#define USE_CONFIG_FILE 1

#define FILESAVE_MODE_EN 0

//-----------------------------------------------------------------------------
// ���α׷� ���� ��� define
//-----------------------------------------------------------------------------
#define END_OK 0
#define END_ERROR 1

#define LEFT_WALK -1
#define SAME_WALK 0
#define RIGHT_WALK 1

#define toggle(a) !a



//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


//OpenCV Global Variables
char* videoFilename = READ_VIDEO_NAME;

cv::VideoCapture capture;
cv::VideoCapture camera;

cv::Mat Current_Frame;
cv::Mat BackgroundMOG;
cv::Mat Silhouette_Final;
cv::Mat ContourImages;

cv::Mat Mask_Output;

//OpenCL ���� ����ϴ� ����
cv::UMat CL_Current_Frame;
cv::UMat CL_Background_Frame;


int Rows, Cols; //������ ������
int frame_no = 0; //������ ���൵

int Walk_Direction = 0; //���� ���� 
int Direction_Tally[2] = { 0 }; //���� ���� ��Ͽ� ǥ
cv::Point Previous_Point; //���� �ڽ��� ���� ��ǥ �����

//������ ����Ʈ ���� Tick Counter ���� 
LARGE_INTEGER Frequency;
LARGE_INTEGER Start_counter;
LARGE_INTEGER End_counter;

LARGE_INTEGER Lag1, Lag2, Lag3, Lag4, Lag5;

double frame_difference = 0;


//config ���� ���� ����
string Train_data_path;
string Video_File_Path;
bool Webcam_mode = WEBCAM_MODE;
int Webcam_number = WEBCAM_NUMBER;
bool Loop_Mode = false;
//

//�νİ�� flag ����
bool Recognition_Processing = false;
bool Recognition_Success = false;
int Contour_condition = 0;


//Button Click Functions
bool PlayCommand = true;
bool ResetCommand = false;
bool ExitCommand = false;
bool StartRecognition = false;


//StringStream
string SavePath1, SavePath2, SavePath3, SavePath4;


using namespace std;
using namespace cv;


enum Config_type{
	TRAIN_PATH = 1,
	VIDEO_PATH,
	WEBCAM,
	WEBCAM_NO,
	LOOP_MODE,
}; //config ���Ͽ�

string FindConfigurationString(string currentline, int* Type)
{
	
	//���� ����
	//configuration_data : ���Ͽ��� ��ȯ�Ǵ� configuration ������ �������� �����ϴ� ����
	//i: index ����, '='�� ��ġ�� ã�� configuration_data�� ���� string�� ¥�� ��ġ�� �����Ҷ� ���
	//data_index : =�� ��ġ�� �����ϴ� �뵵
	//type : ��� ���� ������ �˷��ִ� �뵵.

	string configuration_data;
	uint i, data_index;

	data_index = 0;
	*Type = 0;

	//��� ���������� �Ǻ�
	{
		if (currentline.find("training_data_path=") != string::npos)
			*Type = TRAIN_PATH;

		if (currentline.find("input_video_path=") != string::npos)
			*Type = VIDEO_PATH;

		if (currentline.find("webcam_mode=") != string::npos)
			*Type = WEBCAM;

		if (currentline.find("webcam_number=") != string::npos)
			*Type = WEBCAM_NO;

		if (currentline.find("loop_mode=") != string::npos)
			*Type = LOOP_MODE;
	}

	//Type�� ���������� ���� ������ �ƴϹǷ� ������ȯ
	if (*Type == 0)
		return "";


	for (i = 0; i < currentline.size(); i++)
		if (currentline.c_str()[i] == '=')
		{
			data_index = i;
			break;
		}
	
	//data �κи��� ��ȯ
	configuration_data = currentline.substr(data_index + 1, currentline.size() - 1);

	return configuration_data;
}

int ConfigurationFileRead() //�ɼ� ������ �о�, �ش��ϴ� ������ ��� �����ϴ� �Լ�. ����ÿ� ȣ��
{
	ifstream Input_Stream("Config.txt");

	if (!Input_Stream.is_open())
	{ 
		cout << "Config.txt not found" << endl;
		return END_ERROR;
	}

	string current_line;

	string data;
	int Type;

	while (!Input_Stream.eof()) //���� ������ �д´�.
	{
		getline(Input_Stream, current_line); //���� �д´�.

		data = FindConfigurationString(current_line, &Type);

		switch (Type) 
		{
		case 0:
			continue; //�����̹Ƿ�
		case TRAIN_PATH:
			Train_data_path = data;
			break;
		case VIDEO_PATH:
			Video_File_Path = data;
			break;
		case WEBCAM:
			if (data.find("true") != string::npos)
				Webcam_mode = true;
			else
				Webcam_mode = false;
			break;
		case WEBCAM_NO:
			Webcam_number = stoi(data.c_str());
			break;
		case LOOP_MODE:
			if (data.find("true") != string::npos)
				Loop_Mode = true;
			else
				Loop_Mode = false;
			break;
		default:
			continue;
		}


	}

	return END_OK;

}

void InitLocalVariables()
{

	//�νİ�� flag ����
	 Recognition_Processing = false;
	 Recognition_Success = false;

	frame_no = 0;
	 Walk_Direction = 0;
	 Contour_condition = 0;
}

void InitOpenCVModules() //OPENCV �����͵��� �ʱ�ȭ
{
	/*----------------------*/
	//
	//
	//
	//
	/*-----------------------*/
	

	if (Webcam_mode)
	{
		camera.open(Webcam_number);

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
#if USE_CONFIG_FILE
		capture = VideoCapture(Video_File_Path);
#else
		ostringstream FilePathVideo;
		FilePathVideo << READ_VIDEO_FOLDER << videoFilename;
		capture = VideoCapture(FilePathVideo.str()); //���Ͽ��� ���� ���� �Ҵ����ش�.
#endif

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

void InitSaveDirectories()
{
	//----------------------------------------------------
	//����׿� ������ ������ ���� ���丮�� ����� ���� �۾�
	//data������ �����ϰ� data �ؿ� ���ϵ��� �����Ѵ�.
	//
	//----------------------------------------------------
	ostringstream StringBuffer1, StringBuffer2, StringBuffer3, StringBuffer4;

	time_t now = time(0);

	CreateDirectoryA("Silhouette Data", NULL);

	int index = 0;
	for (int i = 0; i < Video_File_Path.size(); i++)
		if (Video_File_Path[i] == '/')
			index = i;


	if (WEBCAM_MODE)
		StringBuffer1 << "Silhouette Data/" << "Webcam" << ctime(&now) << '/';
	else
		StringBuffer1 << "Silhouette Data/" << Video_File_Path.substr(index,Video_File_Path.size()) << '/';

	CreateDirectoryA(StringBuffer1.str().c_str(), NULL);

	string CommonData = StringBuffer1.str();

	StringBuffer2 << CommonData;
	StringBuffer3 << CommonData; //contour
	StringBuffer4 << CommonData; // resampled

	StringBuffer1 << "Original/";
	CreateDirectoryA(StringBuffer1.str().c_str(), NULL);

	StringBuffer2 << "Longest Contour/";
	CreateDirectoryA(StringBuffer2.str().c_str(), NULL);

	StringBuffer3 << "Result/";
	CreateDirectoryA(StringBuffer3.str().c_str(), NULL);


	SavePath1 = StringBuffer1.str();
	SavePath2 = StringBuffer2.str();
	SavePath3 = StringBuffer3.str();
	SavePath4 = StringBuffer4.str();

}

void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata)
{

	if (event == EVENT_LBUTTONDOWN)
	{
		//Play Button
		if (y > Rows - 60 && Rows - 30)
		{
			if (x > 30 && x < 90)
			{
				cout << "Play/Pause" << endl;
				if (PlayCommand)
					PlayCommand = false;
				else
					PlayCommand = true;
			}

			else if (x > 100 && x < 160)
			{
				cout << "Reset" << endl;
				ResetCommand = true;
			}

			else if (x > 170 && x < 230)
			{
				cout << "Exit" << endl;
				ExitCommand = true;
			}
			else if (x > 240 && x < 360)
			{
				cout << "Recognition" << endl;
				StartRecognition = true;
			}
			
		}


	}

	/*
	else if (event == EVENT_RBUTTONDOWN)
	{
		cout << "Right button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
	else if (event == EVENT_MBUTTONDOWN)
	{
		cout << "Middle button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
	else if (event == EVENT_MOUSEMOVE)
	{
		cout << "Mouse move over the window - position (" << x << ", " << y << ")" << endl;

	}

	*/
}


int main(int argc, char *argv[])
{
	

	//--------------------------------------
	//�ʱ�ȭ
	//--------------------------------------

	//Config File read
	ConfigurationFileRead(); //�ֿ켱������ ó��


	//OPENCV initialize
	InitOpenCVModules();


	//Variable Initialize
	InitLocalVariables();

	//
	namedWindow("Masked Final", CV_WINDOW_AUTOSIZE);
	setMouseCallback("Masked Final", MouseCallBackFunc, NULL);

	
	//createTrackbar("Target FPS")


#if FILESAVE_MODE_EN
	InitSaveDirectories();
		
#endif

	Mat Morph_Element = getStructuringElement(MORPH_RECT, Size(2 * MORPH_STRUCT_SIZE_X + 1, 2 * MORPH_STRUCT_SIZE_Y + 1), Point(MORPH_STRUCT_SIZE_X, MORPH_STRUCT_SIZE_Y));
	
	vector<vector<Point>> VectorPointer;
	Mat ContourData;

	vector<Data_set> Training_data;
	Training_data = Read_training_data(Train_data_path);


	QueryPerformanceFrequency(&Frequency);

	frame_difference =  (double)Frequency.QuadPart / TARGET_FPS;

	QueryPerformanceCounter(&Start_counter); //�ʱ� Ÿ�̸�

	double frame_rate = 0.0;

	//-------------------------------------------------------------
	// ������ ó�� ���� ������
	//--------------------------------------------------------------

	frame_no = 0;

	while (1) //Frame Processing Loop Start
	{
		 //������ �ѹ� ���

		QueryPerformanceCounter(&End_counter);

		long long Counter_diff = (End_counter.QuadPart - Start_counter.QuadPart);

		//Framerate ����ȭ Start
		if ( Counter_diff < frame_difference) //Framerate ������
		{
			continue;
		}

		else
		{
			frame_rate = (double)Frequency.QuadPart / (double)(Counter_diff);

			if (frame_rate < TARGET_FPS && FRAMESKIP_ENABLE)
			{
				capture.read(Current_Frame);
			}

			QueryPerformanceCounter(&Start_counter);
		}
		//Framerate ����ȭ End



		if (Webcam_mode) //��ķ ������� �ƴ����� �Ǻ��� ���ǹ�
		{
			camera >> Current_Frame;
		}

		else
		{
			if (!capture.read(Current_Frame)) 
			{
				if (Loop_Mode) //���� ��忡���� ������ ������ �ٽ� ó������ ���ư���.
				{
#if USE_CONFIG_FILE
					capture = VideoCapture(Video_File_Path);
#else
					ostringstream FilePathVideo;
					FilePathVideo << READ_VIDEO_FOLDER << videoFilename;
					capture = VideoCapture(FilePathVideo.str()); //���Ͽ��� ���� ���� �Ҵ����ش�.
					
#endif
					InitLocalVariables();
					continue;
				}
				else
				{
					cerr << "End of File" << endl;
					cerr << "Exiting..." << endl;
					exit(EXIT_FAILURE);
				}
			}

		}

		static int Longest_Contour_Length;
		static Mat Longest_Contour;
		
#if INPUT_IS_SILHOUETTE //�̹� �Է��� �Ƿ翧�� ��� BGSubtraction�� ����. Contour Based parameter�� �̴´�
		Mat SplitChannel[3];
		split(Current_Frame, SplitChannel);
		ContourBasedParameterCalc(&SplitChannel[0], &Longest_Contour_Length);
		SplitChannel[0].copyTo(Longest_Contour);
#else
		
#if OPENCL_SUPPORT
		CL_Current_Frame = Current_Frame.getUMat(ACCESS_READ);
		FastBGSubtract();
#else
		FastBGSubtract_NonCL();
#endif
		ContourBasedFilter(&Longest_Contour, &Silhouette_Final, &Longest_Contour_Length);
#endif
		
		//cv::morphologyEx(Longest_Contour, Longest_Contour, MORPH_CLOSE, Morph_Element);
		/*
		cv::morphologyEx(Longest_Contour, Longest_Contour, MORPH_DILATE, Morph_Element);
		*/


		vector<Point> contour_point_array;

		//Contour(&Longest_Contour, &contour_point_array);


		/*
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
		*/

		static string Data_Result = "NULL";
		string Returned_name;

		if (Longest_Contour_Length > MIN_DETECT_CONTOUR && Longest_Contour_Length < MAX_DETECT_CONTOUR && frame_no > 0 && !Recognition_Success && StartRecognition) //�Ƿ翧�� �����ϰ�, ������ ���̰� ���� �̻��϶�
		{
			if (Contour_condition > CONTOUR_CONDITION_THRESHOLD)
			{
				Returned_name = Train_main(Longest_Contour, &Training_data);

				Recognition_Processing = true;

				if (Returned_name.find("No Data") == string::npos)
				{
					Recognition_Success = true;
					StartRecognition = false;
					Data_Result = Returned_name;
				}
				else
				{
					Data_Result = "Not Recognized";
				}
			}

			if (Contour_condition <= CONTOUR_CONDITION_THRESHOLD)
				Contour_condition++;
		}

		else
		{
			if (Contour_condition >0)
				Contour_condition--;
		}



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
		FPS << "Frame#: " << frame_no << " FPS: " << frame_rate; 
		cv::putText(Mask_Output, FPS.str(), Point(30, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255, 255, 255), 4, LINE_AA, false);
		cv::putText(Mask_Output, FPS.str(), Point(30, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(0, 0, 0), 2, LINE_AA, false);

#if DEBUG_MODE
		ostringstream DebugStream;

		DebugStream << "Longest Contour: " << Longest_Contour_Length << " Period: " <<period;
		cv::putText(Mask_Output, DebugStream.str(), Point(30, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255, 255, 255), 4, LINE_AA, false);
		cv::putText(Mask_Output, DebugStream.str(), Point(30, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(0, 0, 0), 2, LINE_AA, false);

#endif
		
		//�ȴ� ���⿡ ���� ǥ���� String�� �Է� �� �̹����� ǥ��
		/*
		if (Walk_Direction == LEFT_WALK)
			cv::putText(Mask_Output, "Left", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(0, 255, 0), 1, LINE_AA, false);
		else if (Walk_Direction == RIGHT_WALK)
			cv::putText(Mask_Output, "Right", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(255, 0, 0), 1, LINE_AA, false);
		else
			cv::putText(Mask_Output, "Stop", Point(30, Rows - 30), FONT_HERSHEY_PLAIN, 1.5, Scalar(0, 0, 255), 1, LINE_AA, false);
		*/

		//GUI �����



		
		//�̹����� �����Ǵ� ����� rectangle ǥ��
		if (Longest_Contour_Length > MIN_DETECT_CONTOUR && Longest_Contour_Length < MAX_DETECT_CONTOUR)
			rectangle(Mask_Output, Previous_Point - maxSize / 2, Previous_Point + maxSize / 2, Scalar(128, 128, 128), 2, 8, 0);


		//������ ǥ��
		cv::putText(Mask_Output, "Team MMI-01", Point(Cols - 150, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255, 255, 255), 3, LINE_AA, false);
		cv::putText(Mask_Output, "Team MMI-01", Point(Cols - 150, 30), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);



		//�ڽ��� ���ָ� ǥ��(�ν� ����� ���� ���� ���� ����)
		if (Recognition_Processing)
		{
			if (Recognition_Success)
			{
				cv::putText(Mask_Output, "Recognized ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(128, 255, 128), 3, LINE_AA, false);
				cv::putText(Mask_Output, "Recognized ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);
			}
			else
			{
				cv::putText(Mask_Output, "Processing ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(128, 255, 128), 3, LINE_AA, false);
				cv::putText(Mask_Output, "Processing ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);
			}
		}

		else
		{
			cv::putText(Mask_Output, "Searching.. ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(128, 128, 255), 3, LINE_AA, false);
			cv::putText(Mask_Output, "Searching.. ", Point(Cols - 120, 60), FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(64, 64, 64), 1, LINE_AA, false);
		}


		//�νı� ��ȯ ����� ���
		cv::putText(Mask_Output, "Recognized as:", Point(Cols - 170, Rows - 60), FONT_HERSHEY_COMPLEX_SMALL, 0.9, Scalar(255, 255, 0), 1, LINE_AA, false);
		cv::putText(Mask_Output, Data_Result, Point(Cols - 170, Rows - 30), FONT_HERSHEY_COMPLEX_SMALL, 0.9, Scalar(255, 0, 255), 1, LINE_AA, false);


#if DEBUG_MODE
		cv::imshow("Input", Current_Frame);
		cv::moveWindow("Input", 0 + Cols, 0);

		//�Ƿ翧�� �߾ӿ� ��ġ�ϵ��� ���� ��ġ ����

		cv::imshow("Silhouette", Silhouette_Final);
		cv::moveWindow("Silhouette", 0, 0 + Rows + 30);

		cv::imshow("Filtered", Longest_Contour);
		cv::moveWindow("Filtered", 0 + Cols, 0 + Rows + 30);

#endif
		frame_no++;



#if USE_GUI_BUTTON
		//GUI Make

		//Play Button
		if (PlayCommand)
		{
			rectangle(Mask_Output, Point(30, Rows - 60), Point(90, Rows - 30), Scalar(200, 200, 200), -1, 8, 0);
			rectangle(Mask_Output, Point(30, Rows - 60), Point(90, Rows - 30), Scalar(0, 255, 0), 2, 8, 0);
			putText(Mask_Output, "Play", Point(40, Rows - 40), FONT_HERSHEY_PLAIN, 1.2, Scalar(0, 128, 0), 1, LINE_AA, false);
		}

		else
		{
			rectangle(Mask_Output, Point(30, Rows - 60), Point(90, Rows - 30), Scalar(200, 200, 200), -1, 8, 0);
			rectangle(Mask_Output, Point(30, Rows - 60), Point(90, Rows - 30), Scalar(0, 0, 255), 2, 8, 0);
			putText(Mask_Output, "Pause", Point(32, Rows - 40), FONT_HERSHEY_PLAIN, 1.2, Scalar(0, 0, 128), 1, LINE_AA, false);
		}

		//Reset
		rectangle(Mask_Output, Point(100, Rows - 60), Point(100 + 60, Rows - 30), Scalar(200, 200, 200), -1, 8, 0);
				
		if (ResetCommand)
		{
			rectangle(Mask_Output, Point(100, Rows - 60), Point(100 + 60, Rows - 30), Scalar(120, 120, 120), -1, 8, 0);
			//�ش��ϴ� �Լ� ȣ��;

			ResetCommand = false;
		}

		rectangle(Mask_Output, Point(100, Rows - 60), Point(100 + 60, Rows - 30), Scalar(255, 255, 0), 2, 8, 0);
		putText(Mask_Output, "Reset", Point(100 + 2, Rows - 40), FONT_HERSHEY_PLAIN, 1.2, Scalar(128, 128, 0), 1, LINE_AA, false);

		//Exit Button
		rectangle(Mask_Output, Point(170, Rows - 60), Point(170 + 60, Rows - 30), Scalar(200, 200, 200), -1, 8, 0);
		rectangle(Mask_Output, Point(170, Rows - 60), Point(170 + 60, Rows - 30), Scalar(0, 255, 255), 2, 8, 0);
		putText(Mask_Output, "Exit", Point(170 + 10, Rows - 40), FONT_HERSHEY_PLAIN, 1.2, Scalar(0, 128, 128), 1, LINE_AA, false);

		//Recognition Start Button
		rectangle(Mask_Output, Point(240, Rows - 60), Point(240 + 120, Rows - 30), Scalar(200, 200, 200), -1, 8, 0);
		rectangle(Mask_Output, Point(240, Rows - 60), Point(240 + 120, Rows - 30), Scalar(100, 100, 255), 2, 8, 0);
		putText(Mask_Output, "Recognition", Point(240 + 2, Rows - 40), FONT_HERSHEY_PLAIN, 1.2, Scalar(50, 128, 128), 1, LINE_AA, false);

		if (ExitCommand)
			exit(EXIT_SUCCESS);

#endif



#if FILESAVE_MODE_EN

		ostringstream Save1;
		Save1 << SavePath1 << std::setfill('0') << std::setw(5) << frame_no << ".jpg";
		imwrite(Save1.str(), Silhouette_Final);

		ostringstream Save2;
		Save2 << SavePath2 << std::setfill('0') << std::setw(5) << frame_no << ".jpg";
		imwrite(Save2.str(), Longest_Contour);

		ostringstream Save3;
		Save3 << SavePath3 << std::setfill('0') << std::setw(5) << frame_no << ".jpg";
		imwrite(Save3.str(), Mask_Output);


#endif


		cv::imshow("Masked Final", Mask_Output);

		//â�� �˸´� ��ġ�� �̵�
		cv::moveWindow("Masked Final", 0, 0);

		//Play ���°� �ƴϸ� HOLD, EXIT ���ǵ��� ��� ����
		while (!PlayCommand)
			if (waitKey(1) == 27) //ESCŰ�� ���� 
				break;
			else if (ExitCommand)
				exit(EXIT_SUCCESS);


		if (waitKey(1) == 27) //ESCŰ�� ���� 
			break;

		QueryPerformanceCounter(&End_counter);

	}
	



	return 0;
}