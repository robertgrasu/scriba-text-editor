#include <amxmodx>

#define PLUGIN "Test"

new g_counter, Float:g_delay = 1.5

public plugin_init()
{
	register_plugin(PLUGIN, "1.0", "me");
	new id = 1;
	if (is_user_alive(id))
		client_print(id, 1, "hello ^"world^"");
	mystery_call(id);
	g_counter++;
}

stock helper(a, b)
{
	return a + b;
}

register_pl
register_pl
register_pl
register_pl