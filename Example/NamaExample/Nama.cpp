#include "CameraDS.h"
#include "Nama.h"
#include "GlobalValue.h"
#include <fstream>
#include <iostream>

#include <funama.h>
#include <authpack.h>

#pragma comment(lib, "nama.lib")

namespace NE
{
	size_t FileSize(std::ifstream& file)
	{
		std::streampos oldPos = file.tellg();

		file.seekg(0, std::ios::beg);
		std::streampos beg = file.tellg();
		file.seekg(0, std::ios::end);
		std::streampos end = file.tellg();

		file.seekg(oldPos, std::ios::beg);

		return static_cast<size_t>(end - beg);
	}

	bool LoadBundle(const std::string& filepath, std::vector<char>& data)
	{
		std::ifstream fin(filepath, std::ios::binary);
		if (false == fin.good())
		{
			fin.close();
			return false;
		}
		size_t size = FileSize(fin);
		if (0 == size)
		{
			fin.close();
			return false;
		}
		data.resize(size);
		fin.read(reinterpret_cast<char*>(&data[0]), size);

		fin.close();
		return true;
	}
}

std::string NE::Nama::_filters[6] = { "nature", "delta", "electric", "slowlived", "tokyo", "warm" };

NE::Nama::Nama()
	:m_frameID(0),
	m_cap(0),
	m_curBundleIdx(0),
	m_mode(PROP),
	m_isBeautyOn(true),
	m_frameWidth(0),
	m_frameHeight(0),
	m_curFilterIdx(0),
	m_curColorLevel(1.0f),
	m_curBlurLevel(5.0f),
	m_curCheekThinning(1.0f),
	m_curEyeEnlarging(1.0f)
{
}

void NE::Nama::Init(const int width, const int height)
{
	m_frameWidth = width;
	m_frameHeight = height;
	m_cap = std::tr1::shared_ptr<CCameraDS>(new CCameraDS);
	if (false == m_cap->OpenCamera(0, false, m_frameWidth, m_frameHeight))
	{
		exit(1);
	}

	std::vector<char> v3data;
	if(false == NE::LoadBundle(g_fuDataDir + g_v3Data, v3data))
	{
		exit(1);
	}

	fuSetup(reinterpret_cast<float*>(&v3data[0]), NULL, g_auth_package, sizeof(g_auth_package));

	{
		std::vector<char> propData;
		if (false == NE::LoadBundle(g_fuDataDir + g_faceBeautification, propData))
		{
			std::cout << "load face beautification data failed." << std::endl;
			exit(1);
		}
		std::cout << "load face beautification data." << std::endl;

		m_beautyHandles = fuCreateItemFromPackage(&propData[0], propData.size());

		//"nature", "delta", "electric", "slowlived", "tokyo", "warm"
		fuItemSetParams(m_beautyHandles, "filter_name", &_filters[m_curFilterIdx][0]);
		fuItemSetParamd(m_beautyHandles, "color_level", m_curColorLevel);
		fuItemSetParamd(m_beautyHandles, "blur_level", m_curBlurLevel);
		fuItemSetParamd(m_beautyHandles, "cheek_thinning", m_curCheekThinning);
		fuItemSetParamd(m_beautyHandles, "eye_enlarging", m_curEyeEnlarging);
	}
	


	m_propHandles.resize(g_propCount);

	m_curBundleIdx = -1;
	NextBundle();
}

void NE::Nama::SwitchRenderMode()
{
	static MODE nextMode[] = { LANDMARK , PROP };
	m_mode = nextMode[m_mode];
}

void NE::Nama::SwitchBeauty()
{
	m_isBeautyOn = !m_isBeautyOn;
}

void NE::Nama::PreBundle()
{
	--m_curBundleIdx;
	if (m_curBundleIdx < 0)
	{
		m_curBundleIdx += m_propHandles.size();
	}

	CreateBundle(m_curBundleIdx);
}

void NE::Nama::NextBundle()
{
	++m_curBundleIdx;
	m_curBundleIdx %= m_propHandles.size();

	CreateBundle(m_curBundleIdx);
}

void NE::Nama::SwitchFilter()
{
	if (false == m_isBeautyOn)
	{
		return;
	}
	++m_curFilterIdx;
	m_curFilterIdx %= sizeof(_filters) / sizeof(_filters[0]);
	fuItemSetParams(m_beautyHandles, "filter_name", &_filters[m_curFilterIdx][0]);
}

void NE::Nama::UpdateColorLevel(const double delta)
{
	if (false == m_isBeautyOn)
	{
		return;
	}
	m_curColorLevel += delta;
	m_curColorLevel = min(max(0.0, m_curColorLevel), 1);
	fuItemSetParamd(m_beautyHandles, "color_level", m_curColorLevel);
}

void NE::Nama::UpdateBlurLevel(const double delta)
{
	if (false == m_isBeautyOn)
	{
		return;
	}
	m_curBlurLevel += delta;
	m_curBlurLevel = min(max(0.0, m_curBlurLevel), 6.0);
	fuItemSetParamd(m_beautyHandles, "blur_level", m_curBlurLevel);
}

void NE::Nama::UpdateCheekThinning(const double delta)
{
	if (false == m_isBeautyOn)
	{
		return;
	}
	m_curCheekThinning += delta;
	m_curCheekThinning = min(max(0.0, m_curCheekThinning), 2.0);
	fuItemSetParamd(m_beautyHandles, "cheek_thinning", m_curCheekThinning);
}

void NE::Nama::UpdateEyeEnlarging(const double delta)
{
	if (false == m_isBeautyOn)
	{
		return;
	}
	m_curEyeEnlarging += delta;
	m_curEyeEnlarging = min(max(0.0, m_curEyeEnlarging), 4.0);
	fuItemSetParamd(m_beautyHandles, "eye_enlarging", m_curEyeEnlarging);
}

std::tr1::shared_ptr<unsigned char> NE::Nama::NextFrame()
{
	std::tr1::shared_ptr<unsigned char> frame = m_cap->QueryFrame();
	switch (m_mode)
	{
	case PROP:
		if (true == m_isBeautyOn)
		{
			int handle[2] = {m_beautyHandles, m_propHandles[m_curBundleIdx] };
			fuRenderItems(0, reinterpret_cast<int*>(frame.get()), m_frameWidth, m_frameHeight, m_frameID, handle, 2);
		}
		else
		{
			fuRenderItems(0, reinterpret_cast<int*>(frame.get()), m_frameWidth, m_frameHeight, m_frameID, &m_propHandles[m_curBundleIdx], 1);
		}

		break;
	case LANDMARK:
		fuRenderItems(0, reinterpret_cast<int*>(frame.get()), m_frameWidth, m_frameHeight, m_frameID, NULL, 0);
		DrawLandmarks(frame);
		break;
	default:
		break;
	}
	++m_frameID;
	
	return frame;
}

void NE::Nama::CreateBundle()
{
	for (int i(0); i != g_propCount; ++i)
	{
		CreateBundle(i);
	}
}

void NE::Nama::CreateBundle(const int idx)
{
	if(0 == m_propHandles[idx])
	{
		std::vector<char> propData;
		if (false == NE::LoadBundle(g_fuDataDir + g_propName[idx], propData))
		{
			std::cout << "load prop data failed." << std::endl;
			exit(1);
		}
		std::cout << "load prop data." << std::endl;

		m_propHandles[idx] = fuCreateItemFromPackage(&propData[0], propData.size());
	}
}

void NE::Nama::DrawLandmarks(std::tr1::shared_ptr<unsigned char> frame)
{
	double landmarks[150];
	int ret = fuGetFaceInfo(0, "landmarks", landmarks, sizeof(landmarks) / sizeof(landmarks[0]));	
	for (int i(0); i != 75; ++i)
	{
		DrawPoint(frame, static_cast<int>(landmarks[2 * i]), static_cast<int>(landmarks[2 * i + 1]));
	}

	double quat[4];
	ret = fuGetFaceInfo(0, "rotation_raw", quat, sizeof(quat) / sizeof(quat[0]));
	//double angle = 2 * acos(quat[3]);
	//printf("Rotation angle = %f\n", angle / 3.1415926 * 180.0);

	/*
	double rmode[1];
	ret = fuGetFaceInfo(0, "rotation_mode", rmode, 1);

	
	double quat2[4];
	double alpha;
	double beta[3];
	alpha = (rmode[0] * 90.0) / 180.0 * 3.1415926;
	beta[0] = 0.0; beta[1] = 0.0; beta[2] = 1.0;
	double sinValue = std::sin(alpha * 0.5);
	double cosValue = std::cos(alpha * 0.5);
	quat2[0] = sinValue * beta[0];
	quat2[1] = sinValue * beta[1];
	quat2[2] = sinValue * beta[2];
	quat2[3] = cosValue;
	
	// quat mult
	double tw = (quat[3] * quat2[3]) - (quat[0] * quat2[0] + quat[1] * quat2[1] + quat[2] * quat2[2]);//std::dot(v, q.v);
	double tx = (quat[1] * quat2[2]) - (quat[2] * quat2[1]) + quat2[3] * quat[0] + quat[3] * quat2[0];
	double ty = (quat[2] * quat2[0]) - (quat[0] * quat2[2]) + quat2[3] * quat[1] + quat[3] * quat2[1];
	double tz = (quat[0] * quat2[1]) - (quat[1] * quat2[0]) + quat2[3] * quat[2] + quat[3] * quat2[2];	
	quat[0] = tx;
	quat[1] = ty;
	quat[2] = tz;
	quat[3] = tw;
	

	printf("a- quat = %f %f %f %f\n", quat[0], quat[1], quat[2], quat[3]);
	*/

	double roll, pitch, yaw;	
	{
		double ysqr = quat[1] * quat[1];
		// roll (x-axis rotation)
		double t0 = +2.0 * (quat[3] * quat[0] + quat[1] * quat[2]);
		double t1 = +1.0 - 2.0 * (quat[0] * quat[0] + ysqr);
		roll = std::atan2(t0, t1);
		// pitch (y-axis rotation)
		double t2 = +2.0 * (quat[3] * quat[1] - quat[2] * quat[0]);
		t2 = t2 > 1.0 ? 1.0 : t2;
		t2 = t2 < -1.0 ? -1.0 : t2;
		pitch = std::asin(t2);
		// yaw (z-axis rotation)
		double t3 = +2.0 * (quat[3] * quat[2] + quat[0] * quat[1]);
		double t4 = +1.0 - 2.0 * (ysqr + quat[2] * quat[2]);
		yaw = std::atan2(t3, t4);
		printf("a- roll=%f pitch=%f yaw=%f\n", roll / 3.1415926 * 180.0, pitch / 3.1415926 * 180.0, yaw / 3.1415926 * 180.0);
	}

	/*
	yaw += (rmode[0] * 90.0) / 180.0 * 3.1415926;

	{		
		double t0 = std::cos(yaw * 0.5);
		double t1 = std::sin(yaw * 0.5);
		double t2 = std::cos(roll * 0.5);
		double t3 = std::sin(roll * 0.5);
		double t4 = std::cos(pitch * 0.5);
		double t5 = std::sin(pitch * 0.5);

		quat[3] = t0 * t2 * t4 + t1 * t3 * t5;
		quat[0] = t0 * t3 * t4 - t1 * t2 * t5;
		quat[1] = t0 * t2 * t5 + t1 * t3 * t4;
		quat[2] = t1 * t2 * t4 - t0 * t3 * t5;
		printf("b- quat = %f %f %f %f\n", quat[0], quat[1], quat[2], quat[3]);
	}

	{
		double ysqr = quat[1] * quat[1];
		// roll (x-axis rotation)
		double t0 = +2.0 * (quat[3] * quat[0] + quat[1] * quat[2]);
		double t1 = +1.0 - 2.0 * (quat[0] * quat[0] + ysqr);
		roll = std::atan2(t0, t1);
		// pitch (y-axis rotation)
		double t2 = +2.0 * (quat[3] * quat[1] - quat[2] * quat[0]);
		t2 = t2 > 1.0 ? 1.0 : t2;
		t2 = t2 < -1.0 ? -1.0 : t2;
		pitch = std::asin(t2);
		// yaw (z-axis rotation)
		double t3 = +2.0 * (quat[3] * quat[2] + quat[0] * quat[1]);
		double t4 = +1.0 - 2.0 * (ysqr + quat[2] * quat[2]);
		yaw = std::atan2(t3, t4);
		printf("b- roll=%f pitch=%f yaw=%f rmode = %f\n", roll / 3.1415926 * 180.0, pitch / 3.1415926 * 180.0, yaw / 3.1415926 * 180.0, rmode[0]);
	}
	*/
}

void NE::Nama::DrawPoint(std::tr1::shared_ptr<unsigned char> frame, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	const int offsetX[] = { -1, 0, 1 , -1, 0, 1 , -1, 0, 1 };
	const int offsetY[] = {-1, -1, -1, 0, 0, 0, 1, 1, 1};

	const int count = sizeof(offsetX) / sizeof(offsetX[0]);

	unsigned char* data = frame.get();
	for (int i(0); i != count; ++i)
	{
		int xx = x + offsetX[i];
		int yy = y + offsetY[i];
		if (0 > xx || xx >= m_frameWidth || 0 > yy || yy >= m_frameHeight)
		{
			continue;
		}

		data[yy * 4 * m_frameWidth + xx * 4 + 0] = b;
		data[yy * 4 * m_frameWidth + xx * 4 + 1] = g;
		data[yy * 4 * m_frameWidth + xx * 4 + 2] = r;
	}

}
