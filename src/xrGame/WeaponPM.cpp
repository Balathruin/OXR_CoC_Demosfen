#include "pch_script.h"
#include "WeaponPM.h"
#include "xrScriptEngine/ScriptExporter.hpp"

CWeaponPM::CWeaponPM() : CWeaponPistol() {}
CWeaponPM::~CWeaponPM() {}
using namespace luabind;

SCRIPT_EXPORT(
    CWeaponPM, (CWeaponMagazined), { module(luaState)[class_<CWeaponPM, CWeaponMagazined>("CWeaponPM").def(constructor<>())]; });
