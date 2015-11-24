#include "common.h"

using namespace cv;

void FastBGSubtract() //Silhouette_Final�� �ٷ� �Ƿ翧 �����͸� ��ִ´�.
{
	static UMat CL_Background_MOG;

	static Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor  
	static Mat DissolveMatrix = Mat::zeros(Rows, Cols, CV_8UC3);

	if (pMOG2 == NULL)
		pMOG2 = createBackgroundSubtractorMOG2(200, 16.0, true);

	pMOG2->apply(CL_Current_Frame, CL_Background_MOG);

	CL_Background_MOG.copyTo(Silhouette_Final);



	for (int x = 0; x < Cols; x++)
	{
		for (int y = 0; y < Rows; y++)
		{
			if (Silhouette_Final.data[y*Cols + x] == 127)
				Silhouette_Final.data[y*Cols + x] = 0;
		}
	}
	


}

bool CheckEmpty(Mat* Input_Image) // ���� ä�� �̹����� ����
{
	for (int x = 0; x < Cols; x++)
	{
		for (int y = 0; y < Rows; y++)
		{
			if (Input_Image->data[y*Cols + x] != 0)
				return false;
		}
	}

	return true;
}