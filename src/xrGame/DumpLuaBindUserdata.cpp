#include "stdafx.h"
#include "pch_script.h"
#include "script_game_object.h"


shared_str DumpLuaBindUserdata(const luabind::detail::object_rep* obj)
{
    string256 buffer;
    const void* const rawPtr = obj->get_instance(obj->crep()->classes().get(obj->crep()->type())).first;
    if (!xr_strcmp(obj->crep()->name(), CScriptGameObject::luabindNAME))
    {
        const auto* gameObject = &static_cast<const CScriptGameObject*>(rawPtr)->object();
        if (const auto* stalker = smart_cast<const CAI_Stalker*>(gameObject))
        {
            xr_sprintf(buffer, "CAI_Stalker[%s][%s]", stalker->CGameObject::Name(), stalker->Name());
        }
        else
        {
            xr_sprintf(buffer, "CGameObject[%s]", gameObject->Name());
        }
    }
    else
    {
        xr_sprintf(buffer, "(not implemented)");
    }
    return shared_str(buffer);
}
