// HsBaSlicer.cpp: 定义应用程序的入口点。
//

#include "HsBaSlicer.h"

#include "logger/logger.hpp"
#include "DllHsBaSlicer/initialize.h"

using namespace std;

int main()
{
	auto log = HsBa::Slicer::Log::LoggerSingletone::GetInstance();
	using namespace HsBa::Slicer::Log::LogLiteral;
	if (log->UseLogFile())
	{
		"use log file"_log_info();
	}
	else
	{
		"not use log file"_log_warning();
	}
	initialize();
	"initialize"_log_info();
	cout << "Hello CMake." << endl;
	return 0;
}
