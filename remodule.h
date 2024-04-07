#ifndef REMODULE_H
#define REMODULE_H

/**
 * @file
 * @brief A single file library for live reloading.
 *
 * In **exactly one** source file of the host program, define `REMODULE_HOST_IMPLEMENTATION` before including remodule.h:
 * @snippet example_host.c Include remodule
 *
 * Likewise, in **exactly one** source file of every plugin, define `REMODULE_PLUGIN_IMPLEMENTATION` before including remodule.h:
 * @snippet example_plugin.c Plugin include
 *
 * A plugin must define an @link remodule_entry entrypoint @endlink.
 *
 * If a plugin has any global state that needs to be preserved across reloads, mark those with @ref REMODULE_VAR.
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stddef.h>

//! @cond remodule_internal

#ifdef REMODULE_SHARED
#	if defined(_WIN32) && !defined(__MINGW32__)
#		ifdef REMODULE_HOST_IMPLEMENTATION
#			define REMODULE_API __declspec(dllexport)
#		else
#			define REMODULE_API __declspec(dllimport)
#		endif
#	else
#		ifdef REMODULE_HOST_IMPLEMENTATION
#			define REMODULE_API __attribute__((visibility("default")))
#		else
#			define REMODULE_API extern
#		endif
#	endif
#else
#	define REMODULE_API extern
#endif

//! @endcond

/**
 * @brief Mark a global variable in the plugin for state transfer.
 *
 * Example:
 * @snippet example_plugin.c State transfer
 *
 * @param TYPE The type of the variable.
 * @param NAME The name of the variable.
 *   This must be unique within each plugin.
 *
 * @remarks
 *   If the type of the variable changes between reloads, it will not be preserved.
 *   The new instance will have the variable at its initial value.
 *
 * @remarks
 *   Only a shallow copy will be made using `memcpy` to migrate data from the old
 *   plugin instance to the new one.
 *
 * @remarks
 *   As long as the plugin uses the host's allocator or its allocator's state
 *   is preserved, everything should work out
 *   of the box.
 *
 * @remarks
 *   On the other hand, pointers to static data in the plugin or structures
 *   allocated using the plugin's private allocator are problematic.
 *
 * @remarks
 *   For more complex cases, make use of @ref REMODULE_OP_BEFORE_RELOAD and
 *   @ref REMODULE_OP_AFTER_RELOAD to serialize and deserialize.
 *   The target for serialization could be the `userdata` pointer in @ref remodule_entry.
 */
#define REMODULE_VAR(TYPE, NAME) \
	extern TYPE NAME; \
	const remodule_var_info_t remodule__##NAME##_info = { \
		.name = #NAME, \
		.name_length = sizeof(#NAME) - 1, \
		.value_addr = &NAME, \
		.value_size = sizeof(NAME), \
	}; \
	REMODULE__SECTION_BEGIN \
	const remodule_var_info_t* const remodule__##NAME##_info_ptr = &remodule__##NAME##_info; \
	REMODULE__SECTION_END \
	TYPE NAME

#if defined(_MSC_VER)
#	define REMODULE__SECTION_BEGIN \
	__pragma(data_seg(push)); \
	__pragma(section("remodule$data", read)); \
	__declspec(allocate("remodule$data"))
#elif defined(__APPLE__)
#	define REMODULE__SECTION_BEGIN __attribute__((used, section("__DATA,remodule")))
#elif defined(__unix__)
#	define REMODULE__SECTION_BEGIN __attribute__((used, section("remodule")))
#else
#	error Unsupported compiler
#endif

#if defined(_MSC_VER)
#	define REMODULE__SECTION_END __pragma(data_seg(pop));
#elif defined(__APPLE__)
#	define REMODULE__SECTION_END
#elif defined(__unix__)
#	define REMODULE__SECTION_END
#endif

//! @cond remodule_internal

#ifndef REMODULE_ASSERT
#include <stdlib.h>
#include <stdio.h>

#define REMODULE_ASSERT(COND, MSG) \
	do { \
		if (!(COND)) { \
			fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, MSG, remodule_last_error()); \
			abort(); \
		} \
	} while(0)

#ifdef __cplusplus
extern "C" {
#endif

REMODULE_API const char*
remodule_last_error(void);

#ifdef __cplusplus
}
#endif

//! @endcond

#endif

#ifdef DOXYGEN
/**
 * @brief The platform-dependent file extension for dynamic library.
 */
#	define REMODULE_DYNLIB_EXT ".<dll|dylib|so>"
#endif

#if defined(_WIN32)
#	define REMODULE_DYNLIB_EXT ".dll"
#elif defined(__APPLE__)
#	define REMODULE_DYNLIB_EXT ".dylib"
#elif defined(__linux__)
#	define REMODULE_DYNLIB_EXT ".so"
#endif

//! A reloadable module
typedef struct remodule_s remodule_t;

/**
 * @brief The operation that is being executed.
 */
typedef enum remodule_op_e {
	//! The module is being loaded for the first time.
	REMODULE_OP_LOAD,
	//! The module is being unloaded.
	REMODULE_OP_UNLOAD,
	//! Before a reload, this will be observed by the **old** plugin instance.
	REMODULE_OP_BEFORE_RELOAD,
	//! After a reload, this will be observed by the **new** plugin instance.
	REMODULE_OP_AFTER_RELOAD,
} remodule_op_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load a module.
 *
 * This will trigger @ref REMODULE_OP_LOAD in the module's
 * @link remodule_entry entrypoint @endlink.
 *
 * @param path Path to the module.
 * @param userdata Arbitrary userdata that will be passed to the entrypoint of
 *   the module.
 *
 * @return The reloadable module.
 *
 * @remarks
 *   On Windows, due to file locking, instead of loading the module directly,
 *   a temporary copy will be made.
 *   This will be loaded instead of the original module.
 *   Therefore, the directory containing the module must be writable.
 * @remarks
 *   The temporary file will be deleted once it's no longer needed.
 */
REMODULE_API remodule_t*
remodule_load(const char* path, void* userdata);

/**
 * @brief Reload a module.
 *
 * This will trigger @ref REMODULE_OP_BEFORE_RELOAD and
 * @ref REMODULE_OP_AFTER_RELOAD in the module's
 * @link remodule_entry entrypoint @endlink.
 */
REMODULE_API void
remodule_reload(remodule_t* mod);

/**
 * @brief Unload a module.
 *
 * This will trigger @ref REMODULE_OP_UNLOAD in the module's
 * @link remodule_entry entrypoint @endlink.
 */
REMODULE_API void
remodule_unload(remodule_t* mod);

/**
 * @brief Get the path of a module.
 */
REMODULE_API const char*
remodule_path(remodule_t* mod);

/**
 * @brief Get the userdata associated with a module.
 *
 * This is the same pointer previously passed to @ref remodule_load.
 */
REMODULE_API void*
remodule_userdata(remodule_t* mod);

#ifdef DOXYGEN

/**
 * @brief A plugin **must** define this function.
 *
 * This will be called at various points during the plugin's lifecycle.
 *
 * @param op The operation currently being executed.
 * @param userdata The userdata passed from the host in @ref remodule_load.
 *   This always points to the same object between reloads.
 */
REMODULE_API void
remodule_entry(remodule_op_t op, void* userdata);

#ifdef __cplusplus
}
#endif

#endif

#endif

#if (defined(REMODULE_PLUGIN_IMPLEMENTATION) || defined(REMODULE_HOST_IMPLEMENTATION)) && !defined(REMODULE_INTERNAL)
#define REMODULE_INTERNAL

#define REMODULE_INFO_SYMBOL remodule__plugin_info
#define REMODULE_INFO_SYMBOL_STR REMODULE_STRINGIFY(REMODULE_INFO_SYMBOL)
#define REMODULE_STRINGIFY(X) REMODULE_STRINGIFY2(X)
#define REMODULE_STRINGIFY2(X) #X

typedef struct remodule_var_info_s {
	const char* name;
	size_t name_length;
	void* value_addr;
	size_t value_size;
} remodule_var_info_t;

typedef struct remodule_plugin_info_s {
	const remodule_var_info_t* const* var_info_begin;
	const remodule_var_info_t* const* var_info_end;
	void(*entry)(remodule_op_t op, void* userdata);
} remodule_plugin_info_t;

#endif

#ifdef REMODULE_PLUGIN_IMPLEMENTATION

#if defined(_WIN32)
#	define REMODULE_EXPORT __declspec(dllexport)
#else
#	define REMODULE_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_MSC_VER)
__pragma(section("remodule$begin", read));
__pragma(section("remodule$data", read));
__pragma(section("remodule$end", read));
__declspec(allocate("remodule$begin")) extern const remodule_var_info_t* const remodule_var_info_begin = NULL;
__declspec(allocate("remodule$end")) extern const remodule_var_info_t* const remodule_var_info_end = NULL;
#elif defined(__APPLE__)
extern const remodule_var_info_t* const __start_remodule __asm("section$start$__DATA$remodule");
extern const remodule_var_info_t* const __stop_remodule __asm("section$end$__DATA$remodule");
__attribute__((used, section("__DATA,remodule"))) const remodule_var_info_t* const remodule__dummy = NULL;
#elif defined(__unix__)
extern const remodule_var_info_t* const __start_remodule;
extern const remodule_var_info_t* const __stop_remodule;
__attribute__((used, section("remodule"))) const remodule_var_info_t* const remodule__dummy = NULL;
#endif

#if defined(_MSC_VER)
#	define REMODULE_VAR_INFO_BEGIN (&remodule_var_info_begin + 1)
#	define REMODULE_VAR_INFO_END (&remodule_var_info_end)
#elif defined(__unix__) || defined(__APPLE__)
#	define REMODULE_VAR_INFO_BEGIN (&__start_remodule)
#	define REMODULE_VAR_INFO_END (&__stop_remodule)
#endif

void
remodule_entry(remodule_op_t op, void* userdata);

REMODULE_EXPORT remodule_plugin_info_t REMODULE_INFO_SYMBOL = {
	.entry = &remodule_entry,
	.var_info_begin = REMODULE_VAR_INFO_BEGIN,
	.var_info_end = REMODULE_VAR_INFO_END,
};

#endif

#if defined(REMODULE_HOST_IMPLEMENTATION) && !defined(REMODULE_HOST_IMPLEMENTATION_GUARD)
#define REMODULE_HOST_IMPLEMENTATION_GUARD

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define REMODULE_PATH_MAX MAX_PATH

typedef HMODULE remodule_dynlib_t;

static remodule_dynlib_t
remodule_dynlib_open(const char* path) {
	char dir_buf[MAX_PATH];
	char name_buf[MAX_PATH];
	char tmp_name_buf[MAX_PATH];
	char* file_part;

	// Create a temporary file name
	GetFullPathNameA(path, sizeof(dir_buf), dir_buf, &file_part);
	size_t name_len = strlen(file_part);
	memcpy(name_buf, file_part, name_len);
	*file_part = '\0';
	GetTempFileNameA(dir_buf, name_buf, 0, tmp_name_buf);

	// Copy the file over
	REMODULE_ASSERT(CopyFileA(path, tmp_name_buf, FALSE), "Could not create temporary file");

	return LoadLibraryA(tmp_name_buf);
}

static void*
remodule_dynlib_find(remodule_dynlib_t lib, const char* name) {
	return (void*)GetProcAddress(lib, name);
}

static void
remodule_dynlib_close(remodule_dynlib_t lib) {
	char name_buf[MAX_PATH];
	REMODULE_ASSERT(GetModuleFileNameA(lib, name_buf, sizeof(name_buf)) > 0, "Could not get module name");

	FreeLibrary(lib);
	DeleteFileA(name_buf);
}

static char*
remodule_dynlib_get_path(remodule_dynlib_t lib) {
	char path_buf[MAX_PATH];
	size_t size;
	REMODULE_ASSERT(
		(size = GetModuleFileNameA(lib, path_buf, sizeof(path_buf))) > 0,
		"Could not get module name"
	);

	char* path = malloc(size + 1);
	memcpy(path, path_buf, size);
	path[size] = '\0';

	return path;
}

static void
remodule_dynlib_free_path(char* path) {
	free(path);
}

static char remodule_error_msg_buf[2048];

const char*
remodule_last_error(void) {
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		remodule_error_msg_buf,
		sizeof(remodule_error_msg_buf),
		NULL
	);
	return remodule_error_msg_buf;
}

#elif defined(__unix__) || defined(__APPLE__)

#include <dlfcn.h>
#include <errno.h>
#include <link.h>

#define REMODULE_PATH_MAX PATH_MAX

typedef void* remodule_dynlib_t;

static remodule_dynlib_t
remodule_dynlib_open(const char* path) {
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void*
remodule_dynlib_find(remodule_dynlib_t lib, const char* name) {
	return dlsym(lib, name);
}

static void
remodule_dynlib_close(remodule_dynlib_t lib) {
	dlclose(lib);
}

static char*
remodule_dynlib_get_path(remodule_dynlib_t lib) {
	struct link_map* link_map;
	REMODULE_ASSERT(
		dlinfo(lib, RTLD_DI_LINKMAP, &link_map) == 0,
		"Could not read library info"
	);

	size_t size = strlen(link_map->l_name) + 1;
	char* path = malloc(size);
	memcpy(path, link_map->l_name, size);

	return path;
}

static void
remodule_dynlib_free_path(char* path) {
	free(path);
}

const char*
remodule_last_error(void) {
	return errno ? strerror(errno) : dlerror();
}

#endif

typedef struct remodule_tmp_var_storage_s {
	char* name;
	void* value;
	size_t name_length;
	size_t value_size;
} remodule_tmp_var_storage_t;

struct remodule_s {
	void* userdata;
	remodule_plugin_info_t info;
	remodule_dynlib_t lib;
	char* path;
};

remodule_t*
remodule_load(const char* path, void* userdata) {
	remodule_dynlib_t lib = remodule_dynlib_open(path);
	REMODULE_ASSERT(lib != NULL, "Could not load library");

	remodule_plugin_info_t* info = remodule_dynlib_find(lib, REMODULE_INFO_SYMBOL_STR);
	REMODULE_ASSERT(info != NULL, "Module does not export info struct");

	info->entry(REMODULE_OP_LOAD, userdata);

	remodule_t* mod = malloc(sizeof(remodule_t));
	*mod = (remodule_t){
		.userdata = userdata,
		.path = remodule_dynlib_get_path(lib),
		.info = *info,
		.lib = lib,
	};
	return mod;
}

void
remodule_reload(remodule_t* mod) {
	mod->info.entry(REMODULE_OP_BEFORE_RELOAD, mod->userdata);

	// Store all static vars in a host-allocated buffer
	int num_vars = 0;
	size_t val_buffer_size = 0;
	size_t name_buffer_size = 0;
	for (
		const remodule_var_info_t* const* itr = mod->info.var_info_begin;
		itr != mod->info.var_info_end;
		++itr
	) {
		if (*itr == NULL) { continue; }
		remodule_var_info_t var_info = **itr;

		++num_vars;
		val_buffer_size += var_info.value_size;
		name_buffer_size += var_info.name_length;
	}

	void* tmp_buf = malloc(
		num_vars * sizeof(remodule_tmp_var_storage_t)
		+ val_buffer_size
		+ name_buffer_size
	);
	remodule_tmp_var_storage_t* entry_ptr = tmp_buf;
	char* data_ptr = (char*)(entry_ptr + num_vars);

	for (
		const remodule_var_info_t* const* itr = mod->info.var_info_begin;
		itr != mod->info.var_info_end;
		++itr
	) {
		if (*itr == NULL) { continue; }
		remodule_var_info_t var_info = **itr;

		remodule_tmp_var_storage_t* entry = entry_ptr++;

		entry->name = data_ptr;
		entry->name_length = var_info.name_length;
		data_ptr += var_info.name_length;

		entry->value = data_ptr;
		entry->value_size = var_info.value_size;
		data_ptr += var_info.value_size;

		memcpy(entry->name, var_info.name, var_info.name_length);
		memcpy(entry->value, var_info.value_addr, var_info.value_size);
	}

	remodule_dynlib_close(mod->lib);
	mod->lib = remodule_dynlib_open(mod->path);
	REMODULE_ASSERT(mod->lib != NULL, "Failed to reload");

	remodule_plugin_info_t* info = remodule_dynlib_find(mod->lib, REMODULE_INFO_SYMBOL_STR);
	REMODULE_ASSERT(info != NULL, "Module does not export info struct");
	mod->info = *info;

	// Copy vars back in
	remodule_tmp_var_storage_t* tmp_storage = (remodule_tmp_var_storage_t*)tmp_buf;
	for (
		const remodule_var_info_t* const* var_itr = mod->info.var_info_begin;
		var_itr != mod->info.var_info_end;
		++var_itr
	) {
		if (*var_itr == NULL) { continue; }
		remodule_var_info_t var_info = **var_itr;

		for (
			int storage_index = 0; storage_index < num_vars; ++storage_index
		) {
			remodule_tmp_var_storage_t* storage = &tmp_storage[storage_index];
			if (
				storage->name_length == var_info.name_length
				&& storage->value_size == var_info.value_size
				&& memcmp(storage->name, var_info.name, storage->name_length) == 0
			) {
				memcpy(var_info.value_addr, storage->value, storage->value_size);
				break;
			}
		}
	}
	free(tmp_buf);

	mod->info.entry(REMODULE_OP_AFTER_RELOAD, mod->userdata);
}

void
remodule_unload(remodule_t* mod) {
	mod->info.entry(REMODULE_OP_UNLOAD, mod->userdata);
	remodule_dynlib_free_path(mod->path);
	remodule_dynlib_close(mod->lib);
	free(mod);
}

const char*
remodule_path(remodule_t* mod) {
	return mod->path;
}

void*
remodule_userdata(remodule_t* mod) {
	return mod->userdata;
}

#endif
