#include "stdafx.h"
#include "player_hud.h"
#include "HudItem.h"
#include "ui_base.h"
#include "actor.h"
#include "physic_item.h"
#include "static_cast_checked.hpp"
#include "actoreffector.h"
#include "xrEngine/IGame_Persistent.h"

//Alun: defined in HudItem.cpp now
extern const float PITCH_OFFSET_R;		// barrel movement sideways (to the left) with vertical camera turns
extern const float PITCH_OFFSET_N;		// barrel rise / fall with vertical camera turns
extern const float PITCH_OFFSET_D;		// barrel toward / away when the camera rotates vertically
extern const float PITCH_LOW_LIMIT;		// minimum pitch value when used in conjunction with PITCH_OFFSET_N
extern const float ORIGIN_OFFSET;		// inertia factor influence on position of torso (the smaller, the larger the inertia)
extern const float ORIGIN_OFFSET_AIM;	// (zoomed inertia factor)
extern const float TENDTO_SPEED;		// barrel return speed
extern const float TENDTO_SPEED_AIM;	// (zoomed return speed)

player_hud* g_player_hud = NULL;
Fvector _ancor_pos;
Fvector _wpn_root_pos;
Fvector m_hud_offset_pos = { 0.f, 0.f, 0.f }; //only in hud adj mode
Fvector m_hand_offset_pos = { 0.f, 0.f, 0.f };

float CalcMotionSpeed(const shared_str& anim_name)
{
    if (!IsGameTypeSingle() && (anim_name == "anm_show" || anim_name == "anm_hide"))
        return 2.0f;
    else
        return 1.0f;
}

player_hud_motion* player_hud_motion_container::find_motion(const shared_str& name)
{
    xr_vector<player_hud_motion>::iterator it = m_anims.begin();
    xr_vector<player_hud_motion>::iterator it_e = m_anims.end();
    for (; it != it_e; ++it)
    {
        const shared_str& s = (true) ? (*it).m_alias_name : (*it).m_base_name;
        if (s == name)
            return &*it;
    }
    return nullptr;
}

void player_hud_motion_container::load(IKinematicsAnimated* model, const shared_str& sect)
{
    CInifile::Sect& _sect = pSettings->r_section(sect);
    auto _b = _sect.Data.cbegin();
    auto _e = _sect.Data.cend();
    player_hud_motion* pm = nullptr;

    string512 buff;
    MotionID motion_ID;

    for (; _b != _e; ++_b)
    {
        if (strstr(_b->first.c_str(), "anm_") == _b->first.c_str())
        {
            const shared_str& anm = _b->second;
            m_anims.resize(m_anims.size() + 1);
            pm = &m_anims.back();
            // base and alias name
            pm->m_alias_name = _b->first;

            if (_GetItemCount(anm.c_str()) == 1)
            {
                pm->m_base_name = anm;
                pm->m_additional_name = anm;
                pm->m_anim_speed = 1.f;
            }
            else
            {
                R_ASSERT2(_GetItemCount(anm.c_str()) <= 3, anm.c_str());
                string512 str_item;
                _GetItem(anm.c_str(), 0, str_item);
                pm->m_base_name = str_item;

                _GetItem(anm.c_str(), 1, str_item);
                pm->m_additional_name = (strlen(str_item) > 0) ? pm->m_additional_name = str_item : pm->m_base_name;

                _GetItem(anm.c_str(), 2, str_item);
                pm->m_anim_speed = strlen(str_item) > 0 ? atof(str_item) : 1.f;
            }

            // and load all motions for it

            for (u32 i = 0; i <= 8; ++i)
            {
                if (i == 0)
                    xr_strcpy(buff, pm->m_base_name.c_str());
                else
                    xr_sprintf(buff, "%s%d", pm->m_base_name.c_str(), i);

                motion_ID = model->ID_Cycle_Safe(buff);
                if (motion_ID.valid())
                {
                    pm->m_animations.resize(pm->m_animations.size() + 1);
                    pm->m_animations.back().mid = motion_ID;
                    pm->m_animations.back().name = buff;
#ifdef DEBUG
					//					Msg(" alias=[%s] base=[%s] name=[%s]",pm->m_alias_name.c_str(), pm->m_base_name.c_str(), buff);
#endif // #ifdef DEBUG
                }
            }
            R_ASSERT2(pm->m_animations.size(), make_string("motion not found [%s]", pm->m_base_name.c_str()).c_str());
        }
    }
}

Fvector& attachable_hud_item::hands_attach_pos()
{
    Fvector v;
    v.set(m_measures.m_hands_attach[0]).add(m_hand_offset_pos);
    return v;
}
Fvector& attachable_hud_item::hands_attach_rot() { return m_measures.m_hands_attach[1]; }
Fvector& attachable_hud_item::hands_offset_pos()
{
    u8 idx = m_parent_hud_item->GetCurrentHudOffsetIdx();
    Fvector v;
    v.set(m_measures.m_hands_offset[0][idx]).add(m_hud_offset_pos);
    return v;
}

Fvector& attachable_hud_item::hands_offset_rot()
{
    u8 idx = m_parent_hud_item->GetCurrentHudOffsetIdx();
    return m_measures.m_hands_offset[1][idx];
}

bool attachable_hud_item::set_bone_visible(const shared_str& bone_name, BOOL bVisibility, BOOL bSilent)
{
    u16 bone_id;
    BOOL bVisibleNow;
    bone_id = m_model->LL_BoneID(bone_name);
    if (bone_id == BI_NONE)
    {
        if (bSilent)
            return false;
        R_ASSERT2(0, make_string("model [%s] has no bone [%s]", pSettings->r_string(m_sect_name, "item_visual"),
                         bone_name.c_str())
                         .c_str());
    }
    bVisibleNow = m_model->LL_GetBoneVisible(bone_id);
    if (bVisibleNow != bVisibility)
        m_model->LL_SetBoneVisible(bone_id, bVisibility, TRUE);

    return true;
}

void attachable_hud_item::update(bool bForce)
{
    if (!bForce && m_upd_firedeps_frame == Device.dwFrame)
        return;
    bool is_16x9 = UI().is_widescreen();

    if (!!m_measures.m_prop_flags.test(hud_item_measures::e_16x9_mode_now) != is_16x9)
        m_measures.load(m_sect_name, m_model);

    Fvector ypr = m_measures.m_item_attach[1];
    ypr.mul(PI / 180.f);
    m_attach_offset.setHPB(ypr.x, ypr.y, ypr.z);
    m_attach_offset.translate_over(m_measures.m_item_attach[0]);

    m_parent->calc_transform(m_attach_place_idx, m_attach_offset, m_item_transform);
    m_upd_firedeps_frame = Device.dwFrame;

    IKinematicsAnimated* ka = m_model->dcast_PKinematicsAnimated();
    if (ka)
    {
        ka->UpdateTracks();
        ka->dcast_PKinematics()->CalculateBones_Invalidate();
        ka->dcast_PKinematics()->CalculateBones(TRUE);
    }
}

void attachable_hud_item::update_hud_additional(Fmatrix& trans)
{
    if (m_parent_hud_item)
    {
        m_parent_hud_item->UpdateHudAdditonal(trans);
    }
}

void attachable_hud_item::setup_firedeps(firedeps& fd)
{
    update(false);
    // fire point&direction
    if (m_measures.m_prop_flags.test(hud_item_measures::e_fire_point))
    {
        Fmatrix& fire_mat = m_model->LL_GetTransform(m_measures.m_fire_bone);
        fire_mat.transform_tiny(fd.vLastFP, m_measures.m_fire_point_offset);
        m_item_transform.transform_tiny(fd.vLastFP);

		fd.vLastFD.set(m_measures.m_fire_direction);
        m_item_transform.transform_dir(fd.vLastFD);
        VERIFY(_valid(fd.vLastFD));
        VERIFY(_valid(fd.vLastFD));

        fd.m_FireParticlesXForm.identity();
        fd.m_FireParticlesXForm.k.set(fd.vLastFD);
        Fvector::generate_orthonormal_basis_normalized(
            fd.m_FireParticlesXForm.k, fd.m_FireParticlesXForm.j, fd.m_FireParticlesXForm.i);
        VERIFY(_valid(fd.m_FireParticlesXForm));
    }

    if (m_measures.m_prop_flags.test(hud_item_measures::e_fire_point2))
    {
        Fmatrix& fire_mat = m_model->LL_GetTransform(m_measures.m_fire_bone2);
        fire_mat.transform_tiny(fd.vLastFP2, m_measures.m_fire_point2_offset);
        m_item_transform.transform_tiny(fd.vLastFP2);
        VERIFY(_valid(fd.vLastFP2));
        VERIFY(_valid(fd.vLastFP2));
    }

    if (m_measures.m_prop_flags.test(hud_item_measures::e_shell_point))
    {
        Fmatrix& fire_mat = m_model->LL_GetTransform(m_measures.m_shell_bone);
        fire_mat.transform_tiny(fd.vLastSP, m_measures.m_shell_point_offset);
        m_item_transform.transform_tiny(fd.vLastSP);
        VERIFY(_valid(fd.vLastSP));
        VERIFY(_valid(fd.vLastSP));
    }
}

bool attachable_hud_item::need_renderable() { return m_parent_hud_item->need_renderable(); }
void attachable_hud_item::render()
{
    GEnv.Render->set_Transform(&m_item_transform);
    GEnv.Render->add_Visual(m_model->dcast_RenderVisual());
    debug_draw_firedeps();
    m_parent_hud_item->render_hud_mode();
}

bool attachable_hud_item::render_item_ui_query() { return m_parent_hud_item->render_item_3d_ui_query(); }
void attachable_hud_item::render_item_ui() { m_parent_hud_item->render_item_3d_ui(); }
void hud_item_measures::load(const shared_str& sect_name, IKinematics* K)
{
    bool is_16x9 = UI().is_widescreen();
    string64 _prefix;
    xr_sprintf(_prefix, "%s", is_16x9 ? "_16x9" : "");
    string128 val_name;

    strconcat(sizeof(val_name), val_name, "hands_position", _prefix);
    m_hands_attach[0] = pSettings->r_fvector3(sect_name, val_name);
    strconcat(sizeof(val_name), val_name, "hands_orientation", _prefix);
    m_hands_attach[1] = pSettings->r_fvector3(sect_name, val_name);

    m_item_attach[0] = pSettings->r_fvector3(sect_name, "item_position");
    m_item_attach[1] = pSettings->r_fvector3(sect_name, "item_orientation");

    shared_str bone_name;
    m_prop_flags.set(e_fire_point, pSettings->line_exist(sect_name, "fire_bone"));
    if (m_prop_flags.test(e_fire_point))
    {
        bone_name = pSettings->r_string(sect_name, "fire_bone");
        m_fire_bone = K->LL_BoneID(bone_name);
        m_fire_point_offset = pSettings->r_fvector3(sect_name, "fire_point");
		m_fire_direction = READ_IF_EXISTS(pSettings, r_fvector3, sect_name, "fire_direction", Fvector().set(0.f, 0.f, 1.0f));

    }
    else
        m_fire_point_offset.set(0, 0, 0);

    m_prop_flags.set(e_fire_point2, pSettings->line_exist(sect_name, "fire_bone2"));
    if (m_prop_flags.test(e_fire_point2))
    {
        bone_name = pSettings->r_string(sect_name, "fire_bone2");
        m_fire_bone2 = K->LL_BoneID(bone_name);
        m_fire_point2_offset = pSettings->r_fvector3(sect_name, "fire_point2");
    }
    else
        m_fire_point2_offset.set(0, 0, 0);

    m_prop_flags.set(e_shell_point, pSettings->line_exist(sect_name, "shell_bone"));
    if (m_prop_flags.test(e_shell_point))
    {
        bone_name = pSettings->r_string(sect_name, "shell_bone");
        m_shell_bone = K->LL_BoneID(bone_name);
        m_shell_point_offset = pSettings->r_fvector3(sect_name, "shell_point");
    }
    else
        m_shell_point_offset.set(0, 0, 0);

    m_hands_offset[0][0].set(0, 0, 0);
    m_hands_offset[1][0].set(0, 0, 0);

    strconcat(sizeof(val_name), val_name, "aim_hud_offset_pos", _prefix);
    m_hands_offset[0][1] = pSettings->r_fvector3(sect_name, val_name);
    strconcat(sizeof(val_name), val_name, "aim_hud_offset_rot", _prefix);
    m_hands_offset[1][1] = pSettings->r_fvector3(sect_name, val_name);

    strconcat(sizeof(val_name), val_name, "gl_hud_offset_pos", _prefix);
    m_hands_offset[0][2] = pSettings->r_fvector3(sect_name, val_name);
    strconcat(sizeof(val_name), val_name, "gl_hud_offset_rot", _prefix);
    m_hands_offset[1][2] = pSettings->r_fvector3(sect_name, val_name);

    R_ASSERT2(pSettings->line_exist(sect_name, "fire_point") == pSettings->line_exist(sect_name, "fire_bone"),
        sect_name.c_str());
    R_ASSERT2(pSettings->line_exist(sect_name, "fire_point2") == pSettings->line_exist(sect_name, "fire_bone2"),
        sect_name.c_str());
    R_ASSERT2(pSettings->line_exist(sect_name, "shell_point") == pSettings->line_exist(sect_name, "shell_bone"),
        sect_name.c_str());

    m_prop_flags.set(e_16x9_mode_now, is_16x9);

    //Загрузка параметров инерции --#SM+# Begin--
    m_inertion_params.m_pitch_offset_r = READ_IF_EXISTS(pSettings, r_float, sect_name, "pitch_offset_right", PITCH_OFFSET_R);
    m_inertion_params.m_pitch_offset_n = READ_IF_EXISTS(pSettings, r_float, sect_name, "pitch_offset_up", PITCH_OFFSET_N);
    m_inertion_params.m_pitch_offset_d = READ_IF_EXISTS(pSettings, r_float, sect_name, "pitch_offset_forward", PITCH_OFFSET_D);
    m_inertion_params.m_pitch_low_limit = READ_IF_EXISTS(pSettings, r_float, sect_name, "pitch_offset_up_low_limit", PITCH_LOW_LIMIT);

    m_inertion_params.m_origin_offset = READ_IF_EXISTS(pSettings, r_float, sect_name, "inertion_origin_offset", ORIGIN_OFFSET);
    m_inertion_params.m_origin_offset_aim = READ_IF_EXISTS(pSettings, r_float, sect_name, "inertion_origin_aim_offset", ORIGIN_OFFSET_AIM);
    m_inertion_params.m_tendto_speed = READ_IF_EXISTS(pSettings, r_float, sect_name, "inertion_tendto_speed", TENDTO_SPEED);
    m_inertion_params.m_tendto_speed_aim = READ_IF_EXISTS(pSettings, r_float, sect_name, "inertion_tendto_aim_speed", TENDTO_SPEED_AIM);
    //--#SM+# End--
}

attachable_hud_item::~attachable_hud_item()
{
    IRenderVisual* v = m_model->dcast_RenderVisual();
    GEnv.Render->model_Delete(v);
    m_model = nullptr;
}

void attachable_hud_item::load(const shared_str& sect_name)
{
    m_sect_name = sect_name;

    // Visual
    const shared_str& visual_name = pSettings->r_string(sect_name, "item_visual");
    m_model = smart_cast<IKinematics*>(GEnv.Render->model_Create(visual_name.c_str()));

    m_attach_place_idx = pSettings->r_u16(sect_name, "attach_place_idx");
    m_measures.load(sect_name, m_model);
}

u32 attachable_hud_item::anim_play(const shared_str& anm_name_b, BOOL bMixIn, const CMotionDef*& md, u8& rnd_idx)
{
    R_ASSERT(strstr(anm_name_b.c_str(), "anm_") == anm_name_b.c_str());
    string256 anim_name_r;
    bool is_16x9 = UI().is_widescreen();
    xr_sprintf(anim_name_r, "%s%s", anm_name_b.c_str(), ((m_attach_place_idx == 1) && is_16x9) ? "_16x9" : "");

    player_hud_motion* anm = m_hand_motions.find_motion(anim_name_r);
    R_ASSERT2(
        anm, make_string("model [%s] has no motion alias defined [%s]", m_sect_name.c_str(), anim_name_r).c_str());
    R_ASSERT2(anm->m_animations.size(), make_string("model [%s] has no motion defined in motion_alias [%s]",
                                            pSettings->r_string(m_sect_name, "item_visual"), anim_name_r)
                                            .c_str());

    float speed = anm->m_anim_speed;

    rnd_idx = (u8)Random.randI(anm->m_animations.size());
    const motion_descr& M = anm->m_animations[rnd_idx];

    u32 ret = g_player_hud->anim_play(m_attach_place_idx, M.mid, bMixIn, md, speed);

    if (m_model->dcast_PKinematicsAnimated())
    {
        IKinematicsAnimated* ka = m_model->dcast_PKinematicsAnimated();

        shared_str item_anm_name;
        if (anm->m_base_name != anm->m_additional_name)
            item_anm_name = anm->m_additional_name;
        else
            item_anm_name = M.name;

        MotionID M2 = ka->ID_Cycle_Safe(item_anm_name);
        if (!M2.valid())
            M2 = ka->ID_Cycle_Safe("idle");
        else if (bDebug)
            Msg("playing item animation [%s]", item_anm_name.c_str());

        R_ASSERT3(M2.valid(), "model has no motion [idle] ", pSettings->r_string(m_sect_name, "item_visual"));

        u16 root_id = m_model->LL_GetBoneRoot();
        CBoneInstance& root_binst = m_model->LL_GetBoneInstance(root_id);
        root_binst.set_callback_overwrite(TRUE);
        root_binst.mTransform.identity();

        u16 pc = ka->partitions().count();
        for (u16 pid = 0; pid < pc; ++pid)
        {
            CBlend* B = ka->PlayCycle(pid, M2, bMixIn);
            R_ASSERT(B);
            B->speed *= speed;
        }

        m_model->CalculateBones_Invalidate();
    }

    R_ASSERT2(m_parent_hud_item, "parent hud item is NULL");
    CPhysicItem& parent_object = m_parent_hud_item->object();
    // R_ASSERT2		(parent_object, "object has no parent actor");
    // IGameObject*		parent_object = static_cast_checked<IGameObject*>(&m_parent_hud_item->object());

    if (IsGameTypeSingle() && parent_object.H_Parent() == Level().CurrentControlEntity())
    {
        CActor* current_actor = static_cast_checked<CActor*>(Level().CurrentControlEntity());
        VERIFY(current_actor);
        CEffectorCam* ec = current_actor->Cameras().GetCamEffector(eCEWeaponAction);

        if (ec)
            current_actor->Cameras().RemoveCamEffector(eCEWeaponAction);

        string_path ce_path;
        string_path anm_name;
        strconcat(sizeof(anm_name), anm_name, "camera_effects\\weapon\\", M.name.c_str(), ".anm");
        if (FS.exist(ce_path, "$game_anims$", anm_name))
        {
            CAnimatorCamEffector* e = new CAnimatorCamEffector();
            e->SetType(eCEWeaponAction);
            e->SetHudAffect(false);
            e->SetCyclic(false);
            e->Start(anm_name);
            current_actor->Cameras().AddCamEffector(e);
        }
    }
    return ret;
}

player_hud::player_hud()
{
    m_model = nullptr;
    m_attached_items[0] = nullptr;
    m_attached_items[1] = nullptr;
    m_transform.identity();
    m_attach_offset.identity();
}

player_hud::~player_hud()
{
    IRenderVisual* v = m_model->dcast_RenderVisual();
    GEnv.Render->model_Delete(v);
    m_model = nullptr;

    xr_vector<attachable_hud_item*>::iterator it = m_pool.begin();
    xr_vector<attachable_hud_item*>::iterator it_e = m_pool.end();
    for (; it != it_e; ++it)
    {
        attachable_hud_item* a = *it;
        xr_delete(a);
    }
    m_pool.clear();
}

void player_hud::load(const shared_str& player_hud_sect)
{
    if (player_hud_sect == m_sect_name)
        return;
    bool b_reload = (m_model != nullptr);
    if (m_model)
    {
        IRenderVisual* v = m_model->dcast_RenderVisual();
        GEnv.Render->model_Delete(v);
    }

    m_sect_name = player_hud_sect;
    const shared_str& model_name = pSettings->r_string(player_hud_sect, "visual");
    m_model = smart_cast<IKinematicsAnimated*>(GEnv.Render->model_Create(model_name.c_str()));

    CInifile::Sect& _sect = pSettings->r_section(player_hud_sect);
    auto _b = _sect.Data.cbegin();
    auto _e = _sect.Data.cend();
    for (; _b != _e; ++_b)
    {
        if (strstr(_b->first.c_str(), "ancor_") == _b->first.c_str())
        {
            const shared_str& _bone = _b->second;
            m_ancors.push_back(m_model->dcast_PKinematics()->LL_BoneID(_bone));
        }
    }

    //	Msg("hands visual changed to[%s] [%s] [%s]", model_name.c_str(), b_reload?"R":"", m_attached_items[0]?"Y":"");

    if (!b_reload)
    {
        m_model->PlayCycle("hand_idle_doun");
    }
    else
    {
        if (m_attached_items[1])
            m_attached_items[1]->m_parent_hud_item->on_a_hud_attach();

        if (m_attached_items[0])
            m_attached_items[0]->m_parent_hud_item->on_a_hud_attach();
    }
    m_model->dcast_PKinematics()->CalculateBones_Invalidate();
    m_model->dcast_PKinematics()->CalculateBones(TRUE);
}

bool player_hud::render_item_ui_query()
{
    bool res = false;
    if (m_attached_items[0])
        res |= m_attached_items[0]->render_item_ui_query();

    if (m_attached_items[1])
        res |= m_attached_items[1]->render_item_ui_query();

    return res;
}

void player_hud::render_item_ui()
{
    if (m_attached_items[0])
        m_attached_items[0]->render_item_ui();

    if (m_attached_items[1])
        m_attached_items[1]->render_item_ui();
}

void player_hud::render_hud()
{
    if (!m_attached_items[0] && !m_attached_items[1])
        return;

    bool b_r0 = (m_attached_items[0] && m_attached_items[0]->need_renderable());
    bool b_r1 = (m_attached_items[1] && m_attached_items[1]->need_renderable());

    if (!b_r0 && !b_r1)
        return;

    GEnv.Render->set_Transform(&m_transform);
    GEnv.Render->add_Visual(m_model->dcast_RenderVisual());

    if (m_attached_items[0])
        m_attached_items[0]->render();

    if (m_attached_items[1])
        m_attached_items[1]->render();
}

#include "xrCore/Animation/Motion.hpp"

u32 player_hud::motion_length(const shared_str& anim_name, const shared_str& hud_name, const CMotionDef*& md)
{
    float speed = CalcMotionSpeed(anim_name);
    attachable_hud_item* pi = create_hud_item(hud_name);
    player_hud_motion* pm = pi->m_hand_motions.find_motion(anim_name);
    if (!pm)
        return 100; // ms TEMPORARY
    R_ASSERT2(pm,
        make_string("hudItem model [%s] has no motion with alias [%s]", hud_name.c_str(), anim_name.c_str()).c_str());
    return motion_length(pm->m_animations[0].mid, md, speed);
}

u32 player_hud::motion_length(const MotionID& M, const CMotionDef*& md, float speed)
{
    md = m_model->LL_GetMotionDef(M);
    VERIFY(md);
    if (md->flags & esmStopAtEnd)
    {
        CMotion* motion = m_model->LL_GetRootMotion(M);
        return iFloor(0.5f + 1000.f * motion->GetLength() / (md->Dequantize(md->speed) * speed));
    }
    return 0;
}
const Fvector& player_hud::attach_rot() const
{
    if (m_attached_items[0])
        return m_attached_items[0]->hands_attach_rot();
    else if (m_attached_items[1])
        return m_attached_items[1]->hands_attach_rot();
    else
        return Fvector().set(0, 0, 0);
}

const Fvector& player_hud::attach_pos() const
{
    if (m_attached_items[0])
        return m_attached_items[0]->hands_attach_pos();
    else if (m_attached_items[1])
        return m_attached_items[1]->hands_attach_pos();
    else
        return Fvector().set(0, 0, 0);
}

void player_hud::update(const Fmatrix& cam_trans)
{
    Fmatrix trans = cam_trans;
    update_inertion(trans);
    update_additional(trans);

    Fvector ypr = attach_rot();
    ypr.mul(PI / 180.f);
    m_attach_offset.setHPB(ypr.x, ypr.y, ypr.z);
    m_attach_offset.translate_over(attach_pos());
    m_transform.mul(trans, m_attach_offset);
    // insert inertion here

    m_model->UpdateTracks();
    m_model->dcast_PKinematics()->CalculateBones_Invalidate();
    m_model->dcast_PKinematics()->CalculateBones(TRUE);

    if (m_attached_items[0])
        m_attached_items[0]->update(true);

    if (m_attached_items[1])
        m_attached_items[1]->update(true);
}

u32 player_hud::anim_play(u16 part, const MotionID& M, BOOL bMixIn, const CMotionDef*& md, float speed)
{
    u16 part_id = u16(-1);
    if (attached_item(0) && attached_item(1))
        part_id = m_model->partitions().part_id((part == 0) ? "right_hand" : "left_hand");

    u16 pc = m_model->partitions().count();
    for (u16 pid = 0; pid < pc; ++pid)
    {
        if (pid == 0 || pid == part_id || part_id == u16(-1))
        {
            CBlend* B = m_model->PlayCycle(pid, M, bMixIn);
            R_ASSERT(B);
            B->speed *= speed;
        }
    }
    m_model->dcast_PKinematics()->CalculateBones_Invalidate();

    return motion_length(M, md, speed);
}

void player_hud::update_additional(Fmatrix& trans)
{
    if (m_attached_items[0])
        m_attached_items[0]->update_hud_additional(trans);

    if (m_attached_items[1])
        m_attached_items[1]->update_hud_additional(trans);
}

void player_hud::update_inertion(Fmatrix& trans)
{
	if (inertion_allowed())
	{
		attachable_hud_item* pMainHud = m_attached_items[0];

		Fmatrix								xform;
		Fvector& origin = trans.c;
		xform = trans;


		static Fvector						st_last_dir = { 0, 0, 0 };

		// load params
		float m_pitch_offset_r = PITCH_OFFSET_R;
		float m_pitch_offset_n = PITCH_OFFSET_N;
		float m_pitch_offset_d = PITCH_OFFSET_D;
		float m_pitch_low_limit = PITCH_LOW_LIMIT;
		float m_origin_offset = ORIGIN_OFFSET;
		float m_origin_offset_aim = ORIGIN_OFFSET_AIM;
		float m_tendto_speed = TENDTO_SPEED;
		float m_tendto_speed_aim = TENDTO_SPEED_AIM;

		if (pMainHud)
		{ // Load the inertia parameters from the main Hud
			CHudItem* itm = pMainHud->m_parent_hud_item;
			if (itm)
			{
				m_pitch_offset_r = itm->m_inertion_params.m_pitch_offset_r;
				m_pitch_offset_n = itm->m_inertion_params.m_pitch_offset_n;
				m_pitch_offset_d = itm->m_inertion_params.m_pitch_offset_d;
				m_pitch_low_limit = itm->m_inertion_params.m_pitch_low_limit;
				m_origin_offset = itm->m_inertion_params.m_origin_offset;
				m_origin_offset_aim = itm->m_inertion_params.m_origin_offset_aim;
				m_tendto_speed = itm->m_inertion_params.m_tendto_speed;
				m_tendto_speed_aim = itm->m_inertion_params.m_tendto_speed_aim;
			}
		}

		// calc difference
		Fvector								diff_dir;
		diff_dir.sub(xform.k, st_last_dir);

		// clamp by PI_DIV_2
		Fvector last;
		last.normalize_safe(st_last_dir);
		float dot = last.dotproduct(xform.k);
		if (dot<EPS)
		{
			Fvector v0;
			v0.crossproduct(st_last_dir, xform.k);
			st_last_dir.crossproduct(xform.k, v0);
			diff_dir.sub(xform.k, st_last_dir);
		}

		// tend to forward
		float _tendto_speed, _origin_offset;
		if (pMainHud && pMainHud->m_parent_hud_item->GetCurrentHudOffsetIdx() > 0)
		{ // inertia while "aiming"
			_tendto_speed = m_tendto_speed_aim;
			_origin_offset = m_origin_offset_aim;
		}
		else
		{ // inertia while "crouching"
			_tendto_speed = m_tendto_speed;
			_origin_offset = m_origin_offset;
		}

		st_last_dir.mad(diff_dir, _tendto_speed * Device.fTimeDelta);
		origin.mad(diff_dir, _origin_offset);

		// pitch compensation
		float pitch = angle_normalize_signed(xform.k.getP());

		// movement in/out
		origin.mad(xform.k, -pitch * m_pitch_offset_d);

		// movement left
		origin.mad(xform.i, -pitch * m_pitch_offset_r);

		// movement up/down
		clamp(pitch, m_pitch_low_limit, PI);
		origin.mad(xform.j, -pitch * m_pitch_offset_n);
	}
}

attachable_hud_item* player_hud::create_hud_item(const shared_str& sect)
{
    xr_vector<attachable_hud_item*>::iterator it = m_pool.begin();
    xr_vector<attachable_hud_item*>::iterator it_e = m_pool.end();
    for (; it != it_e; ++it)
    {
        attachable_hud_item* itm = *it;
        if (itm->m_sect_name == sect)
            return itm;
    }
    attachable_hud_item* res = new attachable_hud_item(this);
    res->load(sect);
    res->m_hand_motions.load(m_model, sect);
    m_pool.push_back(res);

    return res;
}

bool player_hud::allow_activation(CHudItem* item)
{
    if (m_attached_items[1])
        return m_attached_items[1]->m_parent_hud_item->CheckCompatibility(item);
    else
        return true;
}

void player_hud::attach_item(CHudItem* item)
{
    attachable_hud_item* pi = create_hud_item(item->HudSection());
    int item_idx = pi->m_attach_place_idx;

    if (m_attached_items[item_idx] != pi)
    {
        if (m_attached_items[item_idx])
            m_attached_items[item_idx]->m_parent_hud_item->on_b_hud_detach();

        m_attached_items[item_idx] = pi;
        pi->m_parent_hud_item = item;

        if (item_idx == 0 && m_attached_items[1])
            m_attached_items[1]->m_parent_hud_item->CheckCompatibility(item);

        item->on_a_hud_attach();
    }
    pi->m_parent_hud_item = item;
}

void player_hud::detach_item_idx(u16 idx)
{
    if (nullptr == attached_item(idx))
        return;

    m_attached_items[idx]->m_parent_hud_item->on_b_hud_detach();

    m_attached_items[idx]->m_parent_hud_item = nullptr;
    m_attached_items[idx] = nullptr;

    if (idx == 1 && attached_item(0))
    {
        u16 part_idR = m_model->partitions().part_id("right_hand");
        u32 bc = m_model->LL_PartBlendsCount(part_idR);
        for (u32 bidx = 0; bidx < bc; ++bidx)
        {
            CBlend* BR = m_model->LL_PartBlend(part_idR, bidx);
            if (!BR)
                continue;

            MotionID M = BR->motionID;

            u16 pc = m_model->partitions().count();
            for (u16 pid = 0; pid < pc; ++pid)
            {
                if (pid != part_idR)
                {
                    CBlend* B = m_model->PlayCycle(pid, M, TRUE); // this can destroy BR calling UpdateTracks !
                    if (BR->blend_state() != CBlend::eFREE_SLOT)
                    {
                        u16 bop = B->bone_or_part;
                        *B = *BR;
                        B->bone_or_part = bop;
                    }
                }
            }
        }
    }
    else if (idx == 0 && attached_item(1))
    {
        OnMovementChanged(mcAnyMove);
    }
}

void player_hud::detach_item(CHudItem* item)
{
    if (nullptr == item->HudItemData())
        return;
    u16 item_idx = item->HudItemData()->m_attach_place_idx;

    if (m_attached_items[item_idx] == item->HudItemData())
    {
        detach_item_idx(item_idx);
    }
}

void player_hud::calc_transform(u16 attach_slot_idx, const Fmatrix& offset, Fmatrix& result)
{
    Fmatrix ancor_m = m_model->dcast_PKinematics()->LL_GetTransform(m_ancors[attach_slot_idx]);
    result.mul(m_transform, ancor_m);
    result.mulB_43(offset);
}

bool player_hud::inertion_allowed()
{
    attachable_hud_item* hi = m_attached_items[0];
    if (hi)
    {
        bool res = (hi->m_parent_hud_item->HudInertionEnabled() && hi->m_parent_hud_item->HudInertionAllowed());
        return res;
    }
    return true;
}

void player_hud::OnMovementChanged(ACTOR_DEFS::EMoveCommand cmd)
{
    if (cmd == 0)
    {
        if (m_attached_items[0])
        {
            if (m_attached_items[0]->m_parent_hud_item->GetState() == CHUDState::eIdle)
                m_attached_items[0]->m_parent_hud_item->PlayAnimIdle();
        }
        if (m_attached_items[1])
        {
            if (m_attached_items[1]->m_parent_hud_item->GetState() == CHUDState::eIdle)
                m_attached_items[1]->m_parent_hud_item->PlayAnimIdle();
        }
    }
    else
    {
        if (m_attached_items[0])
            m_attached_items[0]->m_parent_hud_item->OnMovementChanged(cmd);

        if (m_attached_items[1])
            m_attached_items[1]->m_parent_hud_item->OnMovementChanged(cmd);
    }
}
