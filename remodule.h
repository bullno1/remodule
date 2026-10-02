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
 * @brief Declare a variable in the plugin that is eligible for state transfer.
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
	REMODULE_PERSIST_VAR(NAME) \
	TYPE NAME

/**
 * @brief Mark an existing variable for state transfer.
 *
 * This can be used for global variables in 3rd party libraries without
 * modifying its source.
 *
 * @param NAME variable name
 *
 * @see REMODULE_VAR
 */
#define REMODULE_PERSIST_VAR(NAME) \
	REMODULE_PERSIST_VAR_EX(NAME,)

/**
 * @brief Mark an existing variable for state transfer.
 *
 * This can be used for global variables in 3rd party libraries without
 * modifying its source.
 *
 * The @a NAMESPACE argument can be used so that **static** variables in different
 * translation units can have the same name.
 *
 * @param NAME variable name
 * @param NAMESPACE A namespace to put the metadata into
 *
 * @see REMODULE_VAR_EX
 */
#define REMODULE_PERSIST_VAR_EX(NAME, NAMESPACE) \
	const remodule_var_info_t REMODULE__META_NAME(NAME, NAMESPACE) = { \
		.name = #NAMESPACE "_" #NAME, \
		.name_length = sizeof(#NAMESPACE "_" #NAME) - 1, \
		.value_addr = &NAME, \
		.value_size = sizeof(NAME), \
	}; \
	REMODULE__SECTION_BEGIN \
	const remodule_var_info_t* const REMODULE__META_PTR_NAME(NAME, NAMESPACE) = &REMODULE__META_NAME(NAME, NAMESPACE); \
	REMODULE__SECTION_END(REMODULE__META_PTR_NAME(NAME, NAMESPACE))

//! @cond remodule_internal

#define REMODULE__META_NAME(NAME, NAMESPACE) remodule__##NAMESPACE##_##NAME##_info
#define REMODULE__META_PTR_NAME(NAME, NAMESPACE) remodule__##NAMESPACE##_##NAME##_info_ptr
#define REMODULE_STRINGIFY(X) REMODULE_STRINGIFY2(X)
#define REMODULE_STRINGIFY2(X) #X

#if defined(_MSC_VER)
// In C++, a const variable is internal and can not be found by /INCLUDE
#	ifdef __cplusplus
#		define REMODULE__SLOT_LINKAGE extern "C"
#	else
#		define REMODULE__SLOT_LINKAGE
#	endif
#	define REMODULE__SECTION_BEGIN \
	__pragma(data_seg(push)); \
	__pragma(section("remodule$data", read)); \
	REMODULE__SLOT_LINKAGE __declspec(allocate("remodule$data"))
#elif defined(__APPLE__)
#	define REMODULE__SECTION_BEGIN __attribute__((retain, used, section("__DATA,remodule")))
#elif defined(__unix__)
#	define REMODULE__SECTION_BEGIN __attribute__((retain, used, section("remodule")))
#else
#	error Unsupported compiler
#endif

#if defined(_MSC_VER)
// 32-bit x86 is the only target where C symbols are decorated
#	if defined(_M_IX86)
#		define REMODULE__SYMBOL_PREFIX "_"
#	else
#		define REMODULE__SYMBOL_PREFIX ""
#	endif
#	define REMODULE__SECTION_END(INFO_PTR) \
	__pragma(data_seg(pop)); \
	__pragma(comment(linker, "/INCLUDE:" REMODULE__SYMBOL_PREFIX REMODULE_STRINGIFY(INFO_PTR)));
#elif defined(__APPLE__)
#	define REMODULE__SECTION_END(INFO_PTR)
#elif defined(__unix__)
#	define REMODULE__SECTION_END(INFO_PTR)
#endif

typedef struct remodule_var_info_s {
	const char* name;
	size_t name_length;
	void* value_addr;
	size_t value_size;
} remodule_var_info_t;

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
 * @remarks
 *   If the module has a PDB, a copy of that will also be made next to
 *   the original.
 *   It is named by writing a number over the end of the original name (e.g:
 *   `plugin.pd0` for `plugin.pdb`).
 *   The temporary module is patched to link to this copy.
 *   This leaves the original PDB free to be rewritten by the linker while a
 *   debugger is attached.
 *   The copy is deleted when the module is unloaded or reloaded.
 *   However, this will fail if a debugger is still locking the file.
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

#endif

#ifdef __cplusplus
}
#endif

#endif

#if (defined(REMODULE_PLUGIN_IMPLEMENTATION) || defined(REMODULE_HOST_IMPLEMENTATION)) && !defined(REMODULE_INTERNAL)
#define REMODULE_INTERNAL

#define REMODULE_INFO_SYMBOL remodule__plugin_info
#define REMODULE_INFO_SYMBOL_STR REMODULE_STRINGIFY(REMODULE_INFO_SYMBOL)

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
__attribute__((retain, used, section("__DATA,remodule"))) const remodule_var_info_t* const remodule__dummy = NULL;
#elif defined(__unix__)
extern const remodule_var_info_t* const __start_remodule;
extern const remodule_var_info_t* const __stop_remodule;
__attribute__((retain, used, section("remodule"))) const remodule_var_info_t* const remodule__dummy = NULL;
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
#include <stdbool.h>
#include <stdio.h>

#define REMODULE_PATH_MAX MAX_PATH

typedef struct remodule_dynlib_info_s {
	HMODULE handle;
	char pdb_path[MAX_PATH];
	char watch_path[];
} remodule_dynlib_info_t;

typedef remodule_dynlib_info_t* remodule_dynlib_t;

// The CodeView record which refers to a PDB
typedef struct remodule_pdb_ref_s {
	DWORD signature;
	GUID guid;
	DWORD age;
	char path[];
} remodule_pdb_ref_t;

static void*
remodule_image_at(char* image, size_t image_size, size_t offset, size_t size) {
	if (offset > image_size || size > image_size - offset) { return NULL; }
	return image + offset;
}

static const char*
remodule_file_part(const char* path) {
	const char* file_part = path;
	for (const char* itr = path; *itr != '\0'; ++itr) {
		if (*itr == '\\' || *itr == '/' || *itr == ':') { file_part = itr + 1; }
	}
	return file_part;
}

static char*
remodule_find_pdb_ref(char* image, size_t image_size, size_t* capacity) {
	IMAGE_DOS_HEADER* dos_header = remodule_image_at(image, image_size, 0, sizeof(IMAGE_DOS_HEADER));
	if (
		dos_header == NULL
		|| dos_header->e_magic != IMAGE_DOS_SIGNATURE
		|| dos_header->e_lfanew < 0
	) {
		return NULL;
	}

	// The module is loaded into this process so it has the same header layout
	size_t nt_headers_offset = (size_t)dos_header->e_lfanew;
	IMAGE_NT_HEADERS* nt_headers = remodule_image_at(image, image_size, nt_headers_offset, sizeof(IMAGE_NT_HEADERS));
	if (
		nt_headers == NULL
		|| nt_headers->Signature != IMAGE_NT_SIGNATURE
		|| nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC
		|| nt_headers->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG
	) {
		return NULL;
	}

	// The debug directory is located by its virtual address
	IMAGE_DATA_DIRECTORY debug_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	size_t num_sections = nt_headers->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER* sections = remodule_image_at(
		image, image_size,
		nt_headers_offset
			+ offsetof(IMAGE_NT_HEADERS, OptionalHeader)
			+ nt_headers->FileHeader.SizeOfOptionalHeader,
		num_sections * sizeof(IMAGE_SECTION_HEADER)
	);
	if (sections == NULL) { return NULL; }

	IMAGE_DEBUG_DIRECTORY* entries = NULL;
	for (size_t i = 0; i < num_sections; ++i) {
		if (
			debug_dir.VirtualAddress >= sections[i].VirtualAddress
			&& debug_dir.VirtualAddress - sections[i].VirtualAddress < sections[i].SizeOfRawData
		) {
			entries = remodule_image_at(
				image, image_size,
				(size_t)sections[i].PointerToRawData + (debug_dir.VirtualAddress - sections[i].VirtualAddress),
				debug_dir.Size
			);
			break;
		}
	}
	if (entries == NULL) { return NULL; }

	size_t num_entries = debug_dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
	for (size_t i = 0; i < num_entries; ++i) {
		if (
			entries[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW
			|| entries[i].SizeOfData <= sizeof(remodule_pdb_ref_t)
		) {
			continue;
		}

		remodule_pdb_ref_t* ref = remodule_image_at(image, image_size, entries[i].PointerToRawData, entries[i].SizeOfData);
		size_t path_capacity = entries[i].SizeOfData - sizeof(remodule_pdb_ref_t);
		if (
			ref != NULL
			&& ref->signature == 0x53445352 // RSDS
			&& memchr(ref->path, '\0', path_capacity) != NULL
		) {
			*capacity = path_capacity;
			return ref->path;
		}
	}

	return NULL;
}

// A debugger locks the PDB of a loaded module, even after the module is
// unloaded, so the linker can not write to it.
// Give the temporary module its own copy of the PDB and leave the original
// alone.
// When this is not possible, the module is left as is.
static void
remodule_dynlib_redirect_pdb(const char* module_path, const char* dir, char* pdb_path) {
	pdb_path[0] = '\0';

	HANDLE file = CreateFileA(
		module_path,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (file == INVALID_HANDLE_VALUE) { return; }

	LARGE_INTEGER file_size = { 0 };
	GetFileSizeEx(file, &file_size);
	HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
	char* image = mapping != NULL ? MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0) : NULL;

	size_t ref_capacity = 0;
	char* ref = image != NULL
		? remodule_find_pdb_ref(image, (size_t)file_size.QuadPart, &ref_capacity)
		: NULL;
	if (ref != NULL) {
		// The PDB might have been moved along with the module
		char moved_path[MAX_PATH];
		const char* original_path = ref;
		if (
			GetFileAttributesA(ref) == INVALID_FILE_ATTRIBUTES
			&& snprintf(moved_path, sizeof(moved_path), "%s%s", dir, remodule_file_part(ref)) < (int)sizeof(moved_path)
		) {
			original_path = moved_path;
		}

		// The copy is made next to the original.
		// Its name is made by writing a number over the end of the original
		// name, extension included.
		// This keeps the length so the new path always fits in the old record.
		size_t original_path_len = strlen(original_path);
		size_t name_len = strlen(remodule_file_part(original_path));
		int max_copies = 1;
		for (size_t i = 0; i < name_len && i < 3; ++i) { max_copies *= 10; }

		// The copies made for the previous loads might still be held by a
		// debugger so every load needs a new name
		bool copied = false;
		for (int i = 0; i < max_copies && name_len > 0 && original_path_len < MAX_PATH; ++i) {
			memcpy(pdb_path, original_path, original_path_len + 1);
			char* digit = pdb_path + original_path_len;
			int number = i;
			do {
				*(--digit) = (char)('0' + number % 10);
				number /= 10;
			} while (number > 0);

			copied = CopyFileA(original_path, pdb_path, TRUE) != FALSE;
			if (copied) { break; }

			DWORD error = GetLastError();
			if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) { break; }
		}

		// When the PDB was moved, the new path might not fit in the old record.
		// A debugger also looks for the PDB next to the module so the file
		// name alone is enough.
		const char* new_ref = pdb_path;
		if (strlen(new_ref) >= ref_capacity) {
			new_ref = remodule_file_part(pdb_path);
		}

		if (copied && strlen(new_ref) < ref_capacity) {
			size_t new_ref_len = strlen(new_ref);
			memcpy(ref, new_ref, new_ref_len);
			memset(ref + new_ref_len, 0, ref_capacity - new_ref_len);
		} else {
			if (copied) { DeleteFileA(pdb_path); }
			pdb_path[0] = '\0';
		}
	}

	if (image != NULL) { UnmapViewOfFile(image); }
	if (mapping != NULL) { CloseHandle(mapping); }
	CloseHandle(file);
}

static remodule_dynlib_t
remodule_dynlib_open(const char* path) {
	// Load once to find the real absolute path
	HMODULE module = LoadLibraryExA(
		path,
		NULL,
		DONT_RESOLVE_DLL_REFERENCES
	);
	if (module == NULL) { return NULL; }

	char watch_path[MAX_PATH];
	DWORD watch_path_len = GetModuleFileNameA(module, watch_path, sizeof(watch_path));
	REMODULE_ASSERT(watch_path_len > 0, "Could not get module name");
	FreeLibrary(module);

	// Create a temporary file name
	char dir_buf[MAX_PATH];
	char name_buf[MAX_PATH];
	char tmp_name_buf[MAX_PATH];
	char* file_part;

	GetFullPathNameA(watch_path, sizeof(dir_buf), dir_buf, &file_part);
	size_t name_len = strlen(file_part);
	memcpy(name_buf, file_part, name_len);
	*file_part = '\0';
	GetTempFileNameA(dir_buf, name_buf, 0, tmp_name_buf);

	// Copy the file over
	REMODULE_ASSERT(CopyFileA(watch_path, tmp_name_buf, FALSE), "Could not create temporary file");

	char pdb_path[MAX_PATH];
	remodule_dynlib_redirect_pdb(tmp_name_buf, dir_buf, pdb_path);

	// Load the temporary file
	module = LoadLibraryA(tmp_name_buf);
	if (module == NULL) {
		DWORD error = GetLastError();
		DeleteFileA(tmp_name_buf);
		if (pdb_path[0] != '\0') { DeleteFileA(pdb_path); }
		SetLastError(error);
		return NULL;
	}

	remodule_dynlib_t lib = malloc(sizeof(remodule_dynlib_info_t) + watch_path_len + 1);
	lib->handle = module;
	memcpy(lib->pdb_path, pdb_path, sizeof(pdb_path));
	memcpy(lib->watch_path, watch_path, watch_path_len);
	lib->watch_path[watch_path_len] = '\0';
	return lib;
}

static void*
remodule_dynlib_find(remodule_dynlib_t lib, const char* name) {
	return (void*)GetProcAddress(lib->handle, name);
}

static void
remodule_dynlib_close(remodule_dynlib_t lib) {
	char name_buf[MAX_PATH];
	REMODULE_ASSERT(GetModuleFileNameA(lib->handle, name_buf, sizeof(name_buf)) > 0, "Could not get module name");

	FreeLibrary(lib->handle);
	DeleteFileA(name_buf);
	// This fails when a debugger is still locking the file
	if (lib->pdb_path[0] != '\0') { DeleteFileA(lib->pdb_path); }
	free(lib);
}

static char*
remodule_dynlib_get_path(remodule_dynlib_t lib) {
	return _strdup(lib->watch_path);
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
#if !defined(__APPLE__)
#include <link.h>
#endif

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
#if defined(__APPLE__)
	// There is no dlinfo on macOS.
	// Find the library through a symbol that every plugin exports instead.
	Dl_info lib_info;
	void* symbol = dlsym(lib, REMODULE_INFO_SYMBOL_STR);
	REMODULE_ASSERT(
		symbol != NULL && dladdr(symbol, &lib_info) != 0,
		"Could not read library info"
	);
	const char* lib_path = lib_info.dli_fname;
#else
	struct link_map* link_map;
	REMODULE_ASSERT(
		dlinfo(lib, RTLD_DI_LINKMAP, &link_map) == 0,
		"Could not read library info"
	);
	const char* lib_path = link_map->l_name;
#endif

	size_t size = strlen(lib_path) + 1;
	char* path = malloc(size);
	memcpy(path, lib_path, size);

	return path;
}

static void
remodule_dynlib_free_path(char* path) {
	free(path);
}

const char*
remodule_last_error(void) {
	const char* dlerror_str = dlerror();
	return dlerror_str != NULL ? dlerror_str : strerror(errno);
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
