
#pragma once

#include "../xrEngine/Render.h"

class CCarSignalLights;
class CCar;
class CLAItem;

struct SCarSignalLight
{
public:
	ref_light light_render;
	ref_light light_omni;
	CLAItem*  m_lanim;
	ref_glow  glow_render;
	u16       bone_id;
	CCarSignalLights* m_holder;				
			SCarSignalLight		();
			~SCarSignalLight		();
	void	Switch			();
	void	TurnOn			();
	void	TurnOff			();
	bool	isOn			();
	void	Init			(CCarSignalLights* holder);
	void	Update			();
	void	ParseDefinitions(LPCSTR section);
	

};

DEFINE_VECTOR(SCarSignalLight*,LIGHTS_STORAGE_2,LIGHTS_I_2)
class CCarSignalLights
{
public:
	void	ParseDefinitions	()								;
	void	Init				(CCar* pcar)					;
	void	Update				()								;
	CCar*	PCar				()								{return m_pcar;}
	void	SwitchSignalLights	()								;
	void	TurnOnSignalLights	()								;
	void	TurnOffSignalLights	()								;
	bool	IsLight				(u16 bone_id)					;
	bool	findLight			(u16 bone_id,SCarSignalLight* &light);
			CCarSignalLights			()								;
			~CCarSignalLights			()								;
protected:
	struct SFindLightPredicate {
		const SCarSignalLight *m_light;

		SFindLightPredicate	(const SCarSignalLight *light) :
		m_light(light)
		{
		}

		bool		operator()					(const SCarSignalLight *light) const
		{
			return light->bone_id==m_light->bone_id;
		}
	};
	LIGHTS_STORAGE_2 m_signal_lights;
	CCar*            m_pcar;

private:
};






