#include <jni.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <utils/logging.h>
#include <utils/addresses.h>
#include <utils/armhook.h>
#include "main.h"
#include <string>
#include "offsets.h"


#ifdef __arm__
struct VersionStringData {
    char* saved_string = nullptr;
    char* saved_dest = nullptr;
    char* saved_version = nullptr;
    char* saved_commit = nullptr;
    bool is_patched = false;

    ~VersionStringData() {
        if(saved_string) free(saved_string);
        if(saved_version) free(saved_version);
        if(saved_commit) free(saved_commit);
    }
};
static VersionStringData version_data;

void (*VersionRenderer)(int param_1, int param_2) = nullptr;
void VersionRendererHook(int param_1, int param_2)
{
    if(version_data.saved_dest == nullptr && strlen((char*)(param_1 + 0x53)) > 0)
    {
        version_data.saved_dest = (char*)(param_1 + 0x53);
        
        char version[32] = {0};
        char commit[32] = {0};
        
        sscanf(version_data.saved_dest, "Ver: %[^,], Native: %[^-]", version, commit);
        
        if (version_data.saved_version) free(version_data.saved_version);
        version_data.saved_version = strdup(version);
        
        if (version_data.saved_commit) free(version_data.saved_commit);
        version_data.saved_commit = strdup(commit);
    
        if(version_data.saved_string) {
            std::string str(version_data.saved_string);
            if(str.find("{version}") != std::string::npos) {
                str.replace(str.find("{version}"), 9, version_data.saved_version);
            }
            if(str.find("{commit}") != std::string::npos) {
                str.replace(str.find("{commit}"), 8, version_data.saved_commit);
            }
            strcpy(version_data.saved_dest, str.c_str());
        } else {
            *version_data.saved_dest = '\0';
        }
    }
    VersionRenderer(param_1, param_2);
}


struct ChatRendererData {
    bool is_patched = false;
    bool is_set = false;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
};
static ChatRendererData chat_data;

void (*ChatRenderer)(int param_1, int param_2) = nullptr;
void ChatRendererHook(int param_1, int param_2)
{    
    if (chat_data.is_set) {
        *(float*)(param_1 + 12) = chat_data.pos_x;
        *(float*)(param_1 + 16) = chat_data.pos_y;
        
        *(float*)(param_1 + 44) = chat_data.pos_x;
        *(float*)(param_1 + 48) = chat_data.pos_y;
    }
    
    return ChatRenderer(param_1, param_2);
}
#endif


struct HudData {
    int hud_type = 3;
    int radar_type = 0;
    bool is_patched = false;
    int kostyl = 0;
};
static HudData hud_data;

#ifdef __arm__
void (*InstallHud)(int param_1, int param_2) = nullptr;
void InstallHudHook(int param_1, int param_2)
{
    InstallHud(hud_data.hud_type, param_2);
}

void (*InstallRadar)(int param_1, int param_2) = nullptr;
void InstallRadarHook(int param_1, int param_2)
{
    InstallRadar(param_1, hud_data.radar_type);
} 
#elif defined __aarch64__
void (*InstallHud)(long param_1) = nullptr;
void InstallHudHook(long param_1)
{
    if(hud_data.hud_type == 3) return InstallHud(param_1);
    if(hud_data.kostyl < 5) 
    {
        InstallHud(param_1); 
        hud_data.kostyl++;
    }
}
#endif





extern "C" {
    #ifdef __arm__
    JNIEXPORT void JNICALL
    Java_com_arzmod_radare_InitGamePatch_setVersionString(JNIEnv* env, jobject thiz, jstring string) {
        if(version_data.saved_string) {
            free(version_data.saved_string);
            version_data.saved_string = nullptr;
        }
        
        const char* str = env->GetStringUTFChars(string, nullptr);
        if(str) {
            version_data.saved_string = strdup(str);
            env->ReleaseStringUTFChars(string, str);
            
            if(version_data.saved_dest) {
                std::string mod_str(version_data.saved_string);
                if(version_data.saved_version && mod_str.find("{version}") != std::string::npos) {
                    mod_str.replace(mod_str.find("{version}"), 9, version_data.saved_version);
                }
                if(version_data.saved_commit && mod_str.find("{commit}") != std::string::npos) {
                    mod_str.replace(mod_str.find("{commit}"), 8, version_data.saved_commit);
                }
                strcpy(version_data.saved_dest, mod_str.c_str());
                return;
            }
        }

        if(!version_data.is_patched)
        {
            int result = PatternHook(VERSION_RENDER_PATTERN, libHandle, libSize, reinterpret_cast<uintptr_t>(VersionRendererHook), reinterpret_cast<uintptr_t*>(&VersionRenderer), "VersionRendererHook");
            if(result) {
                version_data.is_patched = true;
            } else {
                version_data.is_patched = true;
            }
        }
    }

    JNIEXPORT void JNICALL
    Java_com_arzmod_radare_InitGamePatch_setChatPosition(JNIEnv* env, jobject thiz, jfloat pos_x, jfloat pos_y) {
        chat_data.pos_x = pos_x;
        chat_data.pos_y = pos_y;
        chat_data.is_set = true;

        if(!chat_data.is_patched) {
            int result = PatternHook(CHAT_RENDER_PATTERN, libHandle, libSize, reinterpret_cast<uintptr_t>(ChatRendererHook), reinterpret_cast<uintptr_t*>(&ChatRenderer), "ChatRendererHook");
            if(result) {
                chat_data.is_patched = true;
            } else {
                chat_data.is_patched = true;
            }
        }
    }
    #endif
    
    JNIEXPORT void JNICALL
    Java_com_arzmod_radare_InitGamePatch_setHudType(JNIEnv* env, jobject thiz, jint hud_type, jint radar_type) {
        hud_data.hud_type = hud_type;
        hud_data.radar_type = radar_type;
        
        if(!hud_data.is_patched) {
            int result = PatternHook(INSTALL_HUD_PATTERN, libHandle, libSize, reinterpret_cast<uintptr_t>(InstallHudHook), reinterpret_cast<uintptr_t*>(&InstallHud), "InstallHudHook");
            if(result) {
                hud_data.is_patched = true;
            } else {
                hud_data.is_patched = true;
            }
            #ifdef __arm__
            result = PatternHook(INSTALL_RADAR_PATTERN, libHandle, libSize, reinterpret_cast<uintptr_t>(InstallRadarHook), reinterpret_cast<uintptr_t*>(&InstallRadar), "InstallRadarHook");
            if(result) {
                hud_data.is_patched = true;
            } else {
                hud_data.is_patched = true;
            }
            #endif
        }
    }
}


__attribute__((constructor))
void init_gamefixes() {
    #ifdef __arm__
        LOGI("GameFixes module inited | x32 | Build time: %s", __DATE__ " " __TIME__);
    #elif defined __aarch64__
        LOGI("GameFixes module inited | x64 | Build time: %s", __DATE__ " " __TIME__);
    #else
        #error "Unsupported architecture"
    #endif
} 