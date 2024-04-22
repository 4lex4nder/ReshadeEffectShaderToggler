#include "GameHookT.h"

using namespace Shim;
using namespace std;

bool GameHook::_hooked = false;

template<typename T>
string GameHookT<T>::GetExecutableName()
{
    char fileName[MAX_PATH + 1];
    DWORD charsWritten = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
    if (charsWritten != 0)
    {
        string ret(fileName);
        size_t found = ret.find_last_of("/\\");
        return ret.substr(found + 1);
    }

    return string();
}

template<typename T>
T* GameHookT<T>::InstallHook(void* target, T* callback)
{
    void* original_function = nullptr;

    if (MH_CreateHook(target, reinterpret_cast<void*>(callback), &original_function) != MH_OK)
        return nullptr;

    return reinterpret_cast<T*>(original_function);
}

template<typename T>
T* GameHookT<T>::InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, T* callback)
{
    void* original_function = nullptr;

    if (MH_CreateHookApi(pszModule, pszProcName, reinterpret_cast<void*>(callback), &original_function) != MH_OK)
        return nullptr;

    return reinterpret_cast<T*>(original_function);
}

template<typename T>
bool GameHookT<T>::Hook(T** original, T* detour, const sigmatch::signature& sig)
{
    if (!_hooked)
    {
        // Initialize MinHook.
        if (MH_Initialize() != MH_OK)
        {
            return false;
        }

        _hooked = true;
    }

    string exe = GetExecutableName();
    if (exe.length() == 0)
        return false;

    sigmatch::this_process_target target;
    sigmatch::search_result result = target.in_module(exe).search(sig);

    for (const std::byte* address : result.matches()) {
        void* adr = static_cast<void*>(const_cast<std::byte*>(address));
        *original = InstallHook(adr, detour);
    }

    if (original != nullptr)
        return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;

    return false;
}

template<typename T>
bool GameHookT<T>::Unhook()
{
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
    {
        return false;
    }

    return true;
}

template class GameHookT<sig_ffxiv_texture_create>;
template class GameHookT<sig_ffxiv_textures_create>;
template class GameHookT<sig_ffxiv_textures_recreate>;
template class GameHookT<sig_memcpy>;
template class GameHookT<sig_ffxiv_cbload0>;
template class GameHookT<sig_ffxiv_cbload1>;
template class GameHookT<sig_ffxiv_memcpy>;
template class GameHookT<sig_nier_replicant_cbload>;