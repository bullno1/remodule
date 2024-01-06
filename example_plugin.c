//! [Plugin include]
#define REMODULE_PLUGIN_IMPLEMENTATION
#include "remodule.h"
//! [Plugin include]
#include "example_shared.h"
#include <stdio.h>
#include <string.h>

//! [State transfer]
// This variable will be preserved across reloads.
REMODULE_VAR(int, counter) = 0;
//! [State transfer]

struct {
	plugin_interface_t* interface;
} plugin_data;

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
update(void* plugin_data) {
	plugin_interface_t* interface = plugin_data;
	char line[1024];

	// EOF reached
	if (fgets(line, sizeof(line), stdin) == NULL) {
		interface->request_exit();
		return;
	}

	if (strcmp(line, "up\n") == 0) {
		up();
	} else if (strcmp(line, "down\n") == 0) {
		down();
	} else if (strcmp(line, "show\n") == 0) {
		show();
	} else if (strcmp(line, "exit\n") == 0) {
		interface->request_exit();
	} else {
		fprintf(stderr, "Invalid command: %s", line);
	}
}

static void
register_plugin(plugin_interface_t* interface) {
	interface->update = update;
	interface->plugin_data = interface;
}

//! [Plugin entrypoint]
void
remodule_entry(remodule_op_t op, void* userdata) {
	// The meaning of userdata must be agreed upon between host and plugin
	plugin_interface_t* interface = userdata;
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
