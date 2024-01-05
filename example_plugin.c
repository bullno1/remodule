#define REMODULE_PLUGIN_IMPLEMENTATION
#include <stdio.h>
#include "remodule.h"
#include "example_shared.h"

REMODULE_VAR(int, counter) = 0;

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

void
remodule_entry(remodule_op_t op, void* userdata) {
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
