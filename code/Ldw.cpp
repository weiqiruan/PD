#include "Ldw.h"

static uchar frameBuff[1280 * 720 * 2];
static uchar sFrameBuff[1280 * 720 * 2];
uchar leftMaskBuff[1280 * 720]; 
uchar rightMaskBuff[1280 * 720];

static stLanePoints lanePoints0[30000];
static stLanePoints lanePoints1[10000];

#define TEST_CAR 1
Ldw::Ldw()
{
	frame = Mat::zeros(720, 1280, CV_8U);
	frame.data = frameBuff;

	smallFrame = Mat::zeros(180, 320, CV_8U);
	smallFrame.data = sFrameBuff;

	cascade.load(LANE_CASCADE_NAME);

	// ������೵���ߵĻ�������
	leftMask = Mat::zeros(720, 1280, CV_8U);
	leftMask.data = leftMaskBuff;

	rightMask = Mat::zeros(720, 1280, CV_8U);
	rightMask.data = rightMaskBuff;

	leftLaneRoi = Rect(0, 90, 160, 90);
	rightLaneRoi = Rect(160, 90, 160, 90);

	leftLane.tracked = 0;
	rightLane.tracked = 0;

	pos1 = 0;
	pos0 = 0;
}

int Ldw::InitMaskAndRoi(Rect roi, Mat& mask, Rect& cannyRoi, stLane& line)
{
	uchar* pMask;
	// �����һ֡��⵽������и���
	if (line.tracked != 0 )
	{
		// ����
		Point2f tmPoint;
		Point leftTarget, rightTarget;
		bool flag = true;
		for (int i = 360; i < 720; i++)
		{
			pMask = mask.ptr<uchar>(i);

			if (line.curLane[0] * line.curLane[1] == 0)
				continue;

			tmPoint.x = (i - line.curLane[3]) / line.curLane[1] * line.curLane[0] + line.curLane[2];
			tmPoint.y = i*1.0f;

			for (int j = (int)tmPoint.x - 40; j < (int)tmPoint.x + 40; j++)
			{
				if (j >= 0 && j < 1280)
				{
					pMask[j] = 255;

					/* ����roi���� */
					if (flag)
					{
						leftTarget = Point(j, i);
						rightTarget = Point(j, i);
						flag = false;
					}

					if (leftTarget.x > j)
						leftTarget = Point(j, i);

					if (rightTarget.x < j)
						rightTarget = Point(j, i);
				}
			}
		}

		// �����ҵ���Ŀ�꣬��ǳ�CANNY���������
		cannyRoi.x = min(leftTarget.x, rightTarget.x);
		cannyRoi.y = min(rightTarget.y, leftTarget.y);
		cannyRoi.width = abs(rightTarget.x - leftTarget.x);
		cannyRoi.height = 360;

		return true;
	}
	else
	{
		// ��С4��
		resize(frame, smallFrame, Size(320, 180), INTER_LINEAR);

		// ����LBP�����߼��
		cascade.detectMultiScale(smallFrame(roi), laneTargets, 1.4, 1, 0, Size(15, 15), Size(30, 30));
		if (laneTargets.size() > 0)
		{
			// ��ȡmask������canny���ݶ�����
			Rect tmRect;

			Rect leftTarget, rightTarget;
			for (int i = 0; i < laneTargets.size(); i++)
			{
				tmRect.x = (laneTargets[i].x + roi.x) * 4;
				tmRect.y = (laneTargets[i].y + roi.y) * 4;
				tmRect.width = laneTargets[i].width * 4;
				tmRect.height = laneTargets[i].height * 4;

				// ���mask����
				for (int j = tmRect.y; j < tmRect.y + tmRect.height; j++)
				{
					pMask = mask.ptr<uchar>(j);
					memset(pMask + tmRect.x, 0xFF, tmRect.width);
				}

				// ȷ�����ROI����
				if (i == 0)
				{
					leftTarget = tmRect;
					rightTarget = tmRect;
				}
				else
				{
					// �����Ŀ��
					if (leftTarget.x > tmRect.x)
						leftTarget = tmRect;

					// ���Ҳ��Ŀ��
					if (rightTarget.x < tmRect.x)
						rightTarget = tmRect;
				}

				// �����ҵ���Ŀ�꣬��ǳ�CANNY���������
				cannyRoi.x = min(leftTarget.x, rightTarget.x);
				cannyRoi.y = min(rightTarget.y, leftTarget.y);
				cannyRoi.width = abs(rightTarget.x - leftTarget.x) + max(leftTarget.width, rightTarget.width);
				cannyRoi.height = abs(leftTarget.y - rightTarget.y) + max(leftTarget.height, rightTarget.height);
			}

			return true;
		}
		else
		{
			memset(&line, 0, sizeof(line));
			return false;
		}
	}
}

void Ldw::InitEdgePoints(Rect roi, Mat mask)
{
	AsertROI(roi);
	Mat element(3, 3, CV_8U, Scalar(1));
	Mat cannyFrame, edge, sCanny, sFrame;
	Rect tmp(roi.x / 2, roi.y / 2, roi.width / 2, roi.height / 2);
	resize(frame, sFrame, Size(640, 360), INTER_LINEAR);
	Canny(sFrame(tmp), edge, 30, 90, 3, false);
	resize(edge, cannyFrame, Size(roi.width, roi.height), INTER_LINEAR);

	// ȡCANNY��Ե�ϵĵ�
	unsigned char a00, a01, a02;
	unsigned char a10, a11, a12;
	unsigned char a20, a21, a22;

	uchar *pCannyPtr, *pMask;
	stLanePoints stPoint;
	uchar* pFrame1, *pFrame2, *pFrame3;

	for (int j = 1; j < roi.height - 1; j++)
	{
		pCannyPtr = cannyFrame.ptr<uchar>(j);		// canny ͼ��ָ��
		pMask = mask.ptr(j + roi.y);
		pFrame1 = frame.ptr<uchar>(j + roi.y);
		pFrame2 = frame.ptr<uchar>(j + roi.y - 1);
		pFrame3 = frame.ptr<uchar>(j + roi.y + 1);
		double ux, uy;

		for (int i = 1; i < roi.width - 1; i++)
		{
			
			if (pCannyPtr[i] > 100 && pMask[i + roi.x] == 255 /*&& pFrame1[i + roi.x] > 150*/)
			{
				stPoint.edgePoint.x = i + roi.x;
				stPoint.edgePoint.y = j + roi.y;
	
				a00 = pFrame2[i + roi.x - 1];
				a01 = pFrame1[i + roi.x - 1];
				a02 = pFrame3[i + roi.x - 1];
				a10 = pFrame2[i + roi.x];
				a11 = pFrame1[i + roi.x];
				a12 = pFrame3[i + roi.x];
				a20 = pFrame2[i + roi.x + 1];
				a21 = pFrame1[i + roi.x + 1];
				a22 = pFrame3[i + roi.x + 1];

				ux = a20 * (1) + a21 * (2) + a22 * (1)
					+ (a00 * (-1) + a01 * (-2) + a02 * (-1));
				uy = a02 * (1) + a12 * (2) + a22 * (1)
					+ a00 * (-1) + a10 * (-2) + a20 * (-1);

				stPoint.grad = atan2(uy, ux);
				//stPoint.gray = a11;

				if (ux * uy != 0)
				{
					lanePoints0[pos0++] = stPoint;
				}
		
			}
		}
	}
}

void Ldw::InitMainDirectPoints(void)
{
	int interVal = 0;
	float gHist[360] = { 0.0f };
	vector<stLanePoints>::iterator it;

	memset(gHist, 0, sizeof(gHist));
	if (pos0 > 0)
	{
		// ��һ��ȷ���ݶ����ķ���
		for (int i = 0; i < pos0; i++)
		{
			interVal = int(lanePoints0[i].grad / (CV_PI*0.1111) + 9.0f);
			lanePoints0[i].interVal = interVal;
			gHist[interVal] += 1;
		}

		float maxHist = -1.0f;
		int maxDirect = 0;
		int start = 0;
		int end = 0;

		if (pre_main_direct > 0)
		{
			start = 9;
			end = 17;
		}
		else if (pre_main_direct < 0)
		{
			start = 0;
			end = 9;
		}
		else
		{
			start = 0;
			end = 17;
		}

		for (int i = 9; i < 17; i++)
		{
			if (i == 2 || i == 12)
			{
				i += 3;
			}
			if (gHist[i] > maxHist)
			{
				maxHist = gHist[i];
				maxDirect = i;
			}
		}

		// ��ȡ��������ݶȷ����ϵĵ�
		for (int i = 0; i < pos0; i++)
		{
			if (abs(lanePoints0[i].interVal - maxDirect) < 2)
			{
				lanePoints1[pos1++] = lanePoints0[i];
			}
		}

		// �ڶ���ȷ��������ľ�ȷֵ
		pos0 = 0;
		float maxNumHist = -1.0f;
		int maxNumDirect = -1;
		float direct = 0.0f;

		memset(gHist, 0, sizeof(gHist));
		if (pos1 > 0)
		{
			for (int i = 0; i < pos1; i++)
			{
				interVal = int(lanePoints1[i].grad / (CV_PI*0.01111)) + 90;
				lanePoints1[i].interVal = interVal;
				gHist[interVal] += 1;
			}

			for (int i = 0; i < 180; i++)
			{
				if (gHist[i] > maxNumHist)
				{
					maxNumHist = gHist[i];
					maxNumDirect = i;
				}
			}
			direct = (CV_PI*0.01111) * (maxNumDirect - 90);
		}

		for (int i = 0; i < pos1; i++)
		{
			if (abs(lanePoints1[i].interVal - maxNumDirect) < 2)
			{
				lanePoints0[pos0++] = lanePoints1[i];
			}
		}
		//printf("size=%d\n", lanePoints.size());
		pre_main_direct = direct;
		//printf("%f\n", direct);
	}
	//ADS_LaneDetect_Test_Show_point();
}

void Ldw::FitLine(stLane& lane)
{
	vector<Point> line_point;

	for (int i = 0; i < pos0; i += 2)
	{
		line_point.push_back(lanePoints0[i].edgePoint);
		//printf("%d \n", lanePoints0[i].gray);
	}
	fitLine(line_point, lane.curLane, CV_DIST_HUBER, 0, 0.01, 0.01);
	float theta = atan(lane.curLane[1] / lane.curLane[0]);

	lane.curTheta = theta;
	pos0 = 0; 
	pos1 = 0;
//	printf("theta=%f\n", theta);
}


void Ldw::LaneDet(Rect roi, Mat& mask, Rect cannyRoi, vector<stLanePoints> &lanePoints, 
	              stLane& lane)
{
	/* ��ʼ�����ߵ�MASK��CANNY���� */
	if (InitMaskAndRoi(roi, mask, cannyRoi, lane))
	{
		InitEdgePoints(cannyRoi, mask);
		InitMainDirectPoints();
	}
	if (pos0 > 10)
	{
		FitLine(lane);
		Lane2BvLane(lane);
		lane.tracked = true;
	}
	else
	{
		lane.tracked = false;
	}

	if (lane.tracked)
	{
		lane.preLane = lane.curLane;
		//lane.preTheta = lane.curTheta;
	}
}


void Ldw::Lane2BvLane(stLane& lane)
{
	Point2f point = { 0 };
	vector<Point2f> be_point;
	vector<Point2f> be_point1;
	vector<Point2f> af_point;
	vector<Point2f> af_point1;
	Vec4f addLane;
	// �������ߵ������

	point.x = (0 - lane.curLane[3]) / lane.curLane[1] * lane.curLane[0] + lane.curLane[2];
	point.y = 0;
	be_point.push_back(point);
	point.x = lane.curLane[2];
	point.y = lane.curLane[3];
	be_point.push_back(point);

	//perspectiveTransform(be_point, af_point, trans_Matrix);

	/*float dist = sqrt((af_point[0].x - af_point[1].x)*(af_point[0].x - af_point[1].x)
		+ (af_point[0].y - af_point[1].y)*(af_point[0].y - af_point[1].y));*/


	point.x = (0 - lane.curLane[3]) / lane.curLane[1] * lane.curLane[0] + lane.curLane[2];
	point.y = 720;

	be_point1.push_back(point);
	point.x = lane.curLane[2];
	point.y = 720 - lane.curLane[3];

	be_point1.push_back(point);
	
	perspectiveTransform(be_point1, af_point1, tran_H);

	float dist = sqrt((af_point1[0].x - af_point1[1].x)*(af_point1[0].x - af_point1[1].x)
		+ (af_point1[0].y - af_point1[1].y)*(af_point1[0].y - af_point1[1].y));

	addLane[0] = (af_point1[0].x - af_point1[1].x) / dist;
	addLane[1] = (af_point1[0].y - af_point1[1].y) / dist;
	addLane[2] = af_point1[0].x;
	addLane[3] = af_point1[0].y;

	if (addLane[0] < 0)
	{
		addLane[0] = -addLane[0];
		addLane[1] = -addLane[1];
	}

	Point temp_point;
	GetPointInLine(addLane, 0, temp_point);
	
	temp_point.x = abs(temp_point.x) - carWidth / 2;

	lane.bvEnd = temp_point;
}

bool Ldw::CheckLane(stLane& leftLine, stLane& rightLine)
{

	float thres = CV_PI / 10.0f;

	int leftFlag = 0, rightFlag = 0;

	// ���ж�����
	if (leftLine.curLane[0] == 0)
	{
		memset(&leftLine, 0, sizeof(leftLine));
		leftLine.tracked = false;
		leftFlag = true;
	}
	// ��������һ֡�߽��бȽ�
	if (leftLine.tracked)
	{
		// �����ǰһ֡����
		printf("\n %f \n", abs(leftLine.preTheta) - abs(leftLine.curTheta));
		if (leftLine.preTheta != 0)
		{
			if (abs(abs(leftLine.preTheta) - abs(leftLine.curTheta)) > thres
				|| abs(leftLine.curTheta) < thres)
			{
				memset(&leftLine, 0, sizeof(leftLine));
				leftLine.tracked = false;
				leftFlag = true;
			}
		}

		if (leftLine.preBvEnd.x != 0)
		{
			// �ж�endx �Ƿ���Ч
			if (abs(leftLine.bvEnd.x) > 250 || abs(leftLine.bvEnd.x - leftLine.preBvEnd.x) > 40)
			{
				memset(&leftLine, 0, sizeof(leftLine));
				leftLine.tracked = false;
				leftFlag = true;
			}
		}
		
	}
	else
	{
		memset(&leftLine, 0, sizeof(leftLine));
		leftFlag = true;
	}

	// ���ж�����
	if (rightLine.curLane[0] == 0)
	{
		cout << "1" << endl;
		memset(&rightLine, 0, sizeof(rightLine));
		rightLine.tracked = false;
		rightFlag = true;
	}
	if (rightLine.tracked)
	{
		if (rightLine.preTheta != 0)
		{
			if (abs(rightLine.preTheta - rightLine.curTheta) > thres
				|| abs(rightLine.curTheta) < thres)
			{
				cout << "2" << endl;
				memset(&rightLine, 0, sizeof(rightLine));
				rightLine.tracked = false;
				rightFlag = true;
			}
		}
		if (rightLine.preBvEnd.x != 0)
		{
			if (abs(rightLine.bvEnd.x) > 250 || abs(rightLine.bvEnd.x - rightLine.preBvEnd.x) > 40)
			{
				cout << "3" << endl;
				memset(&rightLine, 0, sizeof(rightLine));
				rightLine.tracked = false;
				rightFlag = true;
			}
		}
	}
	else
	{
		cout << "4" << endl;
		rightFlag = true;
		memset(&rightLine, 0, sizeof(rightLine));
	}

	// ��������߶����ڣ�����ƽ���ж�
	if (!leftFlag && !rightFlag)
	{
		if (abs(abs(leftLine.bvTheta) - abs(rightLine.bvTheta)) > CV_PI / 9)
		{
			memset(&leftLine, 0, sizeof(leftLine));
			memset(&rightLine, 0, sizeof(rightLine));
			leftLine.tracked = false;
			rightLine.tracked = false;
			leftFlag = true;
			rightFlag = true;
		}

		if (abs(leftLine.bvEnd.x + rightLine.bvEnd.x) < 320 - carWidth || abs(leftLine.bvEnd.x + rightLine.bvEnd.x) > 250)
			//(abs(leftLine.bvEnd.x + rightLine.bvEnd.x) < 80 || abs(leftLine.bvEnd.x + rightLine.bvEnd.x) > 320)
		{
			
			memset(&leftLine, 0, sizeof(leftLine));
			memset(&rightLine, 0, sizeof(rightLine));
			leftLine.tracked = false;
			rightLine.tracked = false;
			leftFlag = true;
			rightFlag = true;
		}


		printf("%.2f \n", leftLine.curTheta - rightLine.curTheta);

		if (abs(leftLine.curTheta - rightLine.curTheta) < CV_PI / 4)
		{
			memset(&leftLine, 0, sizeof(leftLine));
			memset(&rightLine, 0, sizeof(rightLine));
			leftLine.tracked = false;
			rightLine.tracked = false;
			leftFlag = true;
			rightFlag = true;
		}
	}

	if (!leftFlag)
	{
		leftLine.tracked = true;
	}
	if (!rightFlag)
	{
		rightLine.tracked = true;
	}

	return true;
}


bool  Ldw::LaneStat(stLane& leftLane, stLane& rightLane)
{
	/* ������ҳ�����û���ж�Ϊ���ٳɹ����򲻽��б����ж� */
	if (!leftLane.tracked || !rightLane.tracked)
		return false;

	/* ��ȡ�����Ƕ� */
	//	float carThera = (leftLane.bvTheta + rightLane.bvTheta) / 2;

	/* ��һ֡���������ж� */

	// �����һ֡����ƫת
	if (preWarnFlag == -1)
	{
		if (leftLane.bvEnd.x - leftLane.preBvEnd.x < 0 &&
			leftLane.bvEnd.x < 20)
		{
			warnFlag = -1;
		}
		else
			warnFlag = 0;
	}
	// �����һ֡�ж�Ϊ����ƫת
	else if (preWarnFlag == 1)
	{
		if (rightLane.bvEnd.x - rightLane.preBvEnd.x < 0 &&
			rightLane.bvEnd.x < 20)
		{
			warnFlag = 1;
		}
		else
			warnFlag = 0;
	}
	// �����һ֡δ����ƫת
	else if (preWarnFlag == 0)  
	{
		// ����ƫת�ж�
		if (leftLane.bvEnd.x - leftLane.preBvEnd.x < 0 &&
			/*rightLane.bvEnd.x - rightLane.preBvEnd.x >= 0 &&*/
			leftLane.bvEnd.x < 15)
		{
			warnFlag = -1;
		}
		// ����ƫת�ж�
		else if (rightLane.bvEnd.x - rightLane.preBvEnd.x < 0 &&
			/*leftLane.bvEnd.x - leftLane.preBvEnd.x >= 0 &&*/
			rightLane.bvEnd.x < 15)
		{
			warnFlag = 1;
		}
		// ������ʻ
		else
		{
			warnFlag = 0;
		}
	}

	///* ƫת�����Ϣ�ж� */
	//if (leftLane.preBvEnd.x > 10 &&
	//	rightLane.preBvEnd.x > 10)
	//{
	//	departFlag = 0;
	//	warnFlag = 0;
	//}
	//// ����ƫת�����Ϣ�ж�
	//else if (leftLane.bvEnd.x - leftLane.preBvEnd.x < 0 &&
	//	rightLane.bvEnd.x - rightLane.preBvEnd.x > 0 &&
	//	leftLane.bvEnd.x < 0)
	//{
	//	departFlag = -1;
	//	warnFlag = 0;
	//}
	//else if (rightLane.bvEnd.x - rightLane.preBvEnd.x < 0 &&
	//	leftLane.bvEnd.x - leftLane.preBvEnd.x > 0 &&
	//	rightLane.bvEnd.x < 0)
	//{
	//	departFlag = 1;
	//	warnFlag = 0;
	//}

	if (leftLane.tracked && rightLane.tracked)
	{
		leftLane.preLane = leftLane.curLane;
		leftLane.preTheta = leftLane.curTheta;
		leftLane.preBvLane = leftLane.bvLane;
		leftLane.preBvEnd = leftLane.bvEnd;

		rightLane.preLane = rightLane.curLane;
		rightLane.preTheta = rightLane.curTheta;
		rightLane.preBvLane = rightLane.bvLane;
		rightLane.preBvEnd = rightLane.bvEnd;

		preWarnFlag = warnFlag;

	}
	else
	{
		preWarnFlag = warnFlag = 0;
	}
}

/***************************************************************************************************
��������ADS_LaneDetec_get_other_lane
��Ҫ���ܣ�//�������һ�߳�����
*       one_line           -I    ��֪������
*       dir_flag           -O    ���ֱ�߷���
*       other_line         -O    δ֪������
*       trans_Matrix       -I    ת������
*       back_trans_Matrix  -I    ����ת������

��ע���޷��� VOID
***************************************************************************************************/
void Ldw::ADS_LaneDetect_get_other_lane(Vec4f& one_line, int dir_flag, Vec4f& other_line)
{
	Point t_point = { 0 };
	vector<Point2f> be_point;
	vector<Point2f> af_point;

	GetPointInLine(one_line, frame.rows * 0.5, t_point);
	be_point.push_back(t_point);
	t_point.x = one_line[2];
	t_point.y = one_line[3];
	be_point.push_back(t_point);

	perspectiveTransform(be_point, af_point, trans_Matrix);

	Point p1_BV = { 0 };
	Point p2_BV = { 0 };
	Point p3_BV = { 0 };
	Point p4_BV = { 0 };

	p1_BV = af_point[0];
	p2_BV = af_point[1];
	if (dir_flag == -1)
	{
		p3_BV.x = p1_BV.x - 360;
		p4_BV.x = p2_BV.x - 360;
	}
	else if (dir_flag == 1)
	{
		p3_BV.x = p1_BV.x + 360;
		p4_BV.x = p2_BV.x + 360;
	}

	p3_BV.y = p1_BV.y;
	p4_BV.y = p2_BV.y;

	af_point[0] = p3_BV;
	af_point[1] = p4_BV;
	perspectiveTransform(af_point, be_point, back_trans_Matrix);
	//ADS_LaneDetec_perspective_trans_single_point(af_point[0], &be_point[0], back_trans_Matrix);
	//ADS_LaneDetec_perspective_trans_single_point(af_point[1], &be_point[1], back_trans_Matrix);

	float dist = sqrt((be_point[0].x - be_point[1].x)*(be_point[0].x - be_point[1].x)
		+ (be_point[0].y - be_point[1].y)*(be_point[0].y - be_point[1].y));

	Vec4f addLane;

	addLane[0] = (be_point[0].x - be_point[1].x) / dist;
	addLane[1] = (be_point[0].y - be_point[1].y) / dist;
	addLane[2] = be_point[0].x;
	addLane[3] = be_point[0].y;

	if (addLane[0] < 0)
	{
		addLane[0] = -addLane[0];
		addLane[1] = -addLane[1];
	}

	other_line = addLane;
}

void Ldw::Detect(void)
{
	preWarnFlag = warnFlag;

	// ���߼��
	memset(leftMaskBuff, 0, sizeof(leftMaskBuff));
	pos0 = 0; 
	pos1 = 0;
	LaneDet(leftLaneRoi, leftMask, leftCannyRoi, leftLanePoints, leftLane);

	// ���߼��
	memset(rightMaskBuff, 0, sizeof(rightMaskBuff));
	pos0 = 0; 
	pos1 = 0;
 	LaneDet(rightLaneRoi, rightMask, rightCannyRoi, rightLanePoints, rightLane);

	// ȷ������ֱ�ߵ�����
 	CheckLane(leftLane, rightLane);
	int f = ADS_LaneDetect_Check_Circle();
	
	// ���ݳ�ͷ��б����仯ȷ���Ƿ����
	if (!LaneStat(leftLane, rightLane))
	{
		warnFlag = 0;
		preWarnFlag = 0;
	}


	// �����ҳ����ߵĵ�ת�����������
	if (leftLane.tracked && rightLane.tracked)
	{ 
		//preWarnFlag = warnFlag;

		leftLane.preLane   = leftLane.curLane;
		leftLane.preTheta  = leftLane.curTheta;
		leftLane.preBvLane = leftLane.bvLane;
		leftLane.preBvEnd  = leftLane.bvEnd;
		
		rightLane.preLane   = rightLane.curLane;
		rightLane.preTheta  = rightLane.curTheta;
		rightLane.preBvLane = rightLane.bvLane;
		rightLane.preBvEnd  = rightLane.bvEnd;

		rightGetFlag = 0;
		leftGetFlag  = 0;
	}
	else if (leftLane.tracked)
	{
		preWarnFlag = 0;
		warnFlag = 0;
		leftLane.preLane = leftLane.curLane;
		leftLane.preTheta = leftLane.curTheta;
		leftLane.preBvLane = leftLane.bvLane;
		leftLane.preBvEnd = leftLane.bvEnd;
	}
	else if (rightLane.tracked)
	{
		preWarnFlag = 0;
		warnFlag = 0;
		rightLane.preLane = rightLane.curLane;
		rightLane.preTheta = rightLane.curTheta;
		rightLane.preBvLane = rightLane.bvLane;
		rightLane.preBvEnd = rightLane.bvEnd;
	}

	// �������ƫ�ƣ�����=���ߣ�����ͨ���������
	if (departFlag == -1)
	{
		rightLane = leftLane;
		memset(&leftLane, 0, sizeof(leftLane));

		departFlag = 0;
	}
	// �������ƫ�ƣ�����=���ߣ�����ͨ���������
	else if (departFlag == 1)
	{
		leftLane = rightLane;
		memset(&rightLane, 0, sizeof(rightLane));

		departFlag = 0;
	}
}

inline void Ldw::GetPointInLine(Vec4f line, int y, Point& point)
{
	if (line[0] * line[1] == 0)
	{
		point.x = 0;
		point.y = 0;
	}
	else
	{

		point.x = (int)((y - line[3]) / line[1] * line[0] + line[2]);
		point.y = y;
	}

}


void Ldw::InitMatrixData(void)        //;����������ͷ�궨���Զ��궨�� 20161206
{
	vector<Point2f> objPts(9), imgPts(9);

	imgPts[0] = Point2f(448, 456);
	imgPts[1] = Point2f(658, 453);
	imgPts[2] = Point2f(863, 451);
	imgPts[3] = Point2f(388, 520);
	imgPts[4] = Point2f(663, 519);
	imgPts[5] = Point2f(940, 515);
	imgPts[6] = Point2f(310, 598);
	imgPts[7] = Point2f(664, 600);
	imgPts[8] = Point2f(1017, 591);

	objPts[0] = Point2f(280, 1200);
	objPts[1] = Point2f(430, 1200);
	objPts[2] = Point2f(580, 1200);
	objPts[3] = Point2f(280, 700);
	objPts[4] = Point2f(430, 700);
	objPts[5] = Point2f(580, 700);
	objPts[6] = Point2f(280, 500);
	objPts[7] = Point2f(430, 500);
	objPts[8] = Point2f(580, 500);

	trans_Matrix = findHomography(imgPts, objPts);
	back_trans_Matrix = findHomography(objPts, imgPts);

}

void Ldw::PlotSingleLine(Mat& frame, Vec4f& plotLine, double dL)
{
	Point p1, p2;
	p1.x = int(plotLine[2] - dL*plotLine[0]);
	p1.y = int(plotLine[3] - dL*plotLine[1]);
	p2.x = int(plotLine[2] + dL*plotLine[0]);
	p2.y = int(plotLine[3] + dL*plotLine[1]);
	line(frame, p1, p2, Scalar(255, 255, 255), 8);
}

void Ldw::AsertROI(Rect &roi)
{
	int width = 1280 - 10;
	int height = 720 - 10;

	if (roi.x < 0)
		roi.x = 0;
	else if (roi.x > width)
		roi.x = width;

	if (roi.y < 0)
		roi.y = 0;
	else if (roi.y > height)
		roi.y = height;

	if ((roi.x + roi.width) > width)
		roi.width = width - roi.x;

	if ((roi.y + roi.height) > height)
		roi.height = height - roi.y;
}

int Ldw::ADS_LaneDetect_Check_Circle()
{
	Dtheta = abs(leftLane.curTheta - leftLane.preTheta);
	Dtheta += abs(rightLane.curTheta - rightLane.preTheta);

	cout << "Theta::::" << Dtheta << endl;

	return 0;
}