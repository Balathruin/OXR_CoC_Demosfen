#include "stdafx.h"
#include "CarStopLights.h"
#ifdef DEBUG

#include "PHDebug.h"
#endif
#include "alife_space.h"
#include "hit.h"
#include "PHDestroyable.h"
#include "Car.h"
#include "../Include/xrRender/Kinematics.h"
#include "../xrEngine/LightAnimLibrary.h"
//#include "PHWorld.h"
//extern CPHWorld*	ph_world;
#include "../xrphysics/IPHWorld.h"

SCarStopLight::SCarStopLight()
{
    m_lanim = nullptr;
    light_omni = nullptr;
    light_render = nullptr;
    glow_render = nullptr;
	bone_id			=BI_NONE;
    m_holder = nullptr;
}

SCarStopLight::~SCarStopLight()
{
	light_omni.destroy		();
	light_render.destroy	()	;
	glow_render.destroy		()	;
	bone_id			=	BI_NONE	;
}

void SCarStopLight::Init(CCarStopLights* holder)
{
	m_holder=holder;
}

void SCarStopLight::ParseDefinitions(LPCSTR section)
{
    light_omni = ::GEnv.Render->light_create();
	light_omni->set_type		(IRender_Light::POINT);
	light_omni->set_shadow		(false);
    light_render = ::GEnv.Render->light_create();
	light_render->set_type	(IRender_Light::SPOT);
	light_render->set_shadow(true);
    glow_render = ::GEnv.Render->glow_create();
	m_lanim					= 0;
	//	time2hide				= 0;

	// set bone id
	IKinematics*			pKinematics=smart_cast<IKinematics*>(m_holder->PCar()->Visual());
	CInifile* ini		=	pKinematics->LL_UserData();
	
	Fcolor					clr;
	clr.set					(ini->r_fcolor(section,"color"));
	//clr.mul_rgb				(torch->spot_brightness);
	//fBrightness				= torch->spot_brightness;
	light_render->set_range	(ini->r_float(section,"range"));
	light_render->set_color	(clr);
	light_render->set_cone	(deg2rad(ini->r_float(section,"cone_angle")));
	light_render->set_texture(ini->r_string(section,"spot_texture"));

	glow_render->set_texture(ini->r_string(section,"glow_texture"));
	glow_render->set_color	(clr);
	glow_render->set_radius	(ini->r_float(section,"glow_radius"));

	LPCSTR lanim						= ini->r_string	(section,"light_color_animmator");
	m_lanim								= LALib.FindItem(lanim);
	
	light_omni->set_range	(ini->r_float(section,"range_omni"));
	light_omni->set_color	(clr);
	
	bone_id	= pKinematics->LL_BoneID(ini->r_string(section,"bone"));
	glow_render ->set_active(false);
	light_render->set_active(false);
	light_omni->set_active(false);
	pKinematics->LL_SetBoneVisible(bone_id,FALSE,TRUE);

	m_lanim					= LALib.FindItem(ini->r_string(section,"light_color_animmator"));
	
}

void SCarStopLight::Switch()
{
	VERIFY(!physics_world()->Processing());
	if(isOn())TurnOff();
	else	  TurnOn();
}
void SCarStopLight::TurnOn()
{
	VERIFY(!physics_world()->Processing());
	if(isOn()) return;
	IKinematics* K=smart_cast<IKinematics*>(m_holder->PCar()->Visual());
	K->LL_SetBoneVisible(bone_id,TRUE,TRUE);
	K->CalculateBones_Invalidate	();
	K->CalculateBones(TRUE);	
	glow_render ->set_active(true);
	light_render->set_active(true);
	light_omni->set_active(true);
	Update();

}
void SCarStopLight::TurnOff()
{
	VERIFY(!physics_world()->Processing());
	if(!isOn()) return;
 	glow_render ->set_active(false);
	light_render->set_active(false);
	light_omni->set_active(false);
	smart_cast<IKinematics*>(m_holder->PCar()->Visual())->LL_SetBoneVisible(bone_id,FALSE,TRUE);
}

bool SCarStopLight::isOn()
{
	VERIFY(!physics_world()->Processing());
	VERIFY(light_render->get_active()==glow_render->get_active());
	VERIFY(light_render->get_active()==light_omni->get_active());
	return light_render->get_active();
}

void SCarStopLight::Update()
{
	VERIFY(!physics_world()->Processing());
	if(!isOn()) return;
	CCar* pcar=m_holder->PCar();
	CBoneInstance& BI = smart_cast<IKinematics*>(pcar->Visual())->LL_GetBoneInstance(bone_id);
	Fmatrix M;
	M.mul(pcar->XFORM(),BI.mTransform);
	light_render->set_rotation	(M.k,M.i);
	glow_render->set_direction(M.k);
	glow_render->set_position	(M.c);
	light_render->set_position	(M.c);
	light_omni->set_position	(M.c);

}


CCarStopLights::CCarStopLights()
{
	m_pcar=NULL;
}

void CCarStopLights::Init(CCar* pcar)
{
	m_pcar=pcar;
	m_stop_lights.clear();
}

void CCarStopLights::ParseDefinitions()
{
	CInifile* ini= smart_cast<IKinematics*>(m_pcar->Visual())->LL_UserData();
	if(!ini->section_exist("lights")) return;
	LPCSTR S=  (READ_IF_EXISTS(ini,r_string,"lights","stoplights",NULL));
	string64					S1;
	int count =					_GetItemCount(S);
	for (int i=0 ;i<count; ++i) 
	{
		_GetItem					(S,i,S1);
		m_stop_lights.push_back(new SCarStopLight());
		m_stop_lights.back()->Init(this);
		m_stop_lights.back()->ParseDefinitions(S1);
	}
	
}

void CCarStopLights::Update()
{
	VERIFY(!physics_world()->Processing());
	LIGHTS_I_3 i =m_stop_lights.begin(),e=m_stop_lights.end();
	for(;i!=e;++i) (*i)->Update();
}

void CCarStopLights::SwitchStopLights()
{
	
	VERIFY(!physics_world()->Processing());
	LIGHTS_I_3 i =m_stop_lights.begin(),e=m_stop_lights.end();
	for(;i!=e;++i) (*i)->Switch();
}

void CCarStopLights::TurnOnStopLights()
{

	VERIFY(!physics_world()->Processing());
	LIGHTS_I_3 i =m_stop_lights.begin(),e=m_stop_lights.end();
	for(;i!=e;++i) (*i)->TurnOn();
}
void CCarStopLights::TurnOffStopLights()
{
	VERIFY(!physics_world()->Processing());
	LIGHTS_I_3 i =m_stop_lights.begin(),e=m_stop_lights.end();
	for(;i!=e;++i) (*i)->TurnOff();
}

bool CCarStopLights::IsLight(u16 bone_id)
{
	SCarStopLight* light=NULL;
	return findLight(bone_id,light);
}
bool CCarStopLights::findLight(u16 bone_id,SCarStopLight* &light)
{
	LIGHTS_I_3 i,e=m_stop_lights.end();
	SCarStopLight find_light;
	find_light.bone_id=bone_id;
	i=std::find_if(m_stop_lights.begin(),e,SFindLightPredicate(&find_light));
	light=*i;
	return i!=e;
}
CCarStopLights::~CCarStopLights()
{
	LIGHTS_I_3 i =m_stop_lights.begin(),e=m_stop_lights.end();
	for(;i!=e;++i) xr_delete(*i);
	m_stop_lights.clear();
}
