#ifndef EXAMPLE_SHARED_H
#define EXAMPLE_SHARED_H

// A command callback
typedef void(*cmd)(void);

// The plugin is responsible for filling these on load.
typedef struct {
	cmd up;
	cmd down;
	cmd show;
} plugin_interface;

#endif
