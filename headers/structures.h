#ifndef STRUCTURES_H
#define	STRUCTURES_H

#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef enum {ASCII, Image} transfer_t;

struct config {
	size_t ctrl_port, data_port, max_conns;
	char *root_dir, *user_db;
};

struct state {
	short logged;
	char user[32];
	char path[256];
	char dir[32];
	size_t data_port, transfer_count;
	int data_sock, control_sock, port, last_accepted, addr_family;
	pthread_t data_thread;
	transfer_t transfer_type;
	struct sockaddr_storage client_addr;
};

typedef void (*cmd_fnc) (char **, short *, int,
	struct state *, struct config *);

struct cmd {
	char name[256];
	char **params;
	cmd_fnc func;
};

struct control_info {
	int fd;
	struct config *configuration;
	struct sockaddr_storage *client_addr;

};

struct data_info {
	size_t data_port;
	char control_sock[32];
	char user[32];
	char path[256];
	char *fn;
	struct config *configuration;
	struct state *cstate;
	struct sockaddr_storage *client_addr;
};

struct cmd_function {
	cmd_fnc func;
	char name[256];
};

#endif