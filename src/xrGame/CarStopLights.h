
#pragma once

#include "../xrEngine/Render.h"

class CCarStopLights;
class CCar;
class CLAItem;

struct SCarStopLight
{
	ref_light				light_render;
	ref_light		light_omni;
	CLAItem*						m_lanim;
	ref_glow				glow_render;
	u16						bone_id;
	CCarStopLights*				m_holder;				
			SCarStopLight		();
			~SCarStopLight		();
	void	Switch			();
	void	TurnOn			();
	void	TurnOff			();
	bool	isOn			();
	void	Init			(CCarStopLights* holder);
	void	Update			();
	void	ParseDefinitions(LPCSTR section);
	

};

DEFINE_VECTOR(SCarStopLight*,LIGHTS_STORAGE_3,LIGHTS_I_3)
class CCarStopLights
{
public:
	void	ParseDefinitions	()								;
	void	Init				(CCar* pcar)					;
	void	Update				()								;
	CCar*	PCar				()								{return m_pcar;}
	void	SwitchStopLights	()								;
	void	TurnOnStopLights	()								;
	void	TurnOffStopLights	()								;
	bool	IsLight				(u16 bone_id)					;
	bool	findLight			(u16 bone_id,SCarStopLight* &light);
			CCarStopLights			()								;
			~CCarStopLights			()								;
protected:
	struct SFindLightPredicate {
		const SCarStopLight *m_light;

		SFindLightPredicate	(const SCarStopLight *light) :
		m_light(light)
		{
		}

		bool		operator()					(const SCarStopLight *light) const
		{
			return light->bone_id==m_light->bone_id;
		}
	};
	LIGHTS_STORAGE_3	m_stop_lights								;
	CCar*			m_pcar									;

private:
};






