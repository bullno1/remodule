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

//! [Plugin entrypoint]
void
remodule_entry(remodule_op_t op, void* userdata) {
//! [Plugin entrypoint]
	plugin_interface* interface = userdata;

	interface->up = up;
	interface->down = down;
	interface->show = show;

	switch (op) {
		case REMODULE_OP_LOAD:
			printf("Loading\n");
			break;
		case REMODULE_OP_UNLOAD:
			printf("Unloading\n");
			break;
		case REMODULE_OP_BEFORE_RELOAD:
			printf("Begin reload\n");
			break;
		case REMODULE_OP_AFTER_RELOAD:
			printf("End reload\n");
			break;
	}
}
