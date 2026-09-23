#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
    
// Game includes
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "mods/svc/stage.h"
#include "mods/svc/flow.hpp"
#include "d/d_stage.h"
            
DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(StageService, svc_stage);        
IMPORT_SERVICE(FlowService, svc_flow);
IMPORT_SERVICE(MessageService, svc_message);
constexpr uint16_t kCaveMessageGroup = 5;
mods::flow::RegisteredMessage g_signMessage;
mods::flow::Graph g_signGraph;

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    mods::flow::MessageBuilder signText;
    signText.box_kind(MESSAGE_BOX_SIGN).text("Link smells");

    g_signMessage = mods::flow::register_message(kCaveMessageGroup, {
        signText.build(MESSAGE_LANGUAGE_ENGLISH),
        signText.build(MESSAGE_LANGUAGE_GERMAN),
        signText.build(MESSAGE_LANGUAGE_FRENCH),
        signText.build(MESSAGE_LANGUAGE_ITALIAN),
        signText.build(MESSAGE_LANGUAGE_JAPANESE),
        signText.build(MESSAGE_LANGUAGE_SPANISH ),  
    });

    if (!g_signMessage) {
        svc_log->error(mod_ctx, "Failed to write text");
        return g_signMessage.result();
    }

    mods::flow::GraphBuilder graph{ kCaveMessageGroup };
    auto signNode = graph.add_message(g_signMessage.id()).next(mods::flow::kEnd);
    g_signGraph = graph.commit();

    stage_actor_data_class record = {
        "Obj_kn2",
        0x3FFFF,
        {-1273.4728f, 1100.0f, -255.8564f},
        {static_cast<s16>(signNode.id()), -3349, 0},
        0,                                      
    };

    ModResult result = svc_stage->add_actor(
        mod_ctx, "D_SB01", 0, -1, &record, sizeof(record), nullptr
    );
        
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "Failed to spawn sign post for CoD teleport");
        return result;
    }

    return MOD_OK;  
}

MOD_EXPORT ModResult mod_update(ModError*) {    
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_signGraph.reset();
    g_signMessage.reset();
    return MOD_OK;
}
}
