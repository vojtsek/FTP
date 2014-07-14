#include "../headers/common.h"
#include "../headers/structures.h"
#include "../headers/errors.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void readConfiguration(struct config *configuration, int argc, char **argv) {
	size_t opt;

	// initialize the configuration using default macros
	configuration->ctrl_port = CTRL_PORT;
	configuration->data_port = DATA_PORT;
	configuration->max_conns = MAX_CONNS;
	configuration->listen_on = (char *) malloc(256);
	configuration->root_dir = (char *) malloc(256);
	configuration->user_db = (char *) malloc(256);
	strcpy(configuration->listen_on, LISTEN_ON);
	strcpy(configuration->root_dir, ROOT_DIR);
	strcpy(configuration->user_db, USER_DB);

	// process optional parametres
	while ((opt = getopt(argc, argv, "c:d:r:f:")) != -1) {
		switch (opt) {
			case 'r':
				strcpy(configuration->root_dir, optarg);
			break;
			case 'c':
				configuration->ctrl_port = atoi(optarg);
			break;
			case 'd':
				configuration->data_port = atoi(optarg);
			break;
			case '?':
				usage();
			break;
		}
	}
}

void printConfiguration(struct config *configuration) {
	printf("using configuration:\n\
data port: %d\n\
control port: %d\n\
maximum number of connections: %d\n\
root directory: %s\n",
configuration->data_port, configuration->ctrl_port,
configuration->max_conns, configuration->root_dir);
}

void logReport(char *msg) {
	fprintf(stdout, "%s\n", msg);
}

short isWhite(char c) {
	return ((c == ' ') || (c == '\n') || (c == '\r') || (c == '\t'));
}

int change_dir(char *path, char *dir) {
	char *last_slash;
	// result = (char *) malloc( strlen(path) * sizeof (path));
	// step up one directory
	if (strcmp(dir, "..") == 0){
		if (((last_slash = strrchr(path, '/')) == path) || (last_slash == NULL)) { // root dir or invalid path
			return (-1);
		}
		if((path = realloc(path, (last_slash - path) + 1)) == NULL)
			return (-1);
		(*last_slash) = 0;
		//*path = result;
		return (0);
	}
	if ((path = realloc(path, strlen(path) + strlen(dir) + 2)) == NULL)
		return (-1);
	path[strlen(path)] = '/';
	path[strlen(path)  + 1] = 0;
	strcat(path, dir);

	return (0);
}