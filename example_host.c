//! [Include remodule_monitor]
#define REMODULE_MONITOR_IMPLEMENTATION
#include "remodule_monitor.h"
//! [Include remodule_monitor]

//! [Include remodule]
#define REMODULE_HOST_IMPLEMENTATION
#include "remodule.h"
//! [Include remodule]

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "example_shared.h"

int
main(int argc, const char* argv[]) {
	//! [Load plugin]
	plugin_interface interface;  // This will be passed verbatim to the plugin
	remodule_t* mod = remodule_load("./plugin" REMODULE_DYNLIB_EXT, &interface);
	//! [Load plugin]
	remodule_monitor_t* mon = remodule_monitor(mod);

	char line[1024];
	while (true) {
		if (fgets(line, sizeof(line), stdin) == NULL) { break; }

		if (remodule_check(mon)) {
			fprintf(stderr, "Reloaded %s\n", remodule_path(mod));
		}

		if (strcmp(line, "up\n") == 0) {
			interface.up();
		} else if (strcmp(line, "down\n") == 0) {
			interface.down();
		} else if (strcmp(line, "show\n") == 0) {
			interface.show();
		} else if (strcmp(line, "reload\n") == 0) {
			remodule_reload(mod);
		} else {
			fprintf(stderr, "Invalid command: %s", line);
		}
	}

	remodule_unmonitor(mon);
	remodule_unload(mod);

	return 0;
}
