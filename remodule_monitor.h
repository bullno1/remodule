#ifndef REMODULE_MONITOR_H
#define REMODULE_MONITOR_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "remodule.h"

typedef struct remodule_monitor_s remodule_monitor_t;

remodule_monitor_t*
remodule_monitor(remodule_t* mod);

void
remodule_check(remodule_monitor_t* mon);

void
remodule_unmonitor(remodule_monitor_t* mon);

#endif

#ifdef REMODULE_MONITOR_IMPLEMENTATION

#include <stdbool.h>

typedef struct remodule_dirmon_link_s {
	struct remodule_dirmon_link_s* next;
	struct remodule_dirmon_link_s* prev;
} remodule_dirmon_link_t;

typedef struct remodule_dirmon_s {
	remodule_dirmon_link_t link;

	int num_monitors;
	int version;

#ifdef __linux__
	int watchd;
	char path[];
#endif
} remodule_dirmon_t;

typedef struct remodule_dirmon_root_s {
	remodule_dirmon_link_t link;
	int version;

#if defined(__linux__)
	int inotifyfd;
#endif
} remodule_dirmon_root_t;

static remodule_dirmon_root_t remodule_dirmon_root = {
#if defined(__linux__)
	.inotifyfd = -1,
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
#endif
};

#ifdef __linux__

#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>

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
	char _Alignas(struct inotify_event) event_buf[sizeof(struct inotify_event) + NAME_MAX + 1];
	bool has_update = false;

	while (true) {
		ssize_t num_bytes_read = read(remodule_dirmon_root.inotifyfd, event_buf, sizeof(event_buf));

		if (num_bytes_read <= 0) {
			break;
		}

		has_update = true;

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

	if (has_update) {
		++remodule_dirmon_root.version;
	}
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
	stat(path, &stat_buf);
	mon->last_modified = stat_buf.st_mtim;
#endif

	return mon;
}

void
remodule_check(remodule_monitor_t* mon) {
	if (mon->root_version == remodule_dirmon_root.version) {
		remodule_dirmon_update_all();
	}
	mon->root_version = remodule_dirmon_root.version;

	if (mon->dirmon_version == mon->dirmon->version) {
		return;
	}
	mon->dirmon_version = mon->dirmon->version;

#if defined(__linux__)
	struct stat stat_buf;
	stat(remodule_path(mon->mod), &stat_buf);

	if (
		mon->last_modified.tv_sec < stat_buf.st_mtim.tv_sec
		|| mon->last_modified.tv_nsec < stat_buf.st_mtim.tv_nsec
	) {
		mon->last_modified = stat_buf.st_mtim;
		remodule_reload(mon->mod);
	}
#endif
}

void
remodule_unmonitor(remodule_monitor_t* mon) {
	remodule_dirmon_release(mon->dirmon);
	free(mon);
}

#endif
