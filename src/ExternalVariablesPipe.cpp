#pragma once

#include <vector>
#include "ExternalVariablesPipe.h"

ExternalVariablesPipe::ExternalVariablesPipe()
{
    _commThread = new std::thread(&ExternalVariablesPipe::Comm, this);
    _hPipe = INVALID_HANDLE_VALUE;
    _running = true;
}

ExternalVariablesPipe::~ExternalVariablesPipe()
{
    if (_hPipe != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(_hPipe);
    }
}

void ExternalVariablesPipe::StopPipe()
{
    _running = false;
    DisconnectNamedPipe(_hPipe);
}

unordered_map<string, float> ExternalVariablesPipe::GetVariableValues()
{
    std::unique_lock<shared_mutex>(mutex);
    unordered_map<string, float> variables = _variableValues;
    return variables;
}

vector<string> splitString(string& str, char splitter) {
    vector<string> result;
    string current = "";
    for (int i = 0; i < str.size(); i++) {
        if (str[i] == splitter) {
            if (current != "") {
                result.push_back(current);
                current = "";
            }
            continue;
        }
        current += str[i];
    }
    if (current.size() != 0)
        result.push_back(current);
    return result;
}

void ExternalVariablesPipe::Comm()
{
    char _buffer[1024];
    DWORD _dwRead;

    _hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\ReshadeEffectShaderToggler"),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
        1,
        1024 * 16,
        1024 * 16,
        NMPWAIT_USE_DEFAULT_WAIT,
        NULL);
    while (_hPipe != INVALID_HANDLE_VALUE)
    {
        if (ConnectNamedPipe(_hPipe, NULL) != FALSE)   // wait for someone to connect to the pipe
        {
            while (_running)
            {
                while (ReadFile(_hPipe, _buffer, sizeof(_buffer) - 1, &_dwRead, NULL) != FALSE)
                {
                    /* add terminating zero */
                    _buffer[_dwRead] = '\0';

                    string strBuffer(_buffer);
                    vector<string> delim = splitString(strBuffer, '|');

                    {
                        std::unique_lock<shared_mutex>(mutex);
                        _variableValues[delim[0]] = std::stof(delim[1]);
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::duration(500ms));

        DisconnectNamedPipe(_hPipe);
    }
}