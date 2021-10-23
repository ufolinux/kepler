/*
 *  upgrade.c
 *
 *  Copyright (c) 2006-2021 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <string.h>
#include <fnmatch.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "callback.h"
#include "conf.h"
#include "util.h"

static int fnmatch_cmp(const void *pattern, const void *string)
{
	return fnmatch(pattern, string, 0);
}

static void print_broken_dep(alpm_depmissing_t *miss)
{
	char *depstring = alpm_dep_compute_string(miss->depend);
	alpm_list_t *trans_add = alpm_trans_get_add(config->handle);
	alpm_pkg_t *pkg;
	if(miss->causingpkg == NULL) {
		/* package being installed/upgraded has unresolved dependency */
		colon_printf(_("unable to satisfy dependency '%s' required by %s\n"),
				depstring, miss->target);
	} else if((pkg = alpm_pkg_find(trans_add, miss->causingpkg))) {
		/* upgrading a package breaks a local dependency */
		colon_printf(_("installing %s (%s) breaks dependency '%s' required by %s\n"),
				miss->causingpkg, alpm_pkg_get_version(pkg), depstring, miss->target);
	} else {
		/* removing a package breaks a local dependency */
		colon_printf(_("removing %s breaks dependency '%s' required by %s\n"),
				miss->causingpkg, depstring, miss->target);
	}
	free(depstring);
}

static int sync_prepare_execute(void)
{
	alpm_list_t *i, *packages, *remove_packages, *data = NULL;
	int retval = 0;

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(config->handle, &data) == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case ALPM_ERR_PKG_INVALID_ARCH:
				for(i = data; i; i = alpm_list_next(i)) {
					char *pkg = i->data;
					colon_printf(_("package %s does not have a valid architecture\n"), pkg);
					free(pkg);
				}
				break;
			case ALPM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					print_broken_dep(i->data);
					alpm_depmissing_free(i->data);
				}
				break;
			case ALPM_ERR_CONFLICTING_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					alpm_conflict_t *conflict = i->data;
					/* only print reason if it contains new information */
					if(conflict->reason->mod == ALPM_DEP_MOD_ANY) {
						colon_printf(_("%s and %s are in conflict\n"),
								conflict->package1, conflict->package2);
					} else {
						char *reason = alpm_dep_compute_string(conflict->reason);
						colon_printf(_("%s and %s are in conflict (%s)\n"),
								conflict->package1, conflict->package2, reason);
						free(reason);
					}
					alpm_conflict_free(conflict);
				}
				break;
			default:
				break;
		}
		retval = 1;
		goto cleanup;
	}

	packages = alpm_trans_get_add(config->handle);
	remove_packages = alpm_trans_get_remove(config->handle);

	if(packages == NULL && remove_packages == NULL) {
		/* nothing to do: just exit without complaining */
		if(!config->print) {
			printf(_(" there is nothing to do\n"));
		}
		goto cleanup;
	}


	/* Search for holdpkg in target list */
	int holdpkg = 0;
	for(i = remove_packages; i; i = alpm_list_next(i)) {
		alpm_pkg_t *pkg = i->data;
		if(alpm_list_find(config->holdpkg, alpm_pkg_get_name(pkg), fnmatch_cmp)) {
			pm_printf(ALPM_LOG_WARNING, _("%s is designated as a HoldPkg.\n"),
							alpm_pkg_get_name(pkg));
			holdpkg = 1;
		}
	}
	if(holdpkg && (noyes(_("HoldPkg was found in target list. Do you want to continue?")) == 0)) {
		retval = 1;
		goto cleanup;
	}

	/* Step 3: actually perform the operation */
	if(config->print) {
		print_packages(packages);
		print_packages(remove_packages);
		goto cleanup;
	}

	display_targets();
	printf("\n");

	int confirm;
	if(config->op_s_downloadonly) {
		confirm = yesno(_("Proceed with download?"));
	} else {
		confirm = yesno(_("Proceed with installation?"));
	}
	if(!confirm) {
		retval = 1;
		goto cleanup;
	}

	multibar_move_completed_up(true);
	if(alpm_trans_commit(config->handle, &data) == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case ALPM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					alpm_fileconflict_t *conflict = i->data;
					switch(conflict->type) {
						case ALPM_FILECONFLICT_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
									conflict->file, conflict->target, conflict->ctarget);
							break;
						case ALPM_FILECONFLICT_FILESYSTEM:
							if(conflict->ctarget[0]) {
								printf(_("%s: %s exists in filesystem (owned by %s)\n"),
										conflict->target, conflict->file, conflict->ctarget);
							} else {
								printf(_("%s: %s exists in filesystem\n"),
										conflict->target, conflict->file);
							}
							break;
					}
					alpm_fileconflict_free(conflict);
				}
				break;
			case ALPM_ERR_PKG_INVALID:
			case ALPM_ERR_PKG_INVALID_CHECKSUM:
			case ALPM_ERR_PKG_INVALID_SIG:
				for(i = data; i; i = alpm_list_next(i)) {
					char *filename = i->data;
					printf(_("%s is invalid or corrupted\n"), filename);
					free(filename);
				}
				break;
			default:
				break;
		}
		/* TODO: stderr? */
		printf(_("Errors occurred, no packages were upgraded.\n"));
		retval = 1;
		goto cleanup;
	}

	/* Step 4: release transaction resources */
cleanup:
	alpm_list_free(data);
	if(trans_release() == -1) {
		retval = 1;
	}

	return retval;
}

static int group_exists(alpm_list_t *dbs, const char *name)
{
	alpm_list_t *i;
	for(i = dbs; i; i = i->next) {
		alpm_db_t *db = i->data;

		if(alpm_db_get_group(db, name)) {
			return 1;
		}
	}

	return 0;
}

static alpm_db_t *get_db(const char *dbname)
{
	alpm_list_t *i;
	for(i = alpm_get_syncdbs(config->handle); i; i = i->next) {
		alpm_db_t *db = i->data;
		if(strcmp(alpm_db_get_name(db), dbname) == 0) {
			return db;
		}
	}
	return NULL;
}

static int process_pkg(alpm_pkg_t *pkg)
{
	int ret = alpm_add_pkg(config->handle, pkg);

	if(ret == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", alpm_pkg_get_name(pkg), alpm_strerror(err));
		return 1;
	}
	config->explicit_adds = alpm_list_add(config->explicit_adds, pkg);
	return 0;
}

static int process_group(alpm_list_t *dbs, const char *group, int error)
{
	int ret = 0;
	alpm_list_t *i;
	alpm_list_t *pkgs = alpm_find_group_pkgs(dbs, group);
	int count = alpm_list_count(pkgs);

	if(!count) {
		if(group_exists(dbs, group)) {
			return 0;
		}

		pm_printf(ALPM_LOG_ERROR, _("target not found: %s\n"), group);
		return 1;
	}

	if(error) {
		/* we already know another target errored. there is no reason to prompt the
		 * user here; we already validated the group name so just move on since we
		 * won't actually be installing anything anyway. */
		goto cleanup;
	}

	if(config->print == 0) {
		char *array = malloc(count);
		int n = 0;
		const colstr_t *colstr = &config->colstr;
		colon_printf(_n("There is %d member in group %s%s%s:\n",
				"There are %d members in group %s%s%s:\n", count),
				count, colstr->groups, group, colstr->title);
		select_display(pkgs);
		if(!array) {
			ret = 1;
			goto cleanup;
		}
		if(multiselect_question(array, count)) {
			ret = 1;
			free(array);
			goto cleanup;
		}
		for(i = pkgs, n = 0; i; i = alpm_list_next(i)) {
			alpm_pkg_t *pkg = i->data;

			if(array[n++] == 0) {
				continue;
			}

			if(process_pkg(pkg) == 1) {
				ret = 1;
				free(array);
				goto cleanup;
			}
		}
		free(array);
	} else {
		for(i = pkgs; i; i = alpm_list_next(i)) {
			alpm_pkg_t *pkg = i->data;

			if(process_pkg(pkg) == 1) {
				ret = 1;
				goto cleanup;
			}
		}
	}

cleanup:
	alpm_list_free(pkgs);
	return ret;
}

static int process_targname(alpm_list_t *dblist, const char *targname,
		int error)
{
	alpm_pkg_t *pkg = alpm_find_dbs_satisfier(config->handle, dblist, targname);

	/* skip ignored packages when user says no */
	if(alpm_errno(config->handle) == ALPM_ERR_PKG_IGNORED) {
			pm_printf(ALPM_LOG_WARNING, _("skipping target: %s\n"), targname);
			return 0;
	}

	if(pkg) {
		return process_pkg(pkg);
	}
	/* fallback on group */
	return process_group(dblist, targname, error);
}

static int process_target(const char *target, int error)
{
	/* process targets */
	char *targstring = strdup(target);
	char *targname = strchr(targstring, '/');
	int ret = 0;
	alpm_list_t *dblist;

	if(targname && targname != targstring) {
		alpm_db_t *db;
		const char *dbname;
		int usage;

		*targname = '\0';
		targname++;
		dbname = targstring;
		db = get_db(dbname);
		if(!db) {
			pm_printf(ALPM_LOG_ERROR, _("database not found: %s\n"),
					dbname);
			ret = 1;
			goto cleanup;
		}

		/* explicitly mark this repo as valid for installs since
		 * a repo name was given with the target */
		alpm_db_get_usage(db, &usage);
		alpm_db_set_usage(db, usage|ALPM_DB_USAGE_INSTALL);

		dblist = alpm_list_add(NULL, db);
		ret = process_targname(dblist, targname, error);
		alpm_list_free(dblist);

		/* restore old usage so we don't possibly disturb later
		 * targets */
		alpm_db_set_usage(db, usage);
	} else {
		targname = targstring;
		dblist = alpm_get_syncdbs(config->handle);
		ret = process_targname(dblist, targname, error);
	}

cleanup:
	free(targstring);
	if(ret && access(target, R_OK) == 0) {
		pm_printf(ALPM_LOG_WARNING,
				_("'%s' is a file, did you mean %s instead of %s?\n"),
				target, "-U/--upgrade", "-S/--sync");
	}
	return ret;
}

static int load_sync(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *i;

	if(targets == NULL && !config->op_s_upgrade && !config->op_s_sync) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* process targets */
	for(i = targets; i; i = alpm_list_next(i)) {
		const char *targ = i->data;
		if(process_target(targ, retval) == 1) {
			retval = 1;
		}
	}

	return retval;
}

int do_transaction(targets_t *targets) {
	int need_repos = (config->op & PM_OP_SYNC);
	alpm_list_t *sync_dbs;

	if(targets->targets != NULL) {
		pm_printf(ALPM_LOG_ERROR, _("targets must come after operation\n"));
		return 1;
	}

	if(check_syncdbs(need_repos, 0)) {
		return 1;
	}

	sync_dbs = alpm_get_syncdbs(config->handle);

	if(config->op_s_sync) {
		/* grab a fresh package list */
		colon_printf(_("Synchronizing package databases...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"synchronizing package lists\n");
		if(!sync_syncdbs(config->op_s_sync, sync_dbs)) {
			return 1;
		}
	}

	if(check_syncdbs(need_repos, 1)) {
		return 1;
	}

	if(config->op_s_clean || config->op_s_search || config->op_s_info
			|| config->op_q_list || config->group) {
		return pacman_sync(targets->sync);
	}

	/* Step 1: create a new transaction... */
	if(trans_init(config->flags, 1) == -1) {
		return 1;
	}

	if(config->op & PM_OP_SYNC && load_sync(targets->sync)) {
		goto cleanup;
	}
	if(config->op & PM_OP_REMOVE && load_remove(targets->remove)) {
		goto cleanup;
	}
	if(config->op & PM_OP_UPGRADE && load_upgrade(targets->upgrade)) {
		goto cleanup;
	}

	if(config->op_s_upgrade) {
		if(!config->print) {
			colon_printf(_("Starting full system upgrade...\n"));
			alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
					"starting full system upgrade\n");
		}
		if(alpm_sync_sysupgrade(config->handle, config->op_s_upgrade >= 2) == -1) {
			pm_printf(ALPM_LOG_ERROR, "%s\n", alpm_strerror(alpm_errno(config->handle)));
			trans_release();
			return 1;
		}
	}

	return sync_prepare_execute();

cleanup:
	trans_release();
	return 1;
}
