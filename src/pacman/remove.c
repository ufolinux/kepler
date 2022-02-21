/*
 *  remove.c
 *
 *  Copyright (c) 2006-2022 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdlib.h>
#include <stdio.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"

static int remove_target(const char *target)
{
	alpm_pkg_t *pkg;
	alpm_db_t *db_local = alpm_get_localdb(config->handle);
	alpm_list_t *p;

	if((pkg = alpm_db_get_pkg(db_local, target)) != NULL) {
		if(alpm_remove_pkg(config->handle, pkg) == -1) {
			alpm_errno_t err = alpm_errno(config->handle);
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", target, alpm_strerror(err));
			return -1;
		}
		config->explicit_removes = alpm_list_add(config->explicit_removes, pkg);
		return 0;
	}

		/* fallback to group */
	alpm_group_t *grp = alpm_db_get_group(db_local, target);
	if(grp == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("target not found: %s\n"), target);
		return -1;
	}
	for(p = grp->packages; p; p = alpm_list_next(p)) {
		pkg = p->data;
		if(alpm_remove_pkg(config->handle, pkg) == -1) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", target,
					alpm_strerror(alpm_errno(config->handle)));
			return -1;
		}
		config->explicit_removes = alpm_list_add(config->explicit_removes, pkg);
	}
	return 0;
}

int load_remove(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *i;

	if(targets == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* Step 1: add targets to the created transaction */
	for(i = targets; i; i = alpm_list_next(i)) {
		char *target = i->data;
		if(strncmp(target, "local/", 6) == 0) {
			target += 6;
		}
		if(remove_target(target) == -1) {
			retval = 1;
		}
	}

	return retval;
}

