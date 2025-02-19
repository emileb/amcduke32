// Made possible by the Build Engine linking exception.

#if (defined _WIN32 || defined __linux__ || defined EDUKE32_OSX) && !defined(__ANDROID__)
# define VW_ENABLED
#endif

#ifdef VW_ENABLED
# define VOIDWRAP_RUNTIMELINK
# include "voidwrap_steam.h"
#endif

#include "build.h"
#include "communityapi.h"

#ifdef VW_ENABLED

static bool steamworks_enabled;
static VW_LIBHANDLE wrapper_handle;

#ifdef _WIN32
# ifdef _WIN64
static char const wrapper_lib[] = "voidwrap_steam_x64.dll";
# else
static char const wrapper_lib[] = "voidwrap_steam_x86.dll";
# endif
#else
static char const wrapper_lib[] = "libvoidwrap_steam.so";
#endif

#ifdef VWSCREENSHOT
static void steam_callback_screenshotrequested()
{
    DLOG_F(INFO, "Voidwrap: preparing screenshot for Steam.");
    videoCaptureScreen("steam0000.png", 0);
}

#if 0
static void steam_callback_screenshotready(int32_t result)
{
    DLOG_F(INFO, "Voidwrap: screenshot ready: %d", result);
}
#endif
#endif

static void steam_callback_printdebug(char const * str)
{
    UNREFERENCED_PARAMETER(str);
    DLOG_F(INFO, "%s: %s", wrapper_lib, str);
}
#endif


void communityapiInit()
{
#ifdef VW_ENABLED
    DLOG_F(INFO, "Voidwrap: found %s", wrapper_lib);

    wrapper_handle = Voidwrap_LoadLibrary(wrapper_lib);

    if (wrapper_handle == nullptr)
    {
#ifdef _WIN32
        DLOG_F(ERROR, "Voidwrap: failed loading %s.", wrapper_lib);
#else
        DLOG_F(ERROR, "Voidwrap: failed loading %s: dlopen error: %s", wrapper_lib, dlerror());
#endif
        return;
    }

    Voidwrap_Steam_Init = (VW_BOOL)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_Init");
    Voidwrap_Steam_Shutdown = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_Shutdown");
    Voidwrap_Steam_RunCallbacks = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_RunCallbacks");

    if (nullptr != (Voidwrap_Steam_SetCallback_PrintDebug = (VW_SETCALLBACK_VOID_CONSTCHARPTR)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_PrintDebug")))
        Voidwrap_Steam_SetCallback_PrintDebug(steam_callback_printdebug);

    Voidwrap_Steam_UnlockAchievement = (VW_VOID_CONSTCHARPTR)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_UnlockAchievement");
    Voidwrap_Steam_SetStat = (VW_VOID_CONSTCHARPTR_INT32)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetStat");
    Voidwrap_Steam_ResetStats = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_ResetStats");
    Voidwrap_Steam_SetRichPresence = (VW_VOID_CONSTCHARPTR_CONSTCHARPTR)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetRichPresence");
    Voidwrap_Steam_ClearRichPresence = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_ClearRichPresence");
#ifdef VWSCREENSHOT
    Voidwrap_Steam_SendScreenshot = (VW_BOOL_SCREENSHOT)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SendScreenshot");
    Voidwrap_Steam_SetCallback_ScreenshotRequested = (VW_SETCALLBACK_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_ScreenshotRequested");
    Voidwrap_Steam_SetCallback_ScreenshotRequested(steam_callback_screenshotrequested);
#if 0
    Voidwrap_Steam_SetCallback_ScreenshotReady = (VW_SETCALLBACK_VOID_INT32)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_ScreenshotReady");
    Voidwrap_Steam_SetCallback_ScreenshotReady(steam_callback_screenshotready);
#endif
#endif

    if (Voidwrap_Steam_Init == nullptr || Voidwrap_Steam_RunCallbacks == nullptr)
    {
        DLOG_F(ERROR, "Voidwrap: fatal error: required symbols are missing.");
        return;
    }
    else if (!Voidwrap_Steam_Init())
    {
        DLOG_F(WARNING, "Voidwrap: Steamworks initialization failed.");
        return;
    }

    DLOG_F(INFO, "Voidwrap: Steamworks initialized.");

    steamworks_enabled = true;
#endif
}

void communityapiShutdown()
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_Shutdown)
        return;

    Voidwrap_Steam_Shutdown();
#endif
}

void communityapiRunCallbacks()
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    Voidwrap_Steam_RunCallbacks();
#endif
}

bool communityapiEnabled()
{
#ifdef VW_ENABLED
    return steamworks_enabled;
#else
    return false;
#endif
}

char const *communityApiGetPlatformName()
{
#ifdef VW_ENABLED
    return "Steam";
#else
    return NULL;
#endif
}

void communityapiUnlockAchievement(char const * id)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_UnlockAchievement)
        return;

    Voidwrap_Steam_UnlockAchievement(id);
#else
    UNREFERENCED_PARAMETER(id);
#endif
}

void communityapiSetStat(char const * id, int32_t value)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_SetStat)
        return;

    Voidwrap_Steam_SetStat(id, value);
#else
    UNREFERENCED_PARAMETER(id);
    UNREFERENCED_PARAMETER(value);
#endif
}

void communityapiResetStats()
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_ResetStats)
        return;

    Voidwrap_Steam_ResetStats();
#endif
}

void communityapiSetRichPresence(char const * key, char const * str)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_SetRichPresence)
        return;

    Voidwrap_Steam_SetRichPresence(key, str);
#else
    UNREFERENCED_PARAMETER(str);
#endif
}

void communityapiClearRichPresence(void)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    if (nullptr == Voidwrap_Steam_ClearRichPresence)
        return;

    Voidwrap_Steam_ClearRichPresence();
#endif
}

#ifdef VWSCREENSHOT
void communityapiSendScreenshot(char * filename)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    char fullpath[BMAX_PATH];
    buildvfs_getcwd(fullpath, sizeof(fullpath));
    Bstrcat(fullpath, "/");
    Bstrcat(fullpath, filename);

    DVLOG_F(LOG_DEBUG, "Steam screenshot path: %s", fullpath);

    Voidwrap_Steam_SendScreenshot(fullpath, xdim, ydim);
#else
    UNREFERENCED_PARAMETER(filename);
#endif
}
#endif
