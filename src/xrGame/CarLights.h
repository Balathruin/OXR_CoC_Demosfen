
#pragma once

#include "xrEngine/Render.h"

class CCarLights;
class CCar;

struct SCarLight
{
    ref_light light_render;
	ref_light light_omni;
    ref_glow glow_render;
    u16 bone_id;
    CCarLights* m_holder;
    SCarLight();
    ~SCarLight();
    void Switch();
    void TurnOn();
    void TurnOff();
    bool isOn();
    void Init(CCarLights* holder);
    void Update();
    void ParseDefinitions(LPCSTR section);
};

using LIGHTS_STORAGE = xr_vector<SCarLight*>;
class CCarLights
{
public:
    void ParseDefinitions();
    void Init(CCar* pcar);
    void Update();
    CCar* PCar() { return m_pcar; }
    void SwitchHeadLights();
    void TurnOnHeadLights();
    void TurnOffHeadLights();
    bool IsLight(u16 bone_id);
    bool findLight(u16 bone_id, SCarLight*& light);
    CCarLights();
    ~CCarLights();

protected:
    struct SFindLightPredicate
    {
        const SCarLight* m_light;

        SFindLightPredicate(const SCarLight* light) : m_light(light) {}
        bool operator()(const SCarLight* light) const { return light->bone_id == m_light->bone_id; }
    };
    LIGHTS_STORAGE m_lights;
    CCar* m_pcar;

private:
};
