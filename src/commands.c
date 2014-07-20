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
#include <arpa/inet.h>

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
			printf("%s\n", "ok");
			respond(fd, 3, 3, 1, "OK, awaiting password.");
		break;
		case 1:
			respond(fd, 4, 3, 0, "Unknown user.");
			return;
		break;
	}
	struct cmd psswd;
	readCmd(fd, &psswd);
	if (strcasecmp(psswd.name, "PASS") != 0){
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
	char *dir = (char *) malloc (256 * sizeof (char));
	strcpy(old_path, cstate->path);
	if(getFullPath(dir, cstate, configuration, params[0]) == -1){
		free(old_path);
		free(dir);
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	if(isDir(dir) == -1){
		free(old_path);
		free(dir);
		respond(fd, 4, 5, 1, "Directory does not exist.");
		return;
	}
	free(dir);
	if ((cstate->path = changeDir(cstate->path, params[0])) == NULL){
		cstate->path = (char *) malloc (256 * sizeof (char));
		strcpy(cstate->path, old_path);
		free(old_path);
		respond(fd, 4, 5, 1, "Internal server error.");	
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
	if(mkdir(fpath, 0755) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;	
	}
	char *response = (char *) malloc (256 * sizeof (char));
	sprintf(response, "Directory %s created.", fpath);
	respond(fd, 2, 5, 7, response);
	free(response);
	free(fpath);
}

void rmd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
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
	if(rmdir(fpath) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;	
	}
	char *response = (char *) malloc (256 * sizeof (char));
	sprintf(response, "Directory %s removed.", fpath);
	respond(fd, 2, 5, 0, response);
	free(response);
	free(fpath);
}

void cdup(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	free(params[0]);
	params[0] = (char *) malloc (3 * sizeof(char));
	params[0][0] = '.';
	params[0][1] = '.';
	params[0][2] = 0;
	cwd(params, abor, fd, cstate, configuration);
}

void store(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int sock;
	++cstate->transfer_count;
	spawnDataRoutine(cstate, configuration, &sock);
	write(sock, "Ahoj", 4);
}

void syst(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 5, "UNIX Type: L8");
}

void type(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 0, "OK.");
}

void feat(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 1, "Sorry.");
	respond(fd, 2, 1, 1, "End.");
}

void noop(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	printf("port: %d\n", ((struct sockaddr_in *)cstate->client_addr)->sin_port);
	respond(fd, 2, 2, 0, "OK.");
}

void port(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need address and port.");
		return;
	}
	char *ip_str = (char *) malloc (256 * sizeof (char));
	char *port_str = (char *) malloc (256 * sizeof (char));
	char *port_upper = (char *) malloc (4 * sizeof (char));
	char *port_lower = (char *) malloc (4 * sizeof (char));
	char *comma, *port_st;
	struct sockaddr_in *sa = (struct sockaddr_in *) malloc(sizeof (struct sockaddr_in));
	bzero(sa, sizeof (*sa));
	int size, port_no, pts = 0;
	while((comma = strchr(params[0], ',')) != NULL){
		if (++pts == 4){
			size = comma - params[0];
			port_st = comma + 1;
			break;
		}
		*comma = '.';
	}
	strncpy(ip_str, params[0], size);
	strncpy(port_str, port_st, strlen(params[0]) - size);
	comma = strchr(port_str, ',');
	strncpy(port_upper, port_str, comma - port_str);
	strncpy(port_lower, comma + 1, strlen(port_str) - (comma - port_str));
	port_no = 0;
	port_no = (atoi(port_upper) << 8) | atoi(port_lower);
	if(inet_pton(AF_INET, ip_str, &(sa->sin_addr)) == -1){
		respond(fd, 5, 0, 1, "Invalid address.");
		return;
	} 
	sa->sin_port = htons(port_no);
	cstate->client_addr = (struct sockaddr *) sa; 
	respond(fd, 2, 0, 0, "Port changed.");
}