/*
  Copyright (C) 2008, 2009 Jiri Olsa <olsajiri@gmail.com>

  This file is part of the latrace.

  The latrace is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  The latrace is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the latrace (file COPYING).  If not, see 
  <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "config.h"

struct lt_config_audit cfg;

static int read_config(char *dir)
{
	int fd;
	off_t len;
	char file[LT_MAXFILE];

	memset(&cfg, 0, sizeof(cfg));

	cfg.dir = dir;
	sprintf(file, "%s/config", dir);

	if (-1 == (fd = open(file, O_RDONLY))) {
		perror("open failed");
		return -1;
	}

	if (-1 == read(fd, &cfg.sh, sizeof(cfg.sh))) {
		perror("read failed");
		return -1;
	}

	if (-1 == (len = lseek(fd, 0, SEEK_END))) {
		perror("lseek failed");
		return -1;
	}

	if (len != sizeof(cfg.sh)) {
		printf("config file size differs\n");
		return -1;
	}

	if (LT_MAGIC != cfg.sh.magic) {
		printf("config file magic check failed\n");
		return -1;
	}

	return 0;
}

static int get_names(struct lt_config_audit *cfg, char *names, char **ptr)
{
	char* s;
	int cnt = 0;

	PRINT_VERBOSE(cfg->sh.verbose, 1, "names: [%s] max: %d\n", 
			names, LT_NAMES_MAX);

	s = strchr(names, LT_NAMES_SEP);
	while(NULL != (s = strchr(names, LT_NAMES_SEP)) && (cnt < LT_NAMES_MAX)) {
		*s = 0x0;
		PRINT_VERBOSE(cfg->sh.verbose, 1, "got: %s", names);
		ptr[cnt++] = names;
		names = ++s;
	}

	if (cnt) {
		ptr[cnt++] = names;
		PRINT_VERBOSE(cfg->sh.verbose, 1, "got: %s\n", names);
	}

	if (!cnt && *names) {
		ptr[0] = names;
		cnt = 1;
		PRINT_VERBOSE(cfg->sh.verbose, 1, "got: %s\n", names);
	}

	ptr[cnt] = NULL;

	if (!cnt)
		return -1;

	PRINT_VERBOSE(cfg->sh.verbose, 1, "got %d entries\n", cnt);
	return cnt;
}

int audit_init(int argc, char **argv, char **env)
{
	if (-1 == read_config(getenv("LT_DIR")))
		return -1;

	/* -Aa */
	if (cfg.sh.args_enabled && lt_args_init(&cfg.sh))
		return -1;

	/* -t */
	if ((*cfg.sh.libs_to) && 
	    (-1 == (cfg.libs_to_cnt = get_names(&cfg, cfg.sh.libs_to, cfg.libs_to)))) {
		printf("latrace failed to parse libs to\n");
		return -1;
	}

	/* -f */
	if ((*cfg.sh.libs_from) && 
	    (-1 == (cfg.libs_from_cnt = get_names(&cfg, cfg.sh.libs_from, cfg.libs_from)))) {
		printf("latrace failed to parse libs from\n");
		return -1;
	}

	/* -l */
	if ((*cfg.sh.libs_both) && 
	    (-1 == (cfg.libs_both_cnt = get_names(&cfg, cfg.sh.libs_both, cfg.libs_both)))) {
		printf("latrace failed to parse libs from\n");
		return -1;
	}

	/* -s */
	if ((*cfg.sh.symbols) && 
	    (-1 == (cfg.symbols_cnt = get_names(&cfg, cfg.sh.symbols, cfg.symbols)))) {
		printf("latrace failed to parse symbols\n");
		return -1;
	}

	/* -b */
	if ((*cfg.sh.flow_below) && 
	    (-1 == (cfg.flow_below_cnt = get_names(&cfg, cfg.sh.flow_below, cfg.flow_below)))) {
		printf("latrace failed to parse symbols in flow-below option\n");
		return -1;
	}

	/* -L */
	if (*cfg.sh.libs_subst) {

		char *ptr[LT_NAMES_MAX];
		int cnt;

		if (-1 == (cnt = get_names(&cfg, cfg.sh.libs_subst, ptr))) {
			printf("latrace failed to parse input for subst option\n");
			return -1;
		}

		if (-1 == lt_objsearch_init(&cfg, ptr, cnt)) {
			printf("latrace failed to nitialize subst option\n");
			return -1;
		}
	}

	/* -o */
	cfg.sh.fout = stdout;
	if ((*cfg.sh.output) && (NULL == (cfg.sh.fout = fopen(cfg.sh.output, "w")))) {
		printf("latrace failed to open output file %s\n", cfg.sh.output);
		return -1;
	}

	/* -E */
	if (cfg.sh.not_follow_exec)
		unsetenv("LD_AUDIT");

	/* -F */
	if (cfg.sh.not_follow_fork)
		cfg.sh.pid = getpid();

	cfg.init_ok = 1;
	return 0;
}

void finalize(void) __attribute__((destructor));

void
finalize(void)
{
	if ((!cfg.sh.pipe) && (*cfg.sh.output))
		fclose(cfg.sh.fout);
}