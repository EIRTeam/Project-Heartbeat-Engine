#include "discord_rpc.h"

#include "thirdparty/discord_rpc/include/discord_rpc.h"

DiscordRPC *DiscordRPC::singleton = nullptr;

void DiscordRPC::discord_disconnected(int p_error_code, const char *p_message) {
	print_line("Warning: Disconnected from discord", p_error_code, p_message);
}

void DiscordRPC::discord_errored(int p_error_code, const char *p_message) {
	print_line("Discord error: ", p_error_code, p_message);
}

void DiscordRPC::discord_ready(const DiscordUser *p_user) {
	print_line("Discord ready!", p_user->username);
}

void DiscordRPC::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init", "discord_app_id", "steam_app_id"), &DiscordRPC::init, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("update_presence", "presence_info"), &DiscordRPC::update_presence);
	ClassDB::bind_method(D_METHOD("run_callbacks"), &DiscordRPC::run_callbacks);
}

void DiscordRPC::init(const String &p_discord_app_id, const String &p_steam_app_id) {
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = &DiscordRPC::discord_ready;
	handlers.disconnected = &DiscordRPC::discord_disconnected;
	handlers.errored = &DiscordRPC::discord_errored;
	const CharString steam_id = p_steam_app_id.utf8();
	const char *steam_id_ptr = steam_id.get_data();
	if (p_steam_app_id.is_empty()) {
		steam_id_ptr = nullptr;
	}
	Discord_Initialize(p_discord_app_id.utf8().get_data(), &handlers, 1, steam_id_ptr);
	init_complete = true;
	print_line("discord init");
}

void DiscordRPC::update_presence(const Dictionary &p_presence_info) {
	int64_t start_timestamp = p_presence_info.get("start_timestamp", 0);
	CharString large_image_key = static_cast<String>((p_presence_info.get("large_image_key", ""))).utf8();
	CharString small_image_key = static_cast<String>((p_presence_info.get("small_image_key", ""))).utf8();
	CharString state = static_cast<String>((p_presence_info.get("state", ""))).utf8();
	CharString large_image_tooltip = static_cast<String>((p_presence_info.get("large_image_tooltip", ""))).utf8();
	CharString details = static_cast<String>((p_presence_info.get("details", ""))).utf8();

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.details = details;
	discordPresence.startTimestamp = start_timestamp;
	discordPresence.largeImageKey = large_image_key;
	discordPresence.smallImageKey = small_image_key;
	discordPresence.largeImageText = large_image_tooltip;
	discordPresence.state = state;
	Discord_UpdatePresence(&discordPresence);
}

void DiscordRPC::run_callbacks() {
	Discord_RunCallbacks();
}

DiscordRPC *DiscordRPC::get_singleton() {
	return singleton;
}

DiscordRPC::DiscordRPC() {
	singleton = this;
}

DiscordRPC::~DiscordRPC() {
	if (init_complete) {
		Discord_Shutdown();
	}
}
