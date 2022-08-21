#pragma once

#include <string>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <windows.h>
#include <shared_mutex>

using namespace std;

class ExternalVariablesPipe
{
public:
	ExternalVariablesPipe();
	~ExternalVariablesPipe();
	unordered_map<string, float> GetVariableValues();
	void StopPipe();
private:
	std::thread* _commThread;
	unordered_map<string, float> _variableValues;
	HANDLE _hPipe;
	std::shared_mutex _mutex;
	bool _running;

	void Comm();
};