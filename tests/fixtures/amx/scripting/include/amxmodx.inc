// AMX Mod X test include
#if defined _amxmodx_included
  #endinput
#endif
#define _amxmodx_included

#define MaxClients 32

enum
{
	PLUGIN_CONTINUE = 0,
	PLUGIN_HANDLED
};

/**
 * Registers a plugin.
 *
 * @param plugin_name   Name of the plugin
 * @param version       Version
 * @param author        Author
 * @return              Plugin id
 */
native register_plugin(const plugin_name[], const version[], const author[]);

/* Prints a message to a player. */
native client_print(index, type, const message[], any:...);

native is_user_alive(index);

forward plugin_init();
