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

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))

int SYMEXPORT alpm_sandbox_child(const char* sandboxuser)
{
	ASSERT(sandboxuser != NULL, return 1);
	return switch_to_user(sandboxuser);
}
