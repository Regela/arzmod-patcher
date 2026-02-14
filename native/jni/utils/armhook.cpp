#include <iostream>
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstdarg>
#include <exception>
#include <chrono>
#include "logging.h"
#include "armhook.h"

#define HOOK_PROC "\x01\xB4\x01\xB4\x01\x48\x01\x90\x01\xBD\x00\xBF\x00\x00\x00\x00"

uintptr_t mmap_start    = 0;
uintptr_t mmap_end      = 0;
uintptr_t memlib_start  = 0;
uintptr_t memlib_end    = 0;

static size_t PAGE_COUNT = 4;

void UnFuck(uintptr_t ptr)
{
    size_t pageSize = sysconf(_SC_PAGESIZE);
    mprotect((void*)(ptr & ~(pageSize - 1)), pageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
}

void NOP(uintptr_t addr, unsigned int count)
{
    UnFuck(addr);

    for(uintptr_t ptr = addr; ptr != (addr+(count*2)); ptr += 2)
    {
        *(char*)ptr = 0x00;
        *(char*)(ptr+1) = 0x46;
    }

    #ifdef __aarch64__
        __builtin___clear_cache((char*)addr, (char*)(addr + count*2));
    #elif __arm__
        cacheflush(addr, (uintptr_t)(addr + count*2), 0);
    #else
        #error "Unsupported architecture"
    #endif
}

void WriteMemory(uintptr_t dest, uintptr_t src, size_t size)
{
    UnFuck(dest);
    memcpy((void*)dest, (void*)src, size);
    #ifdef __aarch64__
        __builtin___clear_cache((char*)dest, (char*)(dest + size));
        __builtin___clear_cache((char*)src, (char*)(src + size));
    #elif __arm__
        cacheflush(dest, dest + size, 0);
        cacheflush(src, src + size, 0);
    #endif
}

void ReadMemory(uintptr_t dest, uintptr_t src, size_t size)
{
    UnFuck(dest);
    UnFuck(src);
    memcpy((void*)dest, (void*)src, size);
    #ifdef __aarch64__
        __builtin___clear_cache((char*)dest, (char*)(dest + size));
        __builtin___clear_cache((char*)src, (char*)(src + size));
    #elif __arm__
        cacheflush(dest, dest + size, 0);
        cacheflush(src, src + size, 0);
    #endif
}

void InitHookStuff(const char* lib_name)
{
    uintptr_t libHandle = FindLibrary(lib_name);
    if(libHandle == 0)
    {
        LOGE("ERROR: %s address not found!", lib_name);
        return;
    }
    size_t size = GetLibrarySize(lib_name);
    if (size == 0) {
        LOGE("Library size is zero");
        return;
    }
    memlib_start = libHandle;
    memlib_end = memlib_start + size;

    size_t total = PAGE_SIZE * PAGE_COUNT;
    mmap_start = (uintptr_t)mmap(0, total, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_start == (uintptr_t)MAP_FAILED) {
        LOGE("mmap failed");
        return;
    }
    mmap_end = mmap_start + total;
}

static inline void WriteAbsJumpThumbAtomic(uintptr_t at, uintptr_t target)
{
    uint16_t insn_half[2] = { 0xF8DF, 0xF000 };
    uint32_t tgt = (uint32_t)(target | 1);

    WriteMemory(at + 4, (uintptr_t)&tgt, 4);
    WriteMemory(at, (uintptr_t)insn_half, 4);
}

static uintptr_t MakeTrampoline(uintptr_t addr, size_t *out_copied)
{
    const size_t min_copy = 8;
    if (mmap_start + min_copy + 8 > mmap_end) {
        LOGE("Not enough trampoline memory");
        return 0;
    }

    uintptr_t tramp = mmap_start;

    ReadMemory(tramp, addr, min_copy);

    uintptr_t return_addr = addr + min_copy;
    WriteAbsJumpThumbAtomic(tramp + min_copy, return_addr);

    *out_copied = min_copy;
    mmap_start += (min_copy + 8 + 0xF) & ~0xF; 
    return tramp;
}

void SetUpHook(uintptr_t addr, uintptr_t func, uintptr_t *orig)
{
    const size_t need_for_stub = 8 + 8;
    if (mmap_start + need_for_stub > mmap_end) {
        LOGE("space limit reached for mmap stubs");
        std::terminate();
    }

    size_t copied = 0;
    uintptr_t tramp = MakeTrampoline(addr, &copied);
    if (!tramp) {
        LOGE("Failed to make trampoline");
        std::terminate();
    }

    *orig = tramp | 1;
    WriteAbsJumpThumbAtomic(addr, func);
}

void InstallMethodHook(uintptr_t addr, uintptr_t func)
{
    UnFuck(addr);
    *(uintptr_t*)addr = func;
}

#ifdef __arm__
int PatternHook(const std::string& pattern, uintptr_t start, size_t length, uintptr_t func, uintptr_t *orig, const char* tag, bool isDobbyUsed)
#elif defined __aarch64__
int PatternHook(const std::string& pattern, uintptr_t start, size_t length, uintptr_t func, uintptr_t *orig, const char* tag, bool isDobbyUsed)
#endif
{
    std::string pattern_str = pattern + " at " + std::to_string(start);
    if (hooked_pattern_hashes.count(pattern_str)) {
        #ifdef RELEASE_BUILD
            if (tag) LOGE("[%d] Already hooked: %s", bit, tag);
            else LOGE("[%d] Already hooked pattern: UNKNOWN", bit);
        #else
            if (tag) LOGE("[%d] Already hooked: %s at %s", bit, tag, pattern_str.c_str());
            else LOGE("[%d] Already hooked pattern: %s", bit, pattern_str.c_str());
        #endif
        return 1;
    }

    #ifdef RELEASE_BUILD
        if (tag) LOGD("[%d] Try to find pattern for %s", bit, tag);
        else LOGD("[%d] Try to find pattern for UNKNOWN", bit);
    #else
        if (tag) LOGD("[%d] Try to find pattern %s for %s", bit, pattern_str.c_str(), tag);
        else LOGD("[%d] Try to find pattern %s", bit, pattern_str.c_str());
    #endif
    
    auto start_time = std::chrono::high_resolution_clock::now();
    void* func_addr = FindPattern(pattern, start, length);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    if(func_addr) {
        if(isDobbyUsed) {
            DobbyHook(reinterpret_cast<void*>(func_addr), reinterpret_cast<void*>(func), reinterpret_cast<void**>(orig));
        } else {
            SetUpHook(reinterpret_cast<uintptr_t>(func_addr), func, orig);
        }
        
        hooked_pattern_hashes.insert(pattern_str);
        
        if(tag) {
            #ifdef RELEASE_BUILD
                LOGI("[%d] Hooks installed successfully (%s), pattern found in %lld μs", bit, tag, duration.count());
            #else
                #ifdef __arm__
                    LOGI("[%d] Hooks installed successfully (%s), address: %x (static %x), pattern found in %lld μs", bit, tag, (uintptr_t)func_addr, (uintptr_t)func_addr - (uintptr_t)start, duration.count());
                #elif defined __aarch64__
                    LOGI("[%d] Hooks installed successfully (%s), address: %lx (static %lx), pattern found in %lld μs", bit, tag, (uintptr_t)func_addr, (uintptr_t)func_addr - (uintptr_t)start, duration.count());
                #endif
            #endif
        }
        return (uintptr_t)func_addr;
    }
    else {
        if(tag) {
            LOGE("[%d] Can't find offset from pattern %s (%s), search time: %lld μs", bit, pattern_str.c_str(), tag, duration.count());
        }
        return 0;
    }
    return (uintptr_t)func_addr;
}