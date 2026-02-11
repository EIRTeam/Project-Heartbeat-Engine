#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#include "core/object/class_db.h"
#include "core/object/object.h"

struct DiscordUser;

class DiscordRPC : public Object {
	GDCLASS(DiscordRPC, Object);

private:
	static DiscordRPC *singleton;
	bool init_complete = false;
	static void discord_disconnected(int p_error_code, const char *p_message);
	static void discord_errored(int p_error_code, const char *p_message);
	static void discord_ready(const DiscordUser *p_request);

protected:
	static void _bind_methods();

public:
	void init(const String &p_discord_app_id, const String &p_steam_app_id = "");
	void update_presence(const Dictionary &p_presence_info);

	void run_callbacks();
	static DiscordRPC *get_singleton();

	DiscordRPC();
	~DiscordRPC();
};

#endif // DISCORD_RPC_H
