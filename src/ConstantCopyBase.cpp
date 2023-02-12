#include "ConstantCopyBase.h"

using namespace ConstantFeedback;

ConstantHandlerBase* ConstantCopyBase::_constHandler = nullptr;

ConstantCopyBase::ConstantCopyBase()
{

}

ConstantCopyBase::~ConstantCopyBase()
{

}

void ConstantCopyBase::SetConstantHandler(ConstantHandlerBase* constantHandler)
{
    _constHandler = constantHandler;
}

std::string ConstantCopyBase::GetExecutableName()
{
    char fileName[MAX_PATH + 1];
    DWORD charsWritten = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
    if (charsWritten != 0)
    {
        std::string ret(fileName);
        std::size_t found = ret.find_last_of("/\\");
        return ret.substr(found + 1);
    }

    return std::string();
}

template<typename T>
T* ConstantCopyBase::InstallHook(void* target, T* callback)
{
    void* original_function = nullptr;

    if (MH_CreateHook(target, reinterpret_cast<void*>(callback), &original_function) != MH_OK)
        return nullptr;

    return reinterpret_cast<T*>(original_function);
}

template<typename T>
T* ConstantCopyBase::InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, T* callback)
{
    void* original_function = nullptr;

    if (MH_CreateHookApi(pszModule, pszProcName, reinterpret_cast<void*>(callback), &original_function) != MH_OK)
        return nullptr;

    return reinterpret_cast<T*>(original_function);
}

// Template instances
template sig_memcpy* ConstantCopyBase::InstallHook(void* target, sig_memcpy* callback);
template sig_memcpy* ConstantCopyBase::InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, sig_memcpy* callback);

template sig_ffxiv_cbload* ConstantCopyBase::InstallHook(void* target, sig_ffxiv_cbload* callback);
template sig_ffxiv_cbload* ConstantCopyBase::InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, sig_ffxiv_cbload* callback);