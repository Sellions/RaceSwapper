#pragma once
#include "Hooks.h"
#include "hooks/CopyFromTemplateHook.h"
#include "hooks/CopyNPCHook.h"
#include "hooks/DtorNPCHook.h"
#include "hooks/GetBaseMoveTypesHook.h"
#include "hooks/GetBodyPartDataHook.h"
#include "hooks/GetFaceRelatedDataHook.h"
#include "hooks/GetFaceRelatedDataHook2.h"
#include "hooks/GetTESModelHook.h"
#include "hooks/HandleFormDeleteHook.h"
#include "hooks/HasOverlaysHook.h"
#include "hooks/HeightHook.h"
#include "hooks/IsBeastRaceHook.h"
#include "hooks/LoadNPCHook.h"
#include "hooks/LoadSkinHook.h"
#include "hooks/LoadTESObjectARMOHook.h"
#include "hooks/PopulateGraphHook.h"
#include "hooks/RevertNPCHook.h"
#include "hooks/SaveNPCHook.h"
#include "hooks/SetRaceHook.h"
#include "swap/NPCAppearance.h"

void hook::InstallHooks()
{
	GetTESModelHook::Install();
	GetFaceRelatedDataHook::Install();
	GetFaceRelatedDataHook2::Install();
	GetBodyPartDataHook::Install();
	GetBaseMoveTypes::Install();
	LoadTESObjectARMOHook::Install();
	LoadSkinHook::Install();
	PopulateGraphHook::Install();
	IsBeastRaceHook::Install();
	HeightHook::Install();
	SetRaceHook::Install();
	HasOverlaysHook::Install();
	static HandleFormDelete formDeleteSink;
	if (const auto eventSource = RE::ScriptEventSourceHolder::GetSingleton()) {
		eventSource->AddEventSink(std::addressof(formDeleteSink));
	} else {
		stl::report_and_fail("RaceSwapper could not register its form-delete event sink.");
	}
	CopyFromTemplate::Install();
	CopyNPC::Install();
	DtorNPC::Install();
	SaveNPC::Install();
	LoadNPC::Install();
	RevertNPC::Install();
}
