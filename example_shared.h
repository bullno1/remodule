#ifndef EXAMPLE_SHARED_H
#define EXAMPLE_SHARED_H

typedef struct plugin_interface_s {
	// Ask the host to exit
	void(*request_exit)(void);

	// The plugin is responsible for filling these on load.
	void(*update)(void* plugin_data);
	void* plugin_data;
} plugin_interface_t;

#endif
