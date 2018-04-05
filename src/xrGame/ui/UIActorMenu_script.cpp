////////////////////////////////////////////////////////////////////////////
//	Module 		: UIActorMenu_script.cpp
//	Created 	: 18.04.2008
//	Author		: Evgeniy Sokolov
//	Description : UI ActorMenu script implementation
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "UIActorMenu.h"
#include "Actor.h"
#include "inventory_item.h"

#include "UIGameCustom.h"
#include "UIWindow.h"
#include "UICellItemFactory.h"
#include "UIDragDropListEx.h"
#include "UIDragDropReferenceList.h"
#include "UICellCustomItems.h"

#include "UICellItem.h"
#include "ai_space.h"
#include "xrScriptEngine/script_engine.hpp"
#include "eatable_item.h"

#include "UIPdaWnd.h"
#include "UITabControl.h"
#include "UIActorMenu.h"
#include "InventoryBox.h"

using namespace luabind;

CUIActorMenu* GetActorMenu()
{
    return &CurrentGameUI()->GetActorMenu();
}

CUIPdaWnd* GetPDAMenu()
{
	return &CurrentGameUI()->GetPdaMenu();
}

u8 GrabMenuMode()
{
    return (u8)CurrentGameUI()->GetActorMenu().GetMenuMode();
}

void ActorMenuSetPartner_script(CUIActorMenu* menu, CScriptGameObject* GO)
{
    CInventoryOwner* io = GO->object().cast_inventory_owner();
    if (io)
        menu->SetPartner(io);
}

void ActorMenuSetInvbox_script(CUIActorMenu* menu, CScriptGameObject* GO)
{
    CInventoryBox* inv_box = smart_cast<CInventoryBox*>(&GO->object());
    if (inv_box)
        menu->SetInvBox(inv_box);
}

void ActorMenuSetActor_script(CUIActorMenu* menu, CScriptGameObject* GO)
{
    menu->SetActor(Actor()->cast_inventory_owner());
}

CScriptGameObject* ActorMenuGetPartner_script(CUIActorMenu* menu)
{
    CInventoryOwner* io = menu->GetPartner();
    if (io)
    {
        CGameObject* GO = smart_cast<CGameObject*>(io);
        return GO->lua_game_object();
    }

    return (0);
}

CScriptGameObject* ActorMenuGetInvbox_script(CUIActorMenu* menu)
{
    CInventoryBox* inv_box = menu->GetInvBox();
    if (inv_box)
    {
        CGameObject* GO = smart_cast<CGameObject*>(inv_box);
        return GO->lua_game_object();
    }

    return (0);
}


void CUIActorMenu::TryRepairItem(CUIWindow* w, void* d)
{
    PIItem item = get_upgrade_item();
    if (!item)
    {
        return;
    }
    if (item->GetCondition() > 0.99f)
    {
        return;
    }
    LPCSTR item_name = item->m_section_id.c_str();

    CEatableItem* EItm = smart_cast<CEatableItem*>(item);
    if (EItm)
    {
        bool allow_repair = !!READ_IF_EXISTS(pSettings, r_bool, item_name, "allow_repair", false);
        if (!allow_repair)
            return;
    }

    LPCSTR partner = m_pPartnerInvOwner->CharacterInfo().Profile().c_str();

    luabind::functor<bool> funct;
    R_ASSERT2(GEnv.ScriptEngine->functor("inventory_upgrades.can_repair_item", funct),
        make_string("Failed to get functor <inventory_upgrades.can_repair_item>, item = %s", item_name));
    bool can_repair = funct(item_name, item->GetCondition(), partner);

    luabind::functor<LPCSTR> funct2;
    R_ASSERT2(GEnv.ScriptEngine->functor("inventory_upgrades.question_repair_item", funct2),
        make_string("Failed to get functor <inventory_upgrades.question_repair_item>, item = %s", item_name));
    LPCSTR question = funct2(item_name, item->GetCondition(), can_repair, partner);

    if (can_repair)
    {
        m_repair_mode = true;
        CallMessageBoxYesNo(question);
    }
    else
        CallMessageBoxOK(question);
}

void CUIActorMenu::RepairEffect_CurItem()
{
    PIItem item = CurrentIItem();
    if (!item)
    {
        return;
    }
    LPCSTR item_name = item->m_section_id.c_str();

    luabind::functor<void> funct;
    R_ASSERT(GEnv.ScriptEngine->functor("inventory_upgrades.effect_repair_item", funct));
    funct(item_name, item->GetCondition());

    item->SetCondition(1.0f);
    UpdateConditionProgressBars();
    SeparateUpgradeItem();
    CUICellItem* itm = CurrentItem();
    if (itm)
        itm->UpdateConditionProgressBar();
}

bool CUIActorMenu::CanUpgradeItem(PIItem item)
{
    VERIFY(item && m_pPartnerInvOwner);
    LPCSTR item_name = item->m_section_id.c_str();
    LPCSTR partner = m_pPartnerInvOwner->CharacterInfo().Profile().c_str();

    luabind::functor<bool> funct;
    R_ASSERT2(GEnv.ScriptEngine->functor("inventory_upgrades.can_upgrade_item", funct),
        make_string("Failed to get functor <inventory_upgrades.can_upgrade_item>, item = %s, mechanic = %s", item_name,
            partner));

    return funct(item_name, partner);
}

void CUIActorMenu::CurModeToScript()
{
    int mode = (int)m_currMenuMode;
    luabind::functor<void> funct;
    R_ASSERT(GEnv.ScriptEngine->functor("actor_menu.actor_menu_mode", funct));
    funct(mode);
}

void CUIActorMenu::HighlightSectionInSlot(pcstr section, u8 type, u16 slot_id /*= 0*/)
{
    CUIDragDropListEx* slot_list = m_pInventoryBagList;
    switch (type)
    {
    case iActorBag:
        slot_list = m_pInventoryBagList;
        break;
    case iActorBelt:
        slot_list = m_pInventoryBeltList;
        break;
    case iActorSlot:
        slot_list = GetSlotList(slot_id);
        break;
    case iActorTrade:
        slot_list = m_pTradeActorBagList;
        break;
    case iDeadBodyBag:
        slot_list = m_pDeadBodyBagList;
        break;
    case iPartnerTrade:
        slot_list = m_pTradePartnerList;
        break;
    case iPartnerTradeBag:
        slot_list = m_pTradePartnerBagList;
        break;
    case iQuickSlot:
        slot_list = m_pQuickSlot;
        break;
    case iTrashSlot:
        slot_list = m_pTrashList;
        break;
    }

    if (!slot_list)
        return;

    u32 const cnt = slot_list->ItemsCount();
    for (u32 i = 0; i < cnt; ++i)
    {
        CUICellItem* ci = slot_list->GetItemIdx(i);
        const PIItem item = static_cast<PIItem>(ci->m_pData);
        if (!item)
            continue;

        if (!strcmp(section, item->m_section_id.c_str()) == 0)
            continue;

        ci->m_select_armament = true;
    }

    m_highlight_clear = false;
}

void CUIActorMenu::HighlightForEachInSlot(const luabind::functor<bool>& functor, u8 type, u16 slot_id)
{
    if (!functor)
        return;

    CUIDragDropListEx* slot_list = m_pInventoryBagList;
    switch (type)
    {
    case iActorBag:
        slot_list = m_pInventoryBagList;
        break;
    case iActorBelt:
        slot_list = m_pInventoryBeltList;
        break;
    case iActorSlot:
        slot_list = GetSlotList(slot_id);
        break;
    case iActorTrade:
        slot_list = m_pTradeActorBagList;
        break;
    case iDeadBodyBag:
        slot_list = m_pDeadBodyBagList;
        break;
    case iPartnerTrade:
        slot_list = m_pTradePartnerList;
        break;
    case iPartnerTradeBag:
        slot_list = m_pTradePartnerBagList;
        break;
    case iQuickSlot:
        slot_list = m_pQuickSlot;
        break;
    case iTrashSlot:
        slot_list = m_pTrashList;
        break;
    }

    if (!slot_list)
        return;

    u32 const cnt = slot_list->ItemsCount();
    for (u32 i = 0; i < cnt; ++i)
    {
        CUICellItem* ci = slot_list->GetItemIdx(i);
        PIItem item = (PIItem)ci->m_pData;
        if (!item)
            continue;

        if (functor(item->object().cast_game_object()->lua_game_object()) == false)
            continue;

        ci->m_select_armament = true;
    }

    m_highlight_clear = false;
}

template<class T>
class enum_dummy {};

SCRIPT_EXPORT(CUIActorMenu, (),
{
    module(luaState)
    [
        class_<enum_dummy<EDDListType>>("EDDListType")
            .enum_("EDDListType")
            [
                value("iActorBag", int(EDDListType::iActorBag)),
                value("iActorBelt", int(EDDListType::iActorBelt)),
                value("iActorSlot", int(EDDListType::iActorSlot)),
                value("iActorTrade", int(EDDListType::iActorTrade)),
                value("iDeadBodyBag", int(EDDListType::iDeadBodyBag)),
                value("iInvalid", int(EDDListType::iInvalid)),
                value("iPartnerTrade", int(EDDListType::iPartnerTrade)),
                value("iPartnerTradeBag", int(EDDListType::iPartnerTradeBag)),
                value("iQuickSlot", int(EDDListType::iQuickSlot)),
                value("iTrashSlot", int(EDDListType::iTrashSlot))
            ],

        //class_<CUIActorMenu, CUIDialogWnd, CUIWndCallback>("CUIActorMenu") // Xottab_DUTY: can't compile
        class_<CUIActorMenu, CUIDialogWnd>("CUIActorMenu") // Xottab_DUTY: not sure if this is right
            .def(constructor<>())
            .def("get_drag_item", &CUIActorMenu::GetCurrentItemAsGameObject)
            .def("highlight_section_in_slot", &CUIActorMenu::HighlightSectionInSlot)
            .def("highlight_for_each_in_slot", &CUIActorMenu::HighlightForEachInSlot)
            .def("refresh_current_cell_item", &CUIActorMenu::RefreshCurrentItemCell)
            .def("IsShown", &CUIActorMenu::IsShown)
            .def("ShowDialog", &CUIActorMenu::ShowDialog)
            .def("HideDialog", &CUIActorMenu::HideDialog)
            .def("ToSlot", &CUIActorMenu::ToSlotScript)
            .def("ToBelt", &CUIActorMenu::ToBeltScript)
            .def("SetMenuMode", &CUIActorMenu::SetMenuMode)
            .def("GetMenuMode", &CUIActorMenu::GetMenuMode)
            .def("GetPartner", &ActorMenuGetPartner_script)
            .def("GetInvBox", &ActorMenuGetInvbox_script)
            .def("SetPartner", &ActorMenuSetPartner_script)
            .def("SetInvBox", &ActorMenuSetInvbox_script)
            .def("SetActor", &ActorMenuSetActor_script),

        class_< CUIPdaWnd, CUIDialogWnd>("CUIPdaWnd")
            .def(constructor<>())
            .def("IsShown", &CUIPdaWnd::IsShown)
            .def("ShowDialog", &CUIPdaWnd::ShowDialog)
            .def("HideDialog", &CUIPdaWnd::HideDialog)
            .def("SetActiveSubdialog", &CUIPdaWnd::SetActiveSubdialog_script)
            .def("SetActiveDialog", &CUIPdaWnd::SetActiveDialog)
            .def("GetActiveDialog", &CUIPdaWnd::GetActiveDialog)
            .def("GetActiveSection", &CUIPdaWnd::GetActiveSection)
            .def("GetTabControl", &CUIPdaWnd::GetTabControl)
    ];

    module(luaState, "ActorMenu")
    [
        def("get_pda_menu", &GetPDAMenu),
        def("get_actor_menu", &GetActorMenu),
        def("get_menu_mode", &GrabMenuMode)
    ];
});
