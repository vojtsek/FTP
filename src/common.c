#include "../headers/common.h"
#include "../headers/structures.h"
#include "../headers/errors.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

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

char *changeDir(char *path, char *dir) {
	char *last_slash, *old_path = path;
	// step up one directory
	if (strcmp(dir, "..") == 0){
		if (((last_slash = strrchr(path, '/')) == path) || (last_slash == NULL)) { // root dir or invalid path
			return path;
		}
		if((path = realloc(path, (last_slash - path) + 1)) == NULL){
			free(old_path);
			return NULL;
		}
		(*last_slash) = 0;
		return path;
	}
	size_t path_l = strlen(path);
	if ((path = realloc(path, path_l + strlen(dir) + 2)) == NULL){
		free(old_path);
		return NULL;
	}
	path[path_l] = '/';
	path[path_l + 1] = 0;
	strcat(path, dir);

	return path;
}

short isDir(char *path, struct config *configuration){
	char *dir = (char *) malloc(256 * sizeof (char));
	strcpy(dir, configuration->root_dir);
	strcat(dir, path);
	if(opendir(dir) == NULL){
		free(dir);
		return (0);
	}
	free(dir);
	return (1);
}

char *readUntil(int fd, char sep){
	char *buf = (char *) malloc (256 * sizeof (char));
	char *res = buf;
	char c;
	while((read(fd, &c, 1) > 0) && (c != sep)){
		(*buf++) = c;
	}
	if(c == sep) return (res);
	else {
		free(res);
		return (NULL);
	}
}

int lookupUser(char *path, char *username, char *passwd){
	char *user, *pswd;
	user = pswd = NULL;
	int fd, res = 1;
	if((fd = open(path, O_RDONLY)) == -1 )
		return (-1);
	while((user = readUntil(fd, ':')) != NULL){
		pswd = readUntil(fd, '\n');
		if(pswd == NULL){
			res = 1;
			break;
		}
		if(strcmp(username, user) == 0){
			res = 0;
			break;
		}
		free(pswd);
		free(user);
		pswd = user = NULL;
	}
	if (pswd == NULL){
		return (1);
	}
	if(passwd == NULL) return (res);
	strcpy(passwd, pswd);
	free(pswd);
	free(user);
	return (res);
}

int getFullPath(char *fpath, struct state *cstate, struct config *configuration, char *dirname){
	// dirname begins with slash - its dirname
	// otherwise its concatenatio of the three
	return (0);
}