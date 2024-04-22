#include <cstring>
#include <intrin.h>
#include <d3d11.h>
#include "ConstantCopyFFXIV.h"

using namespace Shim::Constants;
using namespace reshade::api;
using namespace std;

sig_ffxiv_cbload0* ConstantCopyFFXIV::org_ffxiv_cbload0 = nullptr;
sig_ffxiv_cbload1* ConstantCopyFFXIV::org_ffxiv_cbload1 = nullptr;
sig_ffxiv_memcpy* ConstantCopyFFXIV::org_ffxiv_memcpy = nullptr;
vector<tuple<const void*, uint64_t, size_t, bool>> ConstantCopyFFXIV::_hostResourceBuffer;
unordered_map<uint64_t, uint64_t> ConstantCopyFFXIV::_hostResourceBufferMap;

ConstantCopyFFXIV::ConstantCopyFFXIV()
{
}

ConstantCopyFFXIV::~ConstantCopyFFXIV()
{
}

bool ConstantCopyFFXIV::Init()
{
    return Shim::GameHookT<sig_ffxiv_cbload0>::Hook(&org_ffxiv_cbload0, detour_ffxiv_cbload0, ffxiv_cbload0) &&
        /*Shim::GameHookT<sig_ffxiv_cbload1>::Hook(&org_ffxiv_cbload1, detour_ffxiv_cbload1, ffxiv_cbload1) &&*/
        Shim::GameHookT<sig_ffxiv_memcpy>::Hook(&org_ffxiv_memcpy, detour_ffxiv_memcpy, ffxiv_memcpy);
}

bool ConstantCopyFFXIV::UnInit()
{
    return MH_Uninitialize() == MH_OK;
}

void ConstantCopyFFXIV::GetHostConstantBuffer(command_list* cmd_list, ShaderToggler::ToggleGroup* group, vector<uint8_t>& dest, size_t size, uint64_t resourceHandle)
{
    const auto& ff = _hostResourceBufferMap.find(resourceHandle);
    if (ff != _hostResourceBufferMap.end())
    {
        auto& [buffer, bufHandle, bufSize, mapped] = _hostResourceBuffer[ff->second];
        size_t minSize = std::min(size, bufSize);
        memcpy(dest.data(), buffer, minSize);
    }
}

inline void ConstantCopyFFXIV::set_host_resource_data_location(void* origin, size_t len, int64_t resource_handle, size_t index)
{
    if (_hostResourceBuffer.size() < index + 1)
        _hostResourceBuffer.resize(index + 1);

    auto& [buffer, bufHandle, bufSize, mapped] = _hostResourceBuffer[index];

    if (!mapped)
        _hostResourceBufferMap[resource_handle] = index;

    buffer = origin;
    bufHandle = resource_handle;
    bufSize = len;
    mapped = true;
}

void ConstantCopyFFXIV::detour_ffxiv_cbload0(uint64_t param_1, uint16_t* param_2, uint64_t param_3, D3D11_MAPPED_SUBRESOURCE* param_4)
{
    uint32_t uVar3;
    int32_t iVar4;
    uint64_t uVar5;
    uint64_t* pauVar5;
    uint32_t uVar6;
    uint64_t* local_res8;
    D3D11_MAPPED_SUBRESOURCE auStack_28;
    ID3D11DeviceContext* plVar2;
    int64_t lVar1;
    void* pIVar2;

    pauVar5 = reinterpret_cast<uint64_t*>(param_4->pData);
    uVar6 = (uint32_t)(param_3 >> 0x10) & 0xffff;
    uVar3 = param_4->RowPitch;
    if (param_4->RowPitch < uVar6) {
        uVar3 = uVar6;
    }
    local_res8 = 0;
    if (!_BitScanReverse(reinterpret_cast<DWORD*>(&iVar4), uVar3 - 1))
        iVar4 = -1;

    if (*pauVar5 == *reinterpret_cast<uint64_t*>(param_1 + 0x10)) {
        *(uint8_t*)((param_3 & 0xff) + 8 + (int64_t)param_2) = 4;
    }
    else {
        pIVar2 = param_4->pData;
        auStack_28.RowPitch = param_4->RowPitch;
        auStack_28.DepthPitch = param_4->DepthPitch;
        plVar2 = *reinterpret_cast<ID3D11DeviceContext**>(param_1 + 8);
        uVar5 = (uint64_t)((uint32_t)(param_3 >> 0x20) & 3);
        if (iVar4 + 1U < 0xc) {
            auStack_28.pData = pIVar2;
            uVar5 = detour_ffxiv_cbload1(param_1 + 0x18 + (uVar5 + static_cast<uint64_t>(iVar4 + 1U) * 4) * 0x50, plVar2,
                &auStack_28, reinterpret_cast<ID3D11Resource**>(&local_res8), 0x18 + (uVar5 + static_cast<uint64_t>(iVar4 + 1U) * 4) * 0x50);
            pauVar5 = local_res8;
            *(uint8_t*)((param_3 & 0xff) + 8 + (int64_t)param_2) = (uint8_t)uVar5;
        }
        else {
            lVar1 = param_1 + uVar5 * 0x10;
            pauVar5 = *(uint64_t**)(lVar1 + 0xf18);
            if (*(void**)(lVar1 + 0xf20) != pIVar2) {
                set_host_resource_data_location(pIVar2, auStack_28.RowPitch * 16, (int64_t)pauVar5, (uVar5 * 0x10 + 0xf18) / 8);

                plVar2->Map(reinterpret_cast<ID3D11Resource*>(pauVar5), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &auStack_28);
                org_ffxiv_memcpy(auStack_28.pData, pIVar2, auStack_28.RowPitch * 16);
                plVar2->Unmap(reinterpret_cast<ID3D11Resource*>(pauVar5), 0);
                *(void**)(lVar1 + 0xf20) = pIVar2;
            }
            *(uint8_t*)((param_3 & 0xff) + 8 + (int64_t)param_2) = 4;
        }
    }
    *(uint64_t**)(param_2 + (param_3 & 0xff) * 4 + 0xc) = pauVar5;
    *param_2 = *param_2 | (uint16_t)(1 << ((uint32_t)param_3 & 0x1f));
    return;
}


uint64_t __fastcall ConstantCopyFFXIV::detour_ffxiv_cbload1(uintptr_t param_1, ID3D11DeviceContext* param_2, D3D11_MAPPED_SUBRESOURCE* param_3, ID3D11Resource** param_4, uint64_t index)
{
    void* pauVar1;
    ID3D11Resource* uVar2;
    uint32_t uVar3;
    uint32_t uVar4;
    uint64_t uVar5;
    int32_t iVar6;
    uint32_t uVar7;
    void** ppauVar8;
    D3D11_MAPPED_SUBRESOURCE apauStack_28;
    
    pauVar1 = param_3->pData;
    ppauVar8 = (void**)(param_1 + 0x20);
    uVar5 = 0;
    do {
        uVar4 = (uint32_t)uVar5;
        if (*ppauVar8 == pauVar1) {
            *(uint32_t*)(param_1 + 0x48) = *(uint32_t*)(param_1 + 0x48) + 1;
            if ((*(uint32_t*)(param_1 + 0x40) & 7) != uVar4) {
                *(uint32_t*)(param_1 + 0x40) = *(uint32_t*)(param_1 + 0x40) * 8 | uVar4;
            }
            *param_4 = *(ID3D11Resource**)(param_1 + uVar5 * 8);
            return uVar5;
        }
        uVar5 = (uint64_t)(uVar4 + 1);
        ppauVar8 = ppauVar8 + 1;
    } while (uVar4 + 1 < 4);
    *(uint32_t*)(param_1 + 0x4c) = *(uint32_t*)(param_1 + 0x4c) + 1;
    uVar7 = 0;
    uVar4 = 0;
    do {
        uVar3 = 1 << ((uint8_t)(*(uint32_t*)(param_1 + 0x40) >> ((uint8_t)uVar7 & 0x1f)) & 3) | uVar4;
        if (uVar3 == 0xf) break;
        uVar7 = uVar7 + 3;
        uVar4 = uVar3;
    } while (uVar7 < 0x1e);
    uVar7 = *(uint32_t*)(param_1 + 0x44) + 1U & 3;
    uVar4 = ~((uVar4 << 4 | uVar4) >> (int8_t)uVar7);
    iVar6 = 0;
    if (uVar4 != 0) {
        for (; (uVar4 >> iVar6 & 1) == 0; iVar6 = iVar6 + 1) {
        }
    }
    if (uVar4 == 0) {
        iVar6 = 0;
    }
    uVar4 = iVar6 + uVar7 & 3;
    uVar5 = (uint64_t)uVar4;
    *(void**)(param_1 + 0x20 + uVar5 * 8) = pauVar1;
    *(uint32_t*)(param_1 + 0x44) = uVar7;
    if ((*(uint32_t*)(param_1 + 0x40) & 7) != uVar4) {
        *(uint32_t*)(param_1 + 0x40) = *(uint32_t*)(param_1 + 0x40) * 8 | uVar4;
    }

    uVar2 = *reinterpret_cast<ID3D11Resource**>(param_1 + uVar5 * 8);
    *param_4 = uVar2;

    set_host_resource_data_location(param_3->pData, param_3->RowPitch * 16, (int64_t)uVar2, (index + uVar5 * 8) / 8);

    param_2->Map(uVar2, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &apauStack_28);
    org_ffxiv_memcpy(apauStack_28.pData, param_3->pData, param_3->RowPitch * 16);
    param_2->Unmap(uVar2, 0);
    
    return uVar5;
}

void __fastcall ConstantCopyFFXIV::detour_ffxiv_memcpy(void* param_1, void* param_2, uintptr_t param_3)
{
    return org_ffxiv_memcpy(param_1, param_2, param_3);
}
