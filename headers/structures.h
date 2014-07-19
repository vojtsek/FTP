#ifndef STRUCTURES_H
#define	STRUCTURES_H

#include <stddef.h>

struct config {
	size_t ctrl_port, data_port, max_conns;
	char *listen_on, *root_dir, *user_db;
};

struct state {
	short logged;
	char *user;
	char *path;
	size_t data_port;
};

typedef void (*cmd_fnc) (char **, short *, int, struct state*, struct config *);
enum CMD_NAME {LOGIN};

struct cmd {
	char name[256];
	char **params;
	cmd_fnc func;
};

struct thread_info {
	int fd;
	struct config *configuration;
};

struct cmd_function {
	struct cmd_function *next;
	cmd_fnc func;
	char name[256];
};

#endif