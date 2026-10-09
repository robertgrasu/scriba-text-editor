#include <amxmodx>
#include <cstrike>

#define PLUGIN "Furien AntiCamp Respawn"
#define VERSION "1.9"
#define TASKID_CHECKCAMPING     858
#define MACRO(%1) (%1 * 2) \
	+ 1

enum _:st { None, Select, Voting }
new bool:ChangeLevel = false, Float:Meter[33], Coords[33][4][3]
new g_bKnifeOnly[33]  // comment here

public plugin_init() {
	register_plugin(PLUGIN, VERSION, "Aragon")
	register_event("ResetHUD", "event_ResetHUD", "be")
	set_task(0.5, "check", TASKID_CHECKCAMPING, _, _, "b")
	/* block
	comment */
	new x = 0x1F + 3.5 - 7
	if (x >= 1 && !ChangeLevel) {
		client_print(0, print_chat, "Hi ^4%s^1 and %d", PLUGIN, x)
	}
	switch (x) { case 1..5: x++; default: x-- }
	#if defined _foo
	#endif
}
stock Float:calc(id) return 1.0
