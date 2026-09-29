# re:module

![Build status](https://github.com/bullno1/remodule/actions/workflows/build.yml/badge.svg)

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
To make this automatic, use [bresmon](https://github.com/bullno1/libs/blob/master/bresmon.h).

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
Build the plugin from outside of the IDE instead (e.g: using `msbuild`).

A debugger locks the PDB of every module it has seen, which would prevent the linker from writing a new one.
To avoid this, re:module makes a temporary copy of the DLL and patch it to refer to a temporary copy of the PDB (e.g: `plugin.pd0` for `plugin.pdb`).
The copy is deleted when the plugin is unloaded or reloaded.
A debugger might still be holding the file at that point so some copies can be left behind.
They are safe to delete once the debugger is detached.

# Documentation

Use [doxygen](https://doxygen.nl) to generate the documentation.

An online version can be found at https://bullno1.github.io/remodule.

# Separate debug info

When using [mold](https://github.com/rui314/mold/tree/main) with [`--separate-debug-file`](https://github.com/rui314/mold/blob/main/docs/mold.md#:~:text=%2D%2Dseparate%2Ddebug%2Dfile), there is a race condition.
A change monitor such as [bresmon](https://github.com/bullno1/libs/blob/master/bresmon.h) usually only waits for the completion of the module file (i.e: the `.so` file).
The debug info file might still be in the process of being written and thus, incomplete.
A reload would trigger the debugger to load the debug info immediately.
Sometimes, esp in a large build with a lot of symbols, this would mean either rejection or corrupt debug info.

The script [wait-debug-info.py](wait-debug-info.py) can be loaded into either gdb or lldb to make the debugger wait for the debug file to be complete.
The instruction can be found at the beginning of the script.
