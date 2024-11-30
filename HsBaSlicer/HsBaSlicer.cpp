// HsBaSlicer.cpp: 定义应用程序的入口点。
//

#include "HsBaSlicer.h"

#include "utils/logger.hpp"
#include "DllHsBaSlicer/initialize.h"

using namespace std;

int main()
{
	auto& log = HsBa::Slicer::Log::LoggerSingletone::GetInstance();
	if (log.UseLogFile())
	{
		log.LogInfo("use log file");
	}
	else
	{
		log.LogWarning("not use log file");
	}
	initialize();
	cout << "Hello CMake." << endl;
	HsBa::Slicer::Log::LoggerSingletone::DeleteInstance();
	return 0;
}
