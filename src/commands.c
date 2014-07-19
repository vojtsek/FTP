#include "../headers/structures.h"
#include "../headers/common.h"
#include "../headers/commands.h"
#include "../headers/conf.h"
#include "../headers/core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

void initCmd(struct cmd *command, struct cmd_function *f) {
	struct cmd_function *func;
	for (func = f; func != NULL; func = func->next) {
		if (strcasecmp(command->name, func->name) == 0) {
			command->func = func->func;
			break;
		}
		else
			command->func = NULL;
	}
}

int respond(int fd, int fst, int snd, int thd, char *response){
	char code[5];
	code[0] = '0' + fst;
	code[1] = '0' + snd;
	code[2] = '0' + thd;
	code[3] = ' ';
	code[4] = '\n';
	if (write(fd, code, 4) == -1) return (-1);
	if (write(fd, response, strlen(response)) == -1) return (-1);
	if (write(fd, &code[4], 1) == -1) return (-1);
	return (0);
}

void user(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Require username.");
		return;
	}
	printf("User: %s\n", params[0]);
	switch (lookupUser(configuration->user_db, params[0], NULL)){
		case -1:
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		break;
		case 0:
			respond(fd, 3, 3, 1, "OK, awaiting password.");
		break;
		case 1:
			respond(fd, 4, 3, 0, "Unknown user.");
			return;
		break;
	}
	struct cmd psswd;
	readCmd(fd, &psswd);
	if (strcmp(psswd.name, "PASS") != 0){
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	free(cstate->user);
	cstate->user = (char *) malloc ((1 + strlen(params[0])) * sizeof (char));
	strcpy(cstate->user, params[0]);
	cstate->path = (char *) malloc ((2 + strlen(params[0])) * sizeof (char));
	(*cstate->path) = '/';
	(*(cstate->path + 1)) = 0;
	strcpy(cstate->path + 1, params[0]);
	cstate->logged = 0;
	executeCmd(&psswd, NULL, fd, cstate, configuration);
}

void paswd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(cstate->logged){
		respond(fd, 5, 0, 3, "User already logged in.");
		return;
	}
	if (cstate->user == NULL){
		respond(fd, 5, 0, 3, "Need username before password.");
		return;
	}
	char *correct_passwd = (char *) malloc(256 * sizeof (char));
	if (lookupUser(configuration->user_db, cstate->user, correct_passwd) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(correct_passwd);
		return;
	}
	if (strcmp(params[0], correct_passwd) == 0){
		respond(fd, 2, 3, 0, "User logged in.");
		cstate->logged = 1;
		free(correct_passwd);
		return;
	}
	char *str = (char *) malloc(256 * sizeof (char));
	sprintf(str, "User %s failed to login: Wrong password.", cstate->user);
	logReport(str);
	free(str);
	respond(fd, 4, 0, 0, "Wrong password.");
	free(correct_passwd);
}

void quit(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 1, "Closing connection.");
	*abor = 1;
}

void pwd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	respond(fd, 2, 5, 7, cstate->path);
}

void cwd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char *old_path = (char *) malloc (256 * sizeof (char));
	strcpy(old_path, cstate->path);
	if ((cstate->path = changeDir(cstate->path, params[0])) == NULL){
		cstate->path = (char *) malloc (256 * sizeof (char));
		strcpy(cstate->path, old_path);
		free(old_path);
		respond(fd, 2, 5, 7, cstate->path);	
		return;
	}
	if(!isDir(cstate->path, configuration)){
		strcpy(cstate->path, old_path);
		free(old_path);
		respond(fd, 2, 5, 7, cstate->path);
		return;
	}
	free(old_path);
	respond(fd, 2, 5, 7, cstate->path);
}

void mkd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char *fpath = (char *) malloc (256 * sizeof (char));
	if(getFullPath(fpath, cstate, configuration, params[0]) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;
	}
	if(mkdir(fpath, 755) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;	
	}
	respond(fd, 2, 5, 7, fpath);
	free(fpath);