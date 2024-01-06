# re:module

re:module is a library for live reloading.

# Usage

Copy [remodule.h](remodule.h) into your project.
A project using re:module must be structured as follow:

* A host program with minimal code (e.g: [example_host.c](example_host.c)).
  This will not be reloadable.
* One or more dynamic library as plugin (e.g: [example_plugin.c](example_plugin.c)).
  This is where the bulk of the behaviour should be.
* Optionally, some sort of shared interface between a program and its plugin: [example_shared.h](example_shared.h).

In **exactly one** source file of the host program, define `REMODULE_HOST_IMPLEMENTATION` before including remodule.h:

```c
#define REMODULE_HOST_IMPLEMENTATION
#include "remodule.h"
```

Likewise, in **exactly one** source file of every plugin, define `REMODULE_PLUGIN_IMPLEMENTATION` before including remodule.h:

```c
#define REMODULE_PLUGIN_IMPLEMENTATION
#include "remodule.h"
```

Additionally, a plugin must define an entrypoint:

```c
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
```

If a plugin has any global state that needs to be preserved across reloads, mark those with `REMODULE_VAR`:

```c
// This variable will be preserved across reloads.
REMODULE_VAR(int, counter) = 0;
```

The plugin will now be loadable from the host with `remodule_load`:

```c
// This will be passed verbatim to the plugin
plugin_interface_t interface = {
    // Something the plugin can call to communicate with the host
    .request_exit = request_exit,
};
remodule_t* mod = remodule_load("./plugin" REMODULE_DYNLIB_EXT, &interface);
```

Subsequenly, `remodule_reload` can be used to reload a plugin.
To make this automatic, refer to [remodule_monitor.h](remodule_monitor.h).

When the plugin is no longer needed, unload it with `remodule_unload`.

# Example
## On Linux

Run `./build`.
Then start `./host` and try entering one of these commands: `up`, `down`, `show` or `exit`.

At any point in time, you can modify [example_plugin.c](example_plugin.c), rebuild it with `./build` and the next command will be served by a new plugin instance.

## On Windows

[premake](https://premake.github.io/) is needed to generate a MSVC solution: `premake vs2022`.

The workflow is similar to Linux.
However, VS 2022 seems to disallow building while the debugger is attached.

# Documentation

Use [doxygen](https://doxygen.nl) to generate the documentation.

An online version can be found at https://bullno1.github.io/remodule.
