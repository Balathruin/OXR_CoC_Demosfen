#include "stdafx.h"
#include "CarWeapon4.h"
#include "xrPhysics/PhysicsShell.h"
#include "PhysicsShellHolder.h"

#include "Include/xrRender/Kinematics.h"
#include "Common/object_broker.h"
#include "ai_sounds.h"
#include "weaponAmmo.h"
#include "xr_level_controller.h"
#include "game_object_space.h"
#include "holder_custom.h"

void CCarWeapon4::BoneCallbackX		(CBoneInstance *B)
{
	CCarWeapon4	*P = static_cast<CCarWeapon4*>(B->callback_param());
	Fmatrix rX;		rX.rotateX		(P->m_cur_x_rot);
	B->mTransform.mulB_43			(rX);
}

void CCarWeapon4::BoneCallbackY		(CBoneInstance *B)
{
	CCarWeapon4	*P = static_cast<CCarWeapon4*>(B->callback_param());
	Fmatrix rY;		rY.rotateY		(P->m_cur_y_rot);
	B->mTransform.mulB_43			(rY);
}

CCarWeapon4::CCarWeapon4(CPhysicsShellHolder* obj)
{
	m_bActive	= false;
	m_bAutoFire	= false;
	m_object	= obj;
	m_Ammo		= new CCartridge();

	IKinematics* K			= smart_cast<IKinematics*>(m_object->Visual());
	CInifile* pUserData		= K->LL_UserData(); 

	m_rotate_x_bone			= K->LL_BoneID	(pUserData->r_string("mounted_weapon_definition4","rotate_x_bone4"));
	m_rotate_y_bone			= K->LL_BoneID	(pUserData->r_string("mounted_weapon_definition4","rotate_y_bone4"));
	m_fire_bone				= K->LL_BoneID	(pUserData->r_string("mounted_weapon_definition4","fire_bone4"));
	m_min_gun_speed			= pUserData->r_float("mounted_weapon_definition4","min_gun_speed4");
	m_max_gun_speed			= pUserData->r_float("mounted_weapon_definition4","max_gun_speed4");
	CBoneData& bdX			= K->LL_GetData(m_rotate_x_bone); //VERIFY(bdX.IK_data.type==jtJoint);
	m_lim_x_rot.set			(bdX.IK_data.limits[0].limit.x,bdX.IK_data.limits[0].limit.y);
	CBoneData& bdY			= K->LL_GetData(m_rotate_y_bone); //VERIFY(bdY.IK_data.type==jtJoint);
	m_lim_y_rot.set			(bdY.IK_data.limits[1].limit.x,bdY.IK_data.limits[1].limit.y);

	xr_vector<Fmatrix>					matrices;
	K->LL_GetBindTransform				(matrices);
	m_i_bind_x_xform.invert				(matrices[m_rotate_x_bone]);
	m_i_bind_y_xform.invert				(matrices[m_rotate_y_bone]);
	m_bind_x_rot						= matrices[m_rotate_x_bone].k.getP();
	m_bind_y_rot						= matrices[m_rotate_y_bone].k.getH();
	m_bind_x.set						(matrices[m_rotate_x_bone].c);
	m_bind_y.set						(matrices[m_rotate_y_bone].c);

	m_cur_x_rot							= m_bind_x_rot;
	m_cur_y_rot							= m_bind_y_rot;
	m_destEnemyDir.setHP				(m_bind_y_rot,m_bind_x_rot);
	m_object->XFORM().transform_dir		(m_destEnemyDir);

	inheritedShooting::Light_Create		();
	Load								(pUserData->r_string("mounted_weapon_definition4","wpn_section4"));
	SetBoneCallbacks					();
	m_object->processing_activate		();

	m_weapon_h							= matrices[m_rotate_y_bone].c.y;
	m_fire_norm.set						(0,1,0);
	m_fire_dir.set						(0,0,1);
	m_fire_pos.set						(0,0,0);
}

CCarWeapon4::~CCarWeapon4()
{
	delete_data(m_Ammo);
//.	m_object->processing_deactivate		();
}

void CCarWeapon4::Load(LPCSTR section)
{
	inheritedShooting::Load(section);
	HUD_SOUND_ITEM::LoadSound(section,"snd_shoot4", m_sndShot, SOUND_TYPE_WEAPON_SHOOTING);
	HUD_SOUND_ITEM::LoadSound(section,"snd_shoot_high_4", m_sndShot_4, SOUND_TYPE_WEAPON_SHOOTING);
	m_Ammo->Load(pSettings->r_string(section, "ammo_class4"), 0);
}

void CCarWeapon4::UpdateCL()
{
	if(!m_bActive) return;
	UpdateBarrelDir				();
	IKinematics* K				= smart_cast<IKinematics*>(m_object->Visual());
	K->CalculateBones_Invalidate();
	K->CalculateBones			(TRUE);
	UpdateFire					();
}

void CCarWeapon4::UpdateFire()
{
	fShotTimeCounter -= Device.fTimeDelta;

	inheritedShooting::UpdateFlameParticles();
	inheritedShooting::UpdateLight();

    if (m_bAutoFire)
    {
		if (m_allow_fire)
        {
		   FireStart();
		}
        else
			FireEnd();
	};

	if(!IsWorking())
	{
		clamp(fShotTimeCounter, 0.0f, flt_max);
		return;
	}

	if(fShotTimeCounter<=0)
	{
		OnShot();
		fShotTimeCounter += fOneShotTime;
	}
}

void CCarWeapon4::Render_internal() { RenderLight(); }

void CCarWeapon4::SetBoneCallbacks()
{
//	m_object->PPhysicsShell()->EnabledCallbacks(FALSE);
	
	CBoneInstance& biX		= smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_x_bone);	
	biX.set_callback		(bctCustom,BoneCallbackX,this);
	CBoneInstance& biY		= smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_y_bone);	
	biY.set_callback		(bctCustom,BoneCallbackY,this);
}

void CCarWeapon4::ResetBoneCallbacks()
{
	CBoneInstance& biX		= smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_x_bone);	
	biX.reset_callback		();
	CBoneInstance& biY		= smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_y_bone);	
	biY.reset_callback		();

//	m_object->PPhysicsShell()->EnabledCallbacks(TRUE);
}

void CCarWeapon4::UpdateBarrelDir()
{
	IKinematics* K		= smart_cast<IKinematics*>(m_object->Visual());
	m_fire_bone_xform	= K->LL_GetTransform(m_fire_bone);

	m_fire_bone_xform.mulA_43(m_object->XFORM());
	m_fire_pos.set(0,0,0); 
	m_fire_bone_xform.transform_tiny(m_fire_pos);
	m_fire_dir.set(0,0,1);
	m_fire_bone_xform.transform_dir(m_fire_dir);
	m_fire_norm.set(0,1,0);
	m_fire_bone_xform.transform_dir(m_fire_norm);

	m_allow_fire		= true;
	Fmatrix XFi;
	XFi.invert			(m_object->XFORM());
	Fvector dep;
	XFi.transform_dir	(dep,m_destEnemyDir);
	{// x angle
		m_i_bind_x_xform.transform_dir(dep); dep.normalize();
		m_tgt_x_rot		= angle_normalize_signed(m_bind_x_rot-dep.getP());
		clamp			(m_tgt_x_rot,-m_lim_x_rot.y,-m_lim_x_rot.x);
	}
	{// y angle
		m_i_bind_y_xform.transform_dir(dep); dep.normalize();
		m_tgt_y_rot		= angle_normalize_signed(m_bind_y_rot-dep.getH());
		clamp			(m_tgt_y_rot,-m_lim_y_rot.y,-m_lim_y_rot.x);
	}

	m_cur_x_rot		= angle_inertion_var(m_cur_x_rot,m_tgt_x_rot,m_min_gun_speed,m_max_gun_speed,PI,Device.fTimeDelta);
	m_cur_y_rot		= angle_inertion_var(m_cur_y_rot,m_tgt_y_rot,m_min_gun_speed,m_max_gun_speed,PI,Device.fTimeDelta);
	static float dir_eps = deg2rad(5.0f);
	if( !fsimilar(m_cur_x_rot,m_tgt_x_rot,dir_eps)|| !fsimilar(m_cur_y_rot,m_tgt_y_rot,dir_eps))
		m_allow_fire=FALSE;

#if (0)
	if(Device.dwFrame%200==0)
    {
		Msg("m_cur_x_rot=[%f]",m_cur_x_rot);
		Msg("m_cur_y_rot=[%f]",m_cur_y_rot);
	}
#endif
}
bool CCarWeapon4::AllowFire() { return m_allow_fire; }

float CCarWeapon4::FireDirDiff()
{
	Fvector d1,d2;
	d1.set(m_cur_x_rot, m_cur_y_rot, 0).normalize_safe();
	d2.set(m_tgt_x_rot, m_tgt_y_rot, 0).normalize_safe();
	return rad2deg(acos(d1.dotproduct(d2)));
}

const Fvector&	CCarWeapon4::get_CurrentFirePoint() { return m_fire_pos; }
const Fmatrix&	CCarWeapon4::get_ParticlesXFORM	() { return m_fire_bone_xform; }
void CCarWeapon4::FireStart() { inheritedShooting::FireStart(); }

void CCarWeapon4::FireEnd()	
{
	inheritedShooting::FireEnd();
	StopFlameParticles	();
}

void CCarWeapon4::OnShot()
{
    CHolderCustom* holder = smart_cast<CHolderCustom*>(m_object);
    FireBullet(m_fire_pos, m_fire_dir, fireDispersionBase, *m_Ammo, holder->Engaged() ? 0 : m_object->ID(), m_object->ID(),
        SendHitAllowed(m_object), ::Random.randI(0, 30));

	StartShotParticles		();

	if(m_bLightShotEnabled) 
		Light_Start			();

	StartFlameParticles		();
	StartSmokeParticles		(m_fire_pos, zero_vel);
	//OnShellDrop				(m_fire_pos, zero_vel);

	HUD_SOUND_ITEM::PlaySound	(m_sndShot, m_fire_pos, m_object, false);
	HUD_SOUND_ITEM::PlaySound	(m_sndShot_4, m_fire_pos, m_object, true);
}

void CCarWeapon4::Action				(u16 id, u32 flags)
{
	switch (id)
    {
        case eWpnFire:
        {
            if (flags == 1)
                FireStart();
            else
                FireEnd();
        }
            break;
            case eWpnActivate:
        {
            if (flags == 1)
                m_bActive = true;
            else
            {
                m_bActive = false;
                FireEnd();
            }
        }
        break;

        case eWpnAutoFire:
        {
            if (flags == 1)
                m_bAutoFire = true;
            else
                m_bAutoFire = false;
        }
        break;
        case eWpnToDefaultDir:
        {
            SetParam(eWpnDesiredDir, Fvector2().set(m_bind_y_rot, m_bind_x_rot));
        }
        break;
    }
}

void CCarWeapon4::SetParam			(int id, Fvector2 val)
{
    switch (id)
    {
        case eWpnDesiredDir: m_destEnemyDir.setHP(val.x,val.y); break;
    }
}

void CCarWeapon4::SetParam			(int id, Fvector val)
{
    switch (id)
    {
        case eWpnDesiredPos: m_destEnemyDir.sub(val,m_fire_pos).normalize_safe(); break;
    }
}
const Fvector&	CCarWeapon4::ViewCameraPos() { return m_fire_pos; }
const Fvector&	CCarWeapon4::ViewCameraDir() { return m_fire_dir; }
const Fvector&	CCarWeapon4::ViewCameraNorm() { return m_fire_norm; }
