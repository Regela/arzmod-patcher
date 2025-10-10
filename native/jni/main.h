#pragma once

#include <cstdint>
#include <jni.h>

extern uintptr_t libSize;
extern uintptr_t libHandle;

extern uintptr_t raknetSize;
extern uintptr_t raknetHandle;

extern JavaVM* g_jvm;
extern jobject g_activity;
extern char g_package[256];

#define HOOK_LIBRARY "libsamp.so"
#define HOOK_RAKNET "libraknet.so"
