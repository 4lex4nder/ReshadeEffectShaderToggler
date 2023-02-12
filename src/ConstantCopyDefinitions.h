#pragma once

using sig_memcpy = void* (__fastcall)(void*, void*, size_t);
using sig_ffxiv_cbload = void (__fastcall)(int64_t, uint16_t*, uint64_t, intptr_t**);