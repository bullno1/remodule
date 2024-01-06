# re:module

re:module is a library for live reloading.
It consists of these modules:

* remodule.h: The main module.
* remodule_monitor.h: Automatic reload.

The program must be structured as follow:

* A host program with minimal code (e.g: @link example_host.c @endlink).
* One or more dynamic library as plugin (e.g: @link example_plugin.c @endlink).
* Optionally, some sort of shared interface between a program and its plugin: (e.g: @link example_shared.h @endlink).

In **exactly one** source file of the host program, define `REMODULE_HOST_IMPLEMENTATION` before including remodule.h:
@snippet example_host.c Include remodule

Likewise, in **exactly one** source file of every plugin, define `REMODULE_PLUGIN_IMPLEMENTATION` before including remodule.h:
@snippet example_plugin.c Plugin include

A plugin must define an @link remodule_entry entrypoint @endlink:
@snippet example_plugin.c Plugin entrypoint

If the plugin has any global state that needs to be preserved across reloads, mark those with @link REMODULE_VAR @endlink:
@snippet example_plugin.c State transfer

The plugin will now be loadable from the host with @link remodule_load @endlink.

Subsequenly, @link remodule_reload @endlink can be used to reload a plugin.
To make this automatic, refer to remodule_monitor.h.

When the plugin is no longer needed, unload it with @link remodule_unload @endlink.

@example example_plugin.c
@example example_host.c
@example example_shared.h
