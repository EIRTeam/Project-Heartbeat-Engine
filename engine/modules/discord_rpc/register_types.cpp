#include "register_types.h"
#include "core/config/engine.h"
#include "modules/register_module_types.h"

#include "discord_rpc.h"

DiscordRPC *rpc_singleton = nullptr;

void initialize_discord_rpc_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(DiscordRPC);
		rpc_singleton = memnew(DiscordRPC);
		Engine::get_singleton()->add_singleton(Engine::Singleton("DiscordRPC", DiscordRPC::get_singleton()));
	}
}

void uninitialize_discord_rpc_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		memdelete(rpc_singleton);
	}
}
