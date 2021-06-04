/*
 *  files.c
 *
 *  Copyright (c) 2015-2021 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <alpm.h>
#include <alpm_list.h>
#include <regex.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"
#include "package.h"

static void print_line_machinereadable(alpm_db_t *db, alpm_pkg_t *pkg, char *filename)
{
	/* Fields are repo, pkgname, pkgver, filename separated with \0 */
	fputs(alpm_db_get_name(db), stdout);
	fputc(0, stdout);
	fputs(alpm_pkg_get_name(pkg), stdout);
	fputc(0, stdout);
	fputs(alpm_pkg_get_version(pkg), stdout);
	fputc(0, stdout);
	fputs(filename, stdout);
	fputs("\n", stdout);
}

static void dump_pkg_machinereadable(alpm_db_t *db, alpm_pkg_t *pkg, alpm_filelist_t *pkgfiles)
{
	for(size_t filenum = 0; filenum < pkgfiles->count; filenum++) {
		const alpm_file_t *file = pkgfiles->files + filenum;
		print_line_machinereadable(db, pkg, file->name);
	}
}

static void print_owned_by(alpm_db_t *db, alpm_pkg_t *pkg, char *filename)
{
	const colstr_t *colstr = &config->colstr;
	printf(_("%s is owned by %s%s/%s%s %s%s%s\n"), filename,
		colstr->repo, alpm_db_get_name(db), colstr->title,
		alpm_pkg_get_name(pkg), colstr->version,
		alpm_pkg_get_version(pkg), colstr->nocolor);
}

static void print_match(alpm_list_t *match, alpm_db_t *repo, alpm_pkg_t *pkg, int exact_file)
{
	alpm_db_t *db_local = alpm_get_localdb(config->handle);
	const colstr_t *colstr = &config->colstr;

	if(config->op_f_machinereadable) {
		alpm_list_t *ml;
		for(ml = match; ml; ml = alpm_list_next(ml)) {
			char *filename = ml->data;
			print_line_machinereadable(repo, pkg, filename);
		}
	} else if(config->quiet) {
		printf("%s/%s\n", alpm_db_get_name(repo), alpm_pkg_get_name(pkg));
	} else if(exact_file) {
		alpm_list_t *ml;
		for(ml = match; ml; ml = alpm_list_next(ml)) {
			char *filename = ml->data;
			print_owned_by(repo, pkg, filename);
		}
	} else {
		alpm_list_t *ml;
		printf("%s%s/%s%s %s%s%s", colstr->repo, alpm_db_get_name(repo),
			colstr->title, alpm_pkg_get_name(pkg),
			colstr->version, alpm_pkg_get_version(pkg), colstr->nocolor);

		print_groups(pkg);
		print_installed(db_local, pkg);
		printf("\n");

		for(ml = match; ml; ml = alpm_list_next(ml)) {
			char *filename = ml->data;
			printf("    %s\n", filename);
		}
	}
}

struct filetarget {
	char *targ;
	int exact_file;
	regex_t reg;
};

static void filetarget_free(struct filetarget *ftarg) {
	regfree(&ftarg->reg);
	/* do not free ftarg->targ as it is owned by the caller of files_search */
	free(ftarg);
}

static int files_search(alpm_list_t *syncs, alpm_list_t *targets, int regex) {
	int ret = 0;
	alpm_list_t *t, *s,  *filetargs = NULL;
	alpm_filelist_t filelist = {0};
	char *pkgname = NULL;

	for(t = targets; t; t = alpm_list_next(t)) {
		char *targ = t->data;
		size_t len = strlen(targ);
		int exact_file = strchr(targ, '/') != NULL;
		regex_t reg = {0};

		if(exact_file) {
			while(len > 1 && targ[0] == '/') {
				targ++;
				len--;
			}
		}

		if(regex) {
			if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
				pm_printf(ALPM_LOG_ERROR,
						_("invalid regular expression '%s'\n"), targ);
				ret = 1;
				continue;
			}
		}

		struct filetarget *ftarg = malloc(sizeof(struct filetarget));
		ftarg->targ = targ;
		ftarg->exact_file = exact_file;
		ftarg->reg = reg;

		filetargs = alpm_list_add(filetargs, ftarg);
	}

	if(ret != 0) {
		goto cleanup;
	}

	for(s = syncs; s; s = alpm_list_next(s)) {
		alpm_db_t *repo = s->data;
		int m;

		alpm_db_files_t *files = alpm_db_files_open(repo);

		if(!files) {
			continue;
		}

		while(1) {
			int ok = alpm_db_files_next(files, &pkgname);
			if(ok == 1) {
				break;
			}
			if(ok != 0) {
				continue;
			}

			if(alpm_db_files_load(files, &filelist) != 0) {
				ret = 1;
				continue;
			}

			alpm_pkg_t *pkg = alpm_db_get_pkg(repo, pkgname);

			for(t = filetargs; t; t = alpm_list_next(t)) {
				struct filetarget *ftarg = t->data;
				char *targ = ftarg->targ;
				regex_t *reg = &ftarg->reg;
				int exact_file = ftarg->exact_file;
				int found = 0;
				alpm_list_t *match = NULL;

				if(exact_file) {
					if(regex) {
						for(size_t f = 0; f < filelist.count; f++) {
							char *c = filelist.files[f].name;
							if(regexec(reg, c, 0, 0, 0) == 0) {
								match = alpm_list_add(match, filelist.files[f].name);
								found = 1;
							}
						}
					} else {
						if(alpm_filelist_contains(&filelist, targ)) {
							match = alpm_list_add(match, targ);
							found = 1;
						}
					}
				} else {
					for(size_t f = 0; f < filelist.count; f++) {
						char *c = strrchr(filelist.files[f].name, '/');
						if(c && *(c + 1)) {
							if(regex) {
								m = regexec(reg, (c + 1), 0, 0, 0);
							} else {
								m = strcmp(c + 1, targ);
							}
							if(m == 0) {
								match = alpm_list_add(match, filelist.files[f].name);
								found = 1;
							}
						}
					}
				}

				if(match != NULL) {
					print_match(match, repo, pkg, exact_file);
					alpm_list_free(match);
				}

				if(!found) {
					ret = 1;
				}
			}
		}

		alpm_db_files_close(files);
	}

cleanup:
	alpm_list_free_inner(filetargs, (alpm_list_fn_free) filetarget_free);
	alpm_list_free(filetargs);
	alpm_filelist_free(&filelist);

	if(pkgname) {
		free(pkgname);
	}

	return ret;
}

static void dump_file_list(alpm_pkg_t *pkg, alpm_filelist_t *pkgfiles) {
	const char *pkgname;
	size_t i;

	pkgname = alpm_pkg_get_name(pkg);

	for(i = 0; i < pkgfiles->count; i++) {
		const alpm_file_t *file = pkgfiles->files + i;
		/* Regular: '<pkgname> <filepath>\n'
		 * Quiet  : '<filepath>\n'
		 */
		if(!config->quiet) {
			printf("%s%s%s ", config->colstr.title, pkgname, config->colstr.nocolor);
		}
		printf("%s\n", file->name);
	}

	fflush(stdout);
}

static int files_list(alpm_list_t *syncs, alpm_list_t *targets) {
	alpm_list_t *i, *j;
	int ret = 0;
	size_t found = 0;
	alpm_filelist_t filelist = {0};
	char *pkgname = NULL;

	for(j = syncs; j; j = alpm_list_next(j)) {
		alpm_db_t *db = j->data;
		alpm_db_files_t *files = alpm_db_files_open(db);

		if(!files) {
			continue;
		}

		while(1) {
			int ok = alpm_db_files_next(files, &pkgname);
			if(ok == 1) {
				break;
			}
			if(ok != 0) {
				continue;
			}

			if(targets != NULL) {
				int match = 0;
				for(i = targets; i; i = alpm_list_next(i)) {
					char *targ =  i->data;
					char *c = strchr(targ, '/');
					char *repo = NULL;

					if(c) {
						if(! *(c + 1)) {
							pm_printf(ALPM_LOG_ERROR,
								_("invalid package: '%s'\n"), targ);
							ret = 1;
							continue;
						}

						repo = strndup(targ, c - targ);
						targ = c + 1;
					}

					if(repo) {
						if(strcmp(alpm_db_get_name(db), repo) != 0) {
							free(repo);
							continue;
						}
						free(repo);
					}

					if(strcmp(pkgname, targ) == 0) {
						match = 1;
						found++;
						break;
					}
				}

				if(!match) {
					continue;
				}
			}


			if(alpm_db_files_load(files, &filelist) != 0) {
				ret = 1;
				continue;
			}

			alpm_pkg_t *pkg = alpm_db_get_pkg(db, pkgname);

			if(config->op_f_machinereadable) {
				dump_pkg_machinereadable(db, pkg, &filelist);
			} else {
				dump_file_list(pkg, &filelist);
			}
			break;
		}
		alpm_db_files_close(files);
	}

	alpm_filelist_free(&filelist);

	if(found != alpm_list_count(targets)) {
		ret = 1;
	}

	if(pkgname) {
		free(pkgname);
	}

	return ret;
}

int pacman_files(alpm_list_t *targets)
{
	alpm_list_t *files_dbs = NULL;

	if(check_syncdbs(1, 0)) {
		return 1;
	}

	files_dbs = alpm_get_syncdbs(config->handle);

	if(config->op_s_sync) {
		/* grab a fresh package list */
		colon_printf(_("Synchronizing package databases...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"synchronizing package lists\n");
		if(!sync_syncdbs(config->op_s_sync, files_dbs)) {
			return 1;
		}
	}

	/* get a listing of files in sync DBs */
	if(config->op_q_list) {
		return files_list(files_dbs, targets);
	}

	if(targets == NULL && !config->op_s_sync) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* search for a file */
	return files_search(files_dbs, targets, config->op_f_regex);
}
