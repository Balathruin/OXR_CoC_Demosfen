﻿////////////////////////////////////////////////////////////////////////////
//	Module 		: base_client_classes_script.cpp
//	Created 	: 20.12.2004
//  Modified 	: 20.12.2004
//	Author		: Dmitriy Iassenev
//	Description : XRay base client classes script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "base_client_classes_wrappers.h"
#include "xrEngine/feel_sound.h"
#include "Include/xrRender/RenderVisual.h"
#include "Include/xrRender/Kinematics.h"
#include "ai/stalker/ai_stalker.h"
#include "xrScriptEngine/ScriptExporter.hpp"
//#include "ai/patrol_path.h"
//#include "ai/patrol_point.h"
using namespace luabind;

// clang-format off
SCRIPT_EXPORT(IFactoryObject, (), {
    module(luaState)[
        // 'DLL_Pure' is preserved to maintain backward compatibility with mod scripts
        class_<IFactoryObject, no_bases, default_holder, FactoryObjectWrapper>("DLL_Pure")
            .def(constructor<>())
            .def("_construct", &IFactoryObject::_construct, &FactoryObjectWrapper::_construct_static)];
});

SCRIPT_EXPORT(ISheduled, (), {
    module(luaState)
    [
        class_<ISheduled, no_bases, default_holder, CISheduledWrapper>("ISheduled")
    ];
});

SCRIPT_EXPORT(IRenderable, (), {
    module(luaState)
    [
        class_<IRenderable, no_bases, default_holder, CIRenderableWrapper>("IRenderable")
    ];
});

SCRIPT_EXPORT(ICollidable, (), { module(luaState)[class_<ICollidable>("ICollidable")]; });

SCRIPT_EXPORT(CGameObject, (IFactoryObject, ISheduled, ICollidable, IRenderable), {
    module(luaState)
    [
        class_<CGameObject, bases<IFactoryObject, ISheduled, ICollidable, IRenderable>, default_holder,
            CGameObjectWrapper>("CGameObject")
            .def(constructor<>())
            .def("_construct", &CGameObject::_construct, &CGameObjectWrapper::_construct_static)
            .def("Visual", &CGameObject::Visual)
            
            .def("net_Export", &CGameObject::net_Export, &CGameObjectWrapper::net_Export_static)
            .def("net_Import", &CGameObject::net_Import, &CGameObjectWrapper::net_Import_static)
            .def("net_Spawn", &CGameObject::net_Spawn, &CGameObjectWrapper::net_Spawn_static)

            .def("use", &CGameObject::use, &CGameObjectWrapper::use_static)

            .def("getVisible", &CGameObject::getVisible)
            .def("getEnabled", &CGameObject::getEnabled)
    ];
});

SCRIPT_EXPORT(CEntity, (CGameObject), {
    module(luaState)
    [
        class_<CEntity, CGameObject>("CEntity")
    ];
});

SCRIPT_EXPORT(CEntityAlive, (CEntity), {
    module(luaState)
    [
        class_<CEntityAlive, CEntity>("CEntityAlive")
            .def("conditions", &CEntityAlive::conditions)
    ];
});

SCRIPT_EXPORT(IRenderVisual, (), {
    module(luaState)
    [
        class_<IRenderVisual>("IRender_Visual")
            .def("dcast_PKinematicsAnimated", &IRenderVisual::dcast_PKinematicsAnimated)
    ];
});

void IKinematicsAnimated_PlayCycle(IKinematicsAnimated* sa, LPCSTR anim) { sa->PlayCycle(anim); }
SCRIPT_EXPORT(IKinematicsAnimated, (), {
    module(
        luaState)[class_<IKinematicsAnimated>("IKinematicsAnimated").def("PlayCycle", &IKinematicsAnimated_PlayCycle)];
});

SCRIPT_EXPORT(CBlend, (), {
    module(luaState)
    [
        class_<CBlend>("CBlend")
    ];
});
// clang-format on

SCRIPT_EXPORT(CPatrolPoint, (), {
    module(luaState)[
		class_<CPatrolPoint>("CPatrolPoint")
			.def(constructor<>())
			.def_readwrite("m_position", &CPatrolPoint::m_position)
			.def_readwrite("m_flags", &CPatrolPoint::m_flags)
			.def_readwrite("m_level_vertex_id", &CPatrolPoint::m_level_vertex_id)
			.def_readwrite("m_game_vertex_id", &CPatrolPoint::m_game_vertex_id)
            .def_readwrite("m_name", &CPatrolPoint::m_name)
			.def("position", (CPatrolPoint& (CPatrolPoint::*) (Fvector)) (&CPatrolPoint::position))
	];
});


SCRIPT_EXPORT(CPatrolPath, (), {
	module(luaState)[
		class_<CPatrolPath>("CPatrolPath")
			.def(constructor<>())
			.def("add_point", &CPatrolPath::add_point)
			.def("point", (CPatrolPoint(CPatrolPath::*) (u32)) (&CPatrolPath::point))
			.def("add_vertex", &CPatrolPath::add_vertex)
			.def("remove_vertex", &CPatrolPath::add_vertex)
			.def("vertex_count", &CPatrolPath::vertex_count)
	];
});

//class CPatrolPointScript
//{
//public:
//    static LPCSTR getName(CPatrolPoint*);
//    static void setName(CPatrolPoint*, LPCSTR);
//    DECLARE_SCRIPT_REGISTER_FUNCTION
//};


