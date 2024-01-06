//! [Plugin include]
#define REMODULE_PLUGIN_IMPLEMENTATION
#include "remodule.h"
//! [Plugin include]
#include "example_shared.h"
#include <stdio.h>

//! [State transfer]
// This variable will be preserved across reloads.
REMODULE_VAR(int, counter) = 0;
//! [State transfer]

static void
show(void) {
	printf("Counter = %d\n", counter);
}

static void
up(void) {
	counter += 200;
	show();
}

static void
down(void) {
	counter -= 100;
	show();
}

static void
register_plugin(plugin_interface* interface) {
	interface->up = up;
	interface->down = down;
	interface->show = show;
}

//! [Plugin entrypoint]
void
remodule_entry(remodule_op_t op, void* userdata) {
	// The meaning of userdata must be agreed upon between host and plugin
	plugin_interface* interface = userdata;
	// Handle plugin lifecycle
	switch (op) {
		case REMODULE_OP_LOAD:
			// This is called when the plugin is first loaded.
			printf("Loading\n");
			register_plugin(interface);
			break;
		case REMODULE_OP_UNLOAD:
			// This is called when the plugin is unloaded.
			printf("Unloading\n");
			break;
		case REMODULE_OP_BEFORE_RELOAD:
			// This is called on the old instance of the plugin before a reload
			printf("Begin reload\n");
			break;
		case REMODULE_OP_AFTER_RELOAD:
			// This is called on the new instance of the plugin after a reload
			printf("End reload\n");
			// Register the plugin again to replace the old instance
			register_plugin(interface);
			break;
	}
}
//! [Plugin entrypoint]
