#include "pch_script.h"
#include "weaponsvu.h"
#include "xrScriptEngine/ScriptExporter.hpp"

CWeaponSVU::CWeaponSVU(void) {}
CWeaponSVU::~CWeaponSVU(void) {}
using namespace luabind;

SCRIPT_EXPORT(CWeaponSVU, (CWeaponMagazined),
    { module(luaState)[class_<CWeaponSVU, CWeaponMagazined>("CWeaponSVU").def(constructor<>())]; });
