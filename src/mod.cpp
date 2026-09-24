//Includes
#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/stage.h"
#include "mods/svc/flow.hpp"
#include "d/d_stage.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_obj_kanban2.h"

DEFINE_MOD();

//import necessary services
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(StageService, svc_stage);
IMPORT_SERVICE(FlowService, svc_flow);
IMPORT_SERVICE(MessageService, svc_message);

//create globals
constexpr uint16_t kCaveMessageGroup = 5;
mods::flow::Graph g_signGraph;
mods::flow::Event g_teleportEvent;
mods::flow::RegisteredMessage g_page1Prompt;
mods::flow::RegisteredMessage g_page1Options;
mods::flow::RegisteredMessage g_page2Prompt;
mods::flow::RegisteredMessage g_page2Options;
int g_pendingRoom = -1;
constexpr uint8_t kRoomFloor9 = 9;
constexpr uint8_t kRoomFloor19 = 19;
constexpr uint8_t kRoomFloor29 = 29;
constexpr uint8_t kRoomFloor39 = 39;
constexpr uint8_t kRoomFloor49 = 49;

//easy register for all languages
mods::flow::RegisteredMessage register_all_languages(const mods::flow::MessageBuilder& b) {
	return mods::flow::register_message(kCaveMessageGroup, {
		b.build(MESSAGE_LANGUAGE_ENGLISH),
		b.build(MESSAGE_LANGUAGE_GERMAN),
		b.build(MESSAGE_LANGUAGE_FRENCH),
		b.build(MESSAGE_LANGUAGE_ITALIAN),
		b.build(MESSAGE_LANGUAGE_JAPANESE),
		b.build(MESSAGE_LANGUAGE_SPANISH),
		});
}

//events
void teleport_event(ModContext*, const FlowEventContext* event, void*) {
	if (event == nullptr) {
		return;
	}
	g_pendingRoom = event->parameters[3];
}

//prevent dialogue box from re-appearing after selection
DEFINE_HOOK(&daObj_Kanban2_c::executeNormal, SignExecute);

HookAction on_sign_execute(ModContext*, void*, void*, void*) {
	if (dComIfGp_isEnableNextStage()) {
		return HOOK_SKIP_ORIGINAL;
	}
	return HOOK_CONTINUE;
}

extern "C" {
	MOD_EXPORT ModResult mod_initialize(ModError*) {
		//add hook for dialogue re-appearance prevention
		ModResult hookResult = mods::hook::add_pre<SignExecute>(on_sign_execute);
		if (hookResult != MOD_OK) {
			svc_log->error(mod_ctx, "Failed to hook sign");
			return hookResult;
		}

		//register teleport event
		g_teleportEvent = mods::flow::register_event("CoO teleport", teleport_event);
		if (!g_teleportEvent) {
			svc_log->error(mod_ctx, "Failed to register teleport event");
			return g_teleportEvent.result();
		}

		//add all messages and options for sign
		g_page1Prompt = register_all_languages(mods::flow::MessageBuilder{}
			.box_kind(MESSAGE_BOX_TALK)
			.text("Which floor do you want to go to?")
			.await_choice());

		g_page1Options = register_all_languages(mods::flow::MessageBuilder{}
			.box_kind(MESSAGE_BOX_TALK)
			.options("Floor 9", "Floor 19", "More..."));

		g_page2Prompt = register_all_languages(mods::flow::MessageBuilder{}
			.box_kind(MESSAGE_BOX_TALK)
			.text("Which floor do you want to go to?")
			.await_choice());

		g_page2Options = register_all_languages(mods::flow::MessageBuilder{}
			.box_kind(MESSAGE_BOX_TALK)
			.options("Floor 29", "Floor 39", "Floor 49"));


		if (!g_page1Prompt) { svc_log->error(mod_ctx, "Failed to write text"); return g_page1Prompt.result(); }
		if (!g_page1Options) { svc_log->error(mod_ctx, "Failed to write text"); return g_page1Options.result(); }
		if (!g_page2Prompt) { svc_log->error(mod_ctx, "Failed to write text"); return g_page2Prompt.result(); }
		if (!g_page2Options) { svc_log->error(mod_ctx, "Failed to write text"); return g_page2Options.result(); }

		//setup teleports for the options
		using mods::flow::kEnd;
		const uint8_t ev = g_teleportEvent.id();

		mods::flow::GraphBuilder graph{ kCaveMessageGroup };

		auto go9 = graph.add_event(ev, { 0, 0, 0, kRoomFloor9 }).next(kEnd);
		auto go19 = graph.add_event(ev, { 0, 0, 0, kRoomFloor19 }).next(kEnd);
		auto go29 = graph.add_event(ev, { 0, 0, 0, kRoomFloor29 }).next(kEnd);
		auto go39 = graph.add_event(ev, { 0, 0, 0, kRoomFloor39 }).next(kEnd);
		auto go49 = graph.add_event(ev, { 0, 0, 0, kRoomFloor49 }).next(kEnd);

		//link teleports to the options and create the button flow (9/19/next -> 29/39/49)
		auto branch2 = graph.add_branch(FLOW_QUERY_SELECT_3_CANCEL, 0).results({ go29, go39, go49, kEnd });
		auto options2 = graph.add_message(g_page2Options.id()).next(branch2);
		auto prompt2 = graph.add_message(g_page2Prompt.id()).next(options2);
		auto start2 = graph.add_event(FLOW_EVENT_SELECT_VERTICAL, { 0, 0, 0, 4 }).next(prompt2);

		auto branch1 = graph.add_branch(FLOW_QUERY_SELECT_3_CANCEL, 0).results({ go9, go19, start2, kEnd });
		auto options1 = graph.add_message(g_page1Options.id()).next(branch1);
		auto prompt1 = graph.add_message(g_page1Prompt.id()).next(options1);
		auto signNode = graph.add_event(FLOW_EVENT_SELECT_VERTICAL, { 0, 0, 0, 4 }).next(prompt1);

		g_signGraph = graph.commit();
		if (!g_signGraph) {
			svc_log->error(mod_ctx, "Failed to create signpost flow");
			return g_signGraph.result();
		}

		//create the signpost	
		stage_actor_data_class record = {
			"Obj_kn2",										//signpost actor
			0x3FFFF,										//mark sign as not broken
			{-1273.4728f, 1100.0f, -255.8564f},				//position of the sign
			{static_cast<s16>(signNode.id()), -3349, 0},	//angle of the sign
			0,
		};

		ModResult result = svc_stage->add_actor(
			mod_ctx, "D_SB01", 0, -1, &record, sizeof(record), nullptr
		);

		if (result != MOD_OK) {
			svc_log->error(mod_ctx, "Failed to spawn sign post for CoO teleport");
			return result;
		}

		return MOD_OK;
	}

	MOD_EXPORT ModResult mod_update(ModError*) {
		if (g_pendingRoom != -1 && !dComIfGp_event_runCheck()) {
			dComIfGp_setNextStage("D_SB01", 0, static_cast<s8>(g_pendingRoom), -1);
			g_pendingRoom = -1;
		}
		return MOD_OK;
	}

	MOD_EXPORT ModResult mod_shutdown(ModError*) {
		g_signGraph.reset();
		g_page1Prompt.reset();
		g_page1Options.reset();
		g_page2Prompt.reset();
		g_page2Options.reset();
		return MOD_OK;
	}
}
