#ifndef EXAMPLE_SHARED_H
#define EXAMPLE_SHARED_H

typedef void(*cmd)(void);

typedef struct {
	cmd up;
	cmd down;
	cmd show;
} plugin_interface;

#endif
