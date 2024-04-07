//! [Include remodule_monitor]
#define REMODULE_MONITOR_IMPLEMENTATION
#include "remodule_monitor.h"
//! [Include remodule_monitor]

//! [Include remodule]
#define REMODULE_HOST_IMPLEMENTATION
#include "remodule.h"
//! [Include remodule]

#include <stdio.h>
#include "example_shared.h"

static bool should_run = true;

static void
request_exit(void) {
	should_run = false;
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
	remodule_monitor_t* mon = remodule_monitor(mod);

	while (should_run) {
		interface.update(interface.plugin_data);

		if (remodule_check(mon)) {
			fprintf(stderr, "Reloaded %s\n", remodule_path(mod));
		}
	}

	remodule_unmonitor(mon);
	remodule_unload(mod);

	return 0;
}
