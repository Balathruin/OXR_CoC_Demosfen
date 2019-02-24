#include "stdafx.h"

#include "SoundRender_CoreA.h"

XRSOUND_API xr_token* snd_devices_token = nullptr;
XRSOUND_API u32 snd_device_id = u32(-1);

ALDeviceList* deviceList = nullptr;

void ISoundManager::_create()
{
    if (deviceList == nullptr)
        enumerate_devices();
    SoundRenderA = new CSoundRender_CoreA(deviceList);
    SoundRender = SoundRenderA;
    GEnv.Sound = SoundRender;
    SoundRender->bPresent = strstr(Core.Params, "-nosound") == nullptr;
    if (!SoundRender->bPresent)
        return;
    GEnv.Sound->_initialize();
}

void ISoundManager::_destroy()
{
    GEnv.Sound->_clear();
    xr_delete(SoundRender);
    GEnv.Sound = nullptr;
    xr_delete(deviceList);
}

void ISoundManager::enumerate_devices()
{
    deviceList = new ALDeviceList();
}
