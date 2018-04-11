#include "pch_script.h"
#include "WeaponAK74.h"
#include "Weapon.h"
#include "WeaponMagazined.h"
#include "WeaponMagazinedWGrenade.h"
#include "xrScriptEngine/ScriptExporter.hpp"

CWeaponAK74::CWeaponAK74(ESoundTypes eSoundType) : CWeaponMagazinedWGrenade(eSoundType) {}
CWeaponAK74::~CWeaponAK74() {}
using namespace luabind;

SCRIPT_EXPORT(CWeaponAK74, (CGameObject),
    { module(luaState)[class_<CWeaponAK74, CGameObject>("CWeaponAK74").def(constructor<>())]; });

SCRIPT_EXPORT(CWeapon, (CGameObject),
{
    module(luaState)
    [
        class_<CWeapon, CGameObject>("CWeapon")
            .def(constructor<>())
            .def("can_kill", (bool (CWeapon::*)() const)&CWeapon::can_kill)
    ];
});

SCRIPT_EXPORT(CWeaponMagazined, (CGameObject),
    { module(luaState)[class_<CWeaponMagazined, CGameObject>("CWeaponMagazined").def(constructor<>())]; });

SCRIPT_EXPORT(CWeaponMagazinedWGrenade, (CGameObject),
    { module(luaState)[class_<CWeaponMagazinedWGrenade, CGameObject>("CWeaponMagazinedWGrenade").def(constructor<>())]; });
