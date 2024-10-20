//! [Include remodule]
#define REMODULE_HOST_IMPLEMENTATION
#include "remodule.h"
//! [Include remodule]

#include <stdio.h>
#include "example_shared.h"
#define BRESMON_IMPLEMENTATION
#include <bresmon.h>

static bool should_run = true;

static void
request_exit(void) {
	should_run = false;
}

static void
reload_module(const char* path, void* mod) {
	remodule_reload(mod);
}

int
main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	//! [Load plugin]
	// This will be passed verbatim to the plugin
	plugin_interface_t interface = {
		// Something the plugin can call to communicate with the host
		.request_exit = request_exit,
	};
	remodule_t* mod = remodule_load("plugin" REMODULE_DYNLIB_EXT, &interface);
	//! [Load plugin]

	// Autmoatic reload
	bresmon_t* mon = bresmon_create(NULL);
	bresmon_watch(mon, remodule_path(mod), reload_module, mod);

	while (should_run) {
		interface.update(interface.plugin_data);

		if (bresmon_check(mon, false)) {
			fprintf(stderr, "Reloaded %s\n", remodule_path(mod));
		}
	}

	bresmon_destroy(mon);
	remodule_unload(mod);

	return 0;
}
