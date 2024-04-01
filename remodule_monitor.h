#ifndef REMODULE_MONITOR_H
#define REMODULE_MONITOR_H

/**
 * @file
 * @brief A single header addon to make reloading automatic.
 *
 * In **exactly one** source file of the host program, define `REMODULE_MONITOR_IMPLEMENTATION` before including remodule_monitor.h:
 * @snippet example_host.c Include remodule_monitor
 */

#include "remodule.h"
#include <stdbool.h>

//! A monitor handle.
typedef struct remodule_monitor_s remodule_monitor_t;

/**
 * @brief Start monitoring.
 * @param mod A module obtained from @link remodule_load @endlink.
 * @return A monitor handle.
 */
REMODULE_API remodule_monitor_t*
remodule_monitor(remodule_t* mod);

/**
 * @brief Check for reload.
 *
 * If the module was updated, it will be reloaded with @link remodule_reload @endlink.
 *
 * @param mon A monitor handle obtained from @link remodule_monitor @endlink.
 * @return Whether a reload happened.
 */
REMODULE_API bool
remodule_check(remodule_monitor_t* mon);

/**
 * @brief Check for reload.
 *
 * Return whether the module has changed.
 *
 * @param mon A monitor handle obtained from @link remodule_monitor @endlink.
 * @return Whether a reload should be made.
 */
REMODULE_API bool
remodule_should_reload(remodule_monitor_t* mon);

/**
 * @brief Stop monitoring.
 *
 * @param mon A monitor handle obtained from @link remodule_monitor @endlink.
 */
REMODULE_API void
remodule_unmonitor(remodule_monitor_t* mon);

#endif

#ifdef REMODULE_MONITOR_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#if defined(__linux__)

#include <sys/inotify.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#endif

typedef struct remodule_dirmon_link_s {
	struct remodule_dirmon_link_s* next;
	struct remodule_dirmon_link_s* prev;
} remodule_dirmon_link_t;

typedef struct remodule_dirmon_s {
	remodule_dirmon_link_t link;

	int num_monitors;
	int version;

#if defined(__linux__)
	int watchd;
#elif defined(_WIN32)
	HANDLE dir_handle;
	OVERLAPPED overlapped;
	_Alignas(FILE_NOTIFY_INFORMATION) char notification_buf[sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH];
#endif
	char path[];
} remodule_dirmon_t;

typedef struct remodule_dirmon_root_s {
	remodule_dirmon_link_t link;
	int version;

#if defined(__linux__)
	int inotifyfd;
#elif defined(_WIN32)
	HANDLE iocp;
#endif
} remodule_dirmon_root_t;

static remodule_dirmon_root_t remodule_dirmon_root = {
#if defined(__linux__)
	.inotifyfd = -1,
#elif defined(_WIN32)
	.iocp = NULL,
#endif
	.link = {
		.next = &remodule_dirmon_root.link,
		.prev = &remodule_dirmon_root.link,
	}
};

struct remodule_monitor_s {
	int dirmon_version;
	int root_version;
	remodule_dirmon_t* dirmon;
	remodule_t* mod;

#if defined(__linux__)
	struct timespec last_modified;
#elif defined(_WIN32)
	FILETIME last_modified;
#endif
};

#if defined(__linux__)

static remodule_dirmon_t*
remodule_dirmon_acquire(const char* path) {
	char name_buf[PATH_MAX];
	char* real_path = realpath(path, name_buf);
	char* dir_name = dirname(real_path);

	for (
		remodule_dirmon_link_t* itr = remodule_dirmon_root.link.next;
		itr != &remodule_dirmon_root.link;
		itr = itr->next
	) {
		remodule_dirmon_t* dirmon = (remodule_dirmon_t*)((char*)itr - offsetof(remodule_dirmon_t, link));
		if (strcmp(dir_name, dirmon->path) == 0) {
			++dirmon->num_monitors;
			return dirmon;
		}
	}

	if (remodule_dirmon_root.inotifyfd < 0) {
		remodule_dirmon_root.inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
		REMODULE_ASSERT(remodule_dirmon_root.inotifyfd > 0, "Could not create inotify");
	}

	size_t dir_name_len = strlen(dir_name);
	remodule_dirmon_t* dirmon = malloc(
		sizeof(remodule_dirmon_t) + dir_name_len + 1
	);
	*dirmon = (remodule_dirmon_t){
		.num_monitors = 1,
		.watchd = inotify_add_watch(
			remodule_dirmon_root.inotifyfd, dir_name, IN_ALL_EVENTS
		),
	};
	REMODULE_ASSERT(dirmon->watchd, "Could not add watch");

	dirmon->link.next = remodule_dirmon_root.link.next;
	remodule_dirmon_root.link.next->prev = &dirmon->link;
	dirmon->link.prev = &remodule_dirmon_root.link;
	remodule_dirmon_root.link.next = &dirmon->link;

	memcpy(dirmon->path, dir_name, dir_name_len);
	dirmon->path[dir_name_len] = '\0';

	return dirmon;
}

static void
remodule_dirmon_release(remodule_dirmon_t* dirmon) {
	if (--dirmon->num_monitors > 0) { return; }

	dirmon->link.next->prev = dirmon->link.prev;
	dirmon->link.prev->next = dirmon->link.next;
	inotify_rm_watch(remodule_dirmon_root.inotifyfd, dirmon->watchd);
	free(dirmon);

	if (remodule_dirmon_root.link.next == &remodule_dirmon_root.link) {
		close(remodule_dirmon_root.inotifyfd);
		remodule_dirmon_root.inotifyfd = -1;
	}
}

static void
remodule_dirmon_update_all(void) {
	_Alignas(struct inotify_event) char event_buf[sizeof(struct inotify_event) + NAME_MAX + 1];

	while (true) {
		ssize_t num_bytes_read = read(remodule_dirmon_root.inotifyfd, event_buf, sizeof(event_buf));

		if (num_bytes_read <= 0) {
			break;
		}

		for (
			char* event_itr = event_buf;
			event_itr - event_buf < num_bytes_read;
		) {
			struct inotify_event* event = (struct inotify_event*)event_itr;
			event_itr += sizeof(struct inotify_event) + event->len;

			for (
				remodule_dirmon_link_t* itr = remodule_dirmon_root.link.next;
				itr != &remodule_dirmon_root.link;
				itr = itr->next
			) {
				remodule_dirmon_t* dirmon = (remodule_dirmon_t*)((char*)itr - offsetof(remodule_dirmon_t, link));

				if (dirmon->watchd == event->wd) {
					++dirmon->version;
					break;
				}
			}
		}
	}

	++remodule_dirmon_root.version;
}

#elif defined(_WIN32)

static remodule_dirmon_t*
remodule_dirmon_acquire(const char* path) {
	char name_buf[MAX_PATH];
	char* file_part;
	GetFullPathNameA(path, sizeof(name_buf), name_buf, &file_part);
	*file_part = '\0';

	for (
		remodule_dirmon_link_t* itr = remodule_dirmon_root.link.next;
		itr != &remodule_dirmon_root.link;
		itr = itr->next
	) {
		remodule_dirmon_t* dirmon = (remodule_dirmon_t*)((char*)itr - offsetof(remodule_dirmon_t, link));
		if (_stricmp(name_buf, dirmon->path) == 0) {
			++dirmon->num_monitors;
			return dirmon;
		}
	}

	if (remodule_dirmon_root.iocp == NULL) {
		remodule_dirmon_root.iocp = CreateIoCompletionPort(
			INVALID_HANDLE_VALUE,
			NULL,
			0,
			1
		);
		REMODULE_ASSERT(remodule_dirmon_root.iocp != NULL, "Could not create IOCP");
	}

	size_t dir_name_len = strlen(name_buf);
	remodule_dirmon_t* dirmon = malloc(
		sizeof(remodule_dirmon_t) + dir_name_len + 1
	);
	*dirmon = (remodule_dirmon_t){
		.num_monitors = 1,
	};

	dirmon->link.next = remodule_dirmon_root.link.next;
	remodule_dirmon_root.link.next->prev = &dirmon->link;
	dirmon->link.prev = &remodule_dirmon_root.link;
	remodule_dirmon_root.link.next = &dirmon->link;

	memcpy(dirmon->path, name_buf, dir_name_len);
	dirmon->path[dir_name_len] = '\0';

	dirmon->dir_handle = CreateFileA(
		name_buf,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);
	REMODULE_ASSERT(dirmon->dir_handle != INVALID_HANDLE_VALUE, "Could not open directory");

	REMODULE_ASSERT(
		CreateIoCompletionPort(
			dirmon->dir_handle,
			remodule_dirmon_root.iocp,
			0,
			1
		) != NULL,
		"Could not associate directory to IOCP"
	);

	REMODULE_ASSERT(
		ReadDirectoryChangesW(
			dirmon->dir_handle,
			dirmon->notification_buf,
			sizeof(dirmon->notification_buf),
			FALSE,
			FILE_NOTIFY_CHANGE_FILE_NAME
			| FILE_NOTIFY_CHANGE_LAST_WRITE
			| FILE_NOTIFY_CHANGE_CREATION,
			NULL,
			&dirmon->overlapped,
			NULL
		),
		"ReadDirectoryChangesW failed"
	);

	return dirmon;
}

static void
remodule_dirmon_release(remodule_dirmon_t* dirmon) {
	if (--dirmon->num_monitors > 0) { return; }

	dirmon->link.next->prev = dirmon->link.prev;
	dirmon->link.prev->next = dirmon->link.next;
	CancelIo(dirmon->dir_handle);
	CloseHandle(dirmon->dir_handle);
	free(dirmon);

	if (remodule_dirmon_root.link.next == &remodule_dirmon_root.link) {
		CloseHandle(remodule_dirmon_root.iocp);
		remodule_dirmon_root.iocp = NULL;
	}
}

static void
remodule_dirmon_update_all(void) {
	DWORD num_bytes;
	ULONG_PTR key;
	OVERLAPPED* overlapped;

	while (GetQueuedCompletionStatus(remodule_dirmon_root.iocp, &num_bytes, &key, &overlapped, 0)) {
		for (
			remodule_dirmon_link_t* itr = remodule_dirmon_root.link.next;
			itr != &remodule_dirmon_root.link;
			itr = itr->next
		) {
			remodule_dirmon_t* dirmon = (remodule_dirmon_t*)((char*)itr - offsetof(remodule_dirmon_t, link));

			if (&dirmon->overlapped == overlapped) {
				++dirmon->version;

				// Queue another read
				REMODULE_ASSERT(
					ReadDirectoryChangesW(
						dirmon->dir_handle,
						dirmon->notification_buf,
						sizeof(dirmon->notification_buf),
						FALSE,
						FILE_NOTIFY_CHANGE_FILE_NAME
						| FILE_NOTIFY_CHANGE_LAST_WRITE
						| FILE_NOTIFY_CHANGE_CREATION,
						NULL,
						&dirmon->overlapped,
						NULL
					),
					"ReadDirectoryChangesW failed"
				);
				break;
			}
		}
	}

	++remodule_dirmon_root.version;
}

#else
#error Unsupported platform
#endif

remodule_monitor_t*
remodule_monitor(remodule_t* mod) {
	const char* path = remodule_path(mod);

	remodule_monitor_t* mon = malloc(sizeof(remodule_monitor_t));
	remodule_dirmon_t* dirmon = remodule_dirmon_acquire(path);
	*mon = (remodule_monitor_t){
		.dirmon_version = dirmon->version,
		.root_version = remodule_dirmon_root.version,
		.dirmon = dirmon,
		.mod = mod,
	};

#if defined(__linux__)
	struct stat stat_buf;
	REMODULE_ASSERT(stat(path, &stat_buf) == 0, "Could not stat file");
	mon->last_modified = stat_buf.st_mtim;
#elif defined(_WIN32)
	HANDLE file = CreateFileA(
		path,
		0,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	REMODULE_ASSERT(file != INVALID_HANDLE_VALUE, "Could not stat file");
	REMODULE_ASSERT(
		GetFileTime(file, NULL, NULL, &mon->last_modified),
		"Could not stat file"
	);
	CloseHandle(file);
#endif

	return mon;
}

bool
remodule_check(remodule_monitor_t* mon) {
	if (remodule_should_reload(mon)) {
		remodule_reload(mon->mod);
		return true;
	} else {
		return false;
	}
}

bool
remodule_should_reload(remodule_monitor_t* mon) {
	if (mon->root_version == remodule_dirmon_root.version) {
		remodule_dirmon_update_all();
	}
	mon->root_version = remodule_dirmon_root.version;

	if (mon->dirmon_version == mon->dirmon->version) {
		return false;
	}
	mon->dirmon_version = mon->dirmon->version;
	bool should_reload = false;

#if defined(__linux__)
	struct stat stat_buf;

	if (
		stat(remodule_path(mon->mod), &stat_buf) == 0
		&& (
			mon->last_modified.tv_sec < stat_buf.st_mtim.tv_sec
			|| (
				mon->last_modified.tv_sec == stat_buf.st_mtim.tv_sec
				&& mon->last_modified.tv_nsec < stat_buf.st_mtim.tv_nsec
			)
		)
	) {
		mon->last_modified = stat_buf.st_mtim;
		should_reload = true;
	}
#elif defined(_WIN32)
	HANDLE file = CreateFileA(
		remodule_path(mon->mod),
		0,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (file != INVALID_HANDLE_VALUE) {
		FILETIME last_modified;
		if (
			GetFileTime(file, NULL, NULL, &last_modified)
			&& (
				mon->last_modified.dwHighDateTime < last_modified.dwHighDateTime
				|| (
					mon->last_modified.dwHighDateTime == last_modified.dwHighDateTime
					&& mon->last_modified.dwLowDateTime < last_modified.dwLowDateTime
				)
			)
		) {
			mon->last_modified = last_modified;
			should_reload = true;
		}

		CloseHandle(file);
	}
#endif

	return should_reload;
}

void
remodule_unmonitor(remodule_monitor_t* mon) {
	remodule_dirmon_release(mon->dirmon);
	free(mon);
}

#endif
