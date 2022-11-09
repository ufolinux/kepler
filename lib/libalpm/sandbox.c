/*
 *  sandbox.c
 *
 *  Copyright (c) 2021-2022 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "alpm.h"
#include "log.h"
#include "sandbox.h"
#include "util.h"

static int switch_to_user(const char *user)
{
	struct passwd const *pw = NULL;
	ASSERT(user != NULL, return EINVAL);
	ASSERT(getuid() == 0, return EPERM);
	ASSERT((pw = getpwnam(user)), return errno);
	ASSERT(setgid(pw->pw_gid) == 0, return errno);
	ASSERT(setgroups(0, NULL) == 0, return errno);
	ASSERT(setuid(pw->pw_uid) == 0, return errno);
	return 0;
}

int SYMEXPORT alpm_sandbox_child(const char* sandboxuser)
{
	ASSERT(sandboxuser != NULL, return 1);
	return switch_to_user(sandboxuser);
}


void _alpm_sandbox_cb_log(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args)
{
	_alpm_sandbox_callback_t type = ALPM_SANDBOX_CB_LOG;
	_alpm_sandbox_callback_context *context = ctx;
	char *string = NULL;
	int string_size = 0;

	if(!context || context->callback_pipe == -1) {
		return;
	}

	string_size = vasprintf(&string, fmt, args);
	if(string != NULL) {
		write(context->callback_pipe, &type, sizeof(type));
		write(context->callback_pipe, &level, sizeof(level));
		write(context->callback_pipe, &string_size, sizeof(string_size));
		write(context->callback_pipe, string, string_size);
		FREE(string);
	}
}

void _alpm_sandbox_cb_dl(void *ctx, const char *filename, alpm_download_event_type_t event, void *data)
{
	_alpm_sandbox_callback_t type = ALPM_SANDBOX_CB_DOWNLOAD;
	_alpm_sandbox_callback_context *context = ctx;
	size_t filename_len;

	if(!context || context->callback_pipe == -1) {
		return;
	}

	if(!filename ||
		(event != ALPM_DOWNLOAD_INIT && event != ALPM_DOWNLOAD_PROGRESS &&
		event != ALPM_DOWNLOAD_RETRY && event != ALPM_DOWNLOAD_COMPLETED)) {
			return;
	}

	filename_len = strlen(filename);

	write(context->callback_pipe, &type, sizeof(type));
	write(context->callback_pipe, &event, sizeof(event));
	switch(event) {
		case ALPM_DOWNLOAD_INIT:
			write(context->callback_pipe, data, sizeof(alpm_download_event_init_t));
			break;
		case ALPM_DOWNLOAD_PROGRESS:
			write(context->callback_pipe, data, sizeof(alpm_download_event_progress_t));
			break;
		case ALPM_DOWNLOAD_RETRY:
			write(context->callback_pipe, data, sizeof(alpm_download_event_retry_t));
			break;
		case ALPM_DOWNLOAD_COMPLETED:
			write(context->callback_pipe, data, sizeof(alpm_download_event_completed_t));
			break;
	}
	write(context->callback_pipe, &filename_len, sizeof(filename_len));
	write(context->callback_pipe, filename, filename_len);
}


bool _alpm_sandbox_process_cb_log(alpm_handle_t *handle, int callback_pipe) {
	alpm_loglevel_t level;
	char *string = NULL;
	int string_size = 0;
	ssize_t got;

	got = read(callback_pipe, &level, sizeof(level));
	ASSERT(got > 0 && (size_t)got == sizeof(level), return false);

	got = read(callback_pipe, &string_size, sizeof(string_size));
	ASSERT(got > 0 && (size_t)got == sizeof(string_size), return false);

	MALLOC(string, string_size + 1, return false);

	got = read(callback_pipe, string, string_size);
	if(got < 0 || got != string_size) {
		FREE(string);
		return false;
	}
	string[string_size] = '\0';

	_alpm_log(handle, level, "%s", string);
	FREE(string);
	return true;
}

bool _alpm_sandbox_process_cb_download(alpm_handle_t *handle, int callback_pipe) {
	alpm_download_event_type_t type;
	char *filename = NULL;
	size_t filename_size, cb_data_size;
	ssize_t got;
	union {
		alpm_download_event_init_t init;
		alpm_download_event_progress_t progress;
		alpm_download_event_retry_t retry;
		alpm_download_event_completed_t completed;
	} cb_data;

	got = read(callback_pipe, &type, sizeof(type));
	ASSERT(got > 0 && (size_t)got == sizeof(type), return false);

	switch (type) {
	case ALPM_DOWNLOAD_INIT:
		cb_data_size = sizeof(alpm_download_event_init_t);
		got = read(callback_pipe, &cb_data.init, cb_data_size);
		break;
	case ALPM_DOWNLOAD_PROGRESS:
		cb_data_size = sizeof(alpm_download_event_progress_t);
		got = read(callback_pipe, &cb_data.progress, cb_data_size);
		break;
	case ALPM_DOWNLOAD_RETRY:
		cb_data_size = sizeof(alpm_download_event_retry_t);
		got = read(callback_pipe, &cb_data.retry, cb_data_size);
		break;
	case ALPM_DOWNLOAD_COMPLETED:
		cb_data_size = sizeof(alpm_download_event_completed_t);
		got = read(callback_pipe, &cb_data.completed, cb_data_size);
		break;
	default:
		return false;
	}
	ASSERT(got > 0 && (size_t)got == cb_data_size, return false);

	got = read(callback_pipe, &filename_size, sizeof(filename_size));
	ASSERT(got > 0 && (size_t)got == sizeof(filename_size), return false);

	MALLOC(filename, filename_size + 1, return false);

	got = read(callback_pipe, filename, filename_size);
	if(got < 0 || (size_t)got != filename_size) {
		FREE(filename);
		return false;
	}
	filename[filename_size] = '\0';

	handle->dlcb(handle->dlcb_ctx, filename, type, &cb_data);
	FREE(filename);
	return true;
}
