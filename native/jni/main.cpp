#include "utils/logging.h"
#include "utils/addresses.h"
#include "utils/armhook.h"
#include "main.h"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <jni.h>

uintptr_t libSize = 0;
uintptr_t libHandle = 0;

uintptr_t raknetSize = 0;
uintptr_t raknetHandle = 0;

JavaVM* g_jvm = nullptr;
jobject g_activity = nullptr;
char g_package[256] = {0};


#define HOOK_LIBRARY "libsamp.so"
#define HOOK_RAKNET "libraknet.so"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_arzmod_radare_InitGamePatch_setActivity(JNIEnv* env, jclass clazz, jobject activity) {
    if (g_activity != nullptr) {
        env->DeleteGlobalRef(g_activity);
    }
    g_activity = env->NewGlobalRef(activity);
}

__attribute__((constructor))
void init() {
    char path[64] = {0};
    char package[256] = {0};
    pid_t pid = getpid();
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE* fp = fopen(path, "r");
    size_t size = fread(package, 1, sizeof(package) - 1, fp);
    fclose(fp);
    if (size > 0) {
        package[size] = '\0';
        strncpy(g_package, package, sizeof(g_package) - 1);
        g_package[sizeof(g_package) - 1] = '\0';
    }
    #ifdef __arm__
        libHandle = FindLibrary(HOOK_LIBRARY);
        if(libHandle == 0)
        {
            char prefix[256] = {0};
            strncpy(prefix, HOOK_LIBRARY, sizeof(prefix) - 1);
            char* dot = strrchr(prefix, '.');
            if(dot) *dot = '\0';
            LOGI("Not found %s, trying to find by prefix %s", HOOK_LIBRARY, prefix);
            LibraryInfo libInfo = FindLibraryByPrefix(prefix);
            if(libInfo.address == 0 || libInfo.name[0] == '\0')
            {
                Log("Not found %s or any library starting with %s", HOOK_LIBRARY, prefix);
                pid_t pid = getpid();
                kill(pid, SIGKILL);
            }
            LOGI("Found library by prefix at address: %x with name: %s", libInfo.address, libInfo.name);
            libHandle = libInfo.address;
            libSize = GetLibrarySize(libInfo.name);
            InitHookStuff(libInfo.name);
        }
        else
        {
            libSize = GetLibrarySize(HOOK_LIBRARY);
            InitHookStuff(HOOK_LIBRARY);
        }

        LOGI("ARZMOD Native Init (%s) (samp_base: %x) | x%i | Build time: %s", g_package, libHandle, sizeof(void*) * 8, __DATE__ " " __TIME__);
    #elif defined __aarch64__
        libSize = GetLibrarySize(HOOK_LIBRARY);
        raknetSize = GetLibrarySize(HOOK_RAKNET);
        libHandle = FindLibrary(HOOK_LIBRARY);
        raknetHandle = FindLibrary(HOOK_RAKNET);
        LOGI("ARZMOD Native Init (%s) (samp_base: %lx | raknet_base: %lx) | x%lu | Build time: %s", g_package, libHandle, raknetHandle, sizeof(void*) * 8, __DATE__ " " __TIME__);
    #else
        #error This lib is supposed to work on ARM only!
    #endif
}


