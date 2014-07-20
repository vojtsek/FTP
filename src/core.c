#include "../headers/core.h"
#include "../headers/structures.h"
#include "../headers/commands.h"
#include "../headers/common.h"
#include "../headers/conf.h"
#include "../headers/networking.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/un.h>
#include <string.h>
#include <strings.h> // bzero
#include <errno.h>

char readWord(int sock, char *word) {
	int r, i = 0;
	char c = 0;
	while (1) {
		if ((r = read(sock, &c, 1)) != 1)
			break;
		if (isWhite(c)) break;
		word[i++] = c;
	}
	word[i] = 0;
	return (c);
}

int readCmd(int sock, struct cmd *command) {
	command->params = (char **)
	malloc((MAX_CMD_PARAMS + 1) * sizeof (char *));
	int i = 0;
	char c, r;
	if ((r = readWord(sock, command->name)) == 0)
		return (-1);
	while (1) {
		if (r == '\r') {
			if (read(sock, &c, 1) != 1)
				return (-1);
			if (c == '\n') break; // \r\n sequence
		}
		if (r == '\n') break;
		command->params[i] = (char *) malloc(256 * sizeof (char));
		if ((r = readWord(sock, command->params[i])) == 0)
			return (-1);
		++i;
		if (i == MAX_CMD_PARAMS) break;
	}
	command->params[i] = NULL;
	return (i);
}

int executeCmd(struct cmd *command, short *abor, int fd, struct state *cstate,struct config *configuration) {
	initCmd(command, &user_cmd);
	if (command->func)
		command->func(command->params, abor, fd, cstate, configuration);
	else {
		printf("%s\n", "Unknown command!");
		respond(fd, 5, 0, 2, "Command not implemented.\n");
	}

	return (0);
}

int spawnDataRoutine(struct state *cstate, struct config *configuration, int* sock){
	struct data_info *info = (struct data_info *) malloc(sizeof(struct data_info));
	char name[5] = "csk";
	name[3] = '0' + cstate->transfer_count;
	name[4] = 0;
	int sck;
	struct sockaddr_un sa;

	unlink(name);
	bzero(&sa, sizeof (sa));
	strncpy(sa.sun_path, name, sizeof (sa.sun_path));
	sa.sun_family = AF_UNIX;

	if ((sck = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("Error creating control socket.");
		return (-1);
	}
	if (bind(sck, (struct sockaddr *) &sa, sizeof (sa)) == -1){
		perror("Error binding control socket.");
		return (-1);
	}
	if (listen(sck, SOMAXCONN) == -1){
		perror("Error listening on control socket.");
		return (-1);
	}
	info->data_port = cstate->data_port;
	info->user = (char *) malloc(256 * sizeof (char));
	info->path = (char *) malloc(256 * sizeof (char));
	info->control_sock = (char *) malloc(256 * sizeof (char));
	info->configuration = (struct config *) malloc(sizeof (struct config));
	info->user = cstate->user;
	info->path = cstate->path;
	strcpy(info->control_sock, name);
	info->configuration = configuration;
	info->client_addr = cstate->client_addr;
	pthread_t data_thread;
	if ((pthread_create(&data_thread, NULL,
		&dataRoutine, info)) != 0){
		perror("Error while creating thread.");
		return (-1);
	}
	if ((*sock = accept(sck, NULL, 0)) == -1){
		perror("Error accepting connection on control socket.");
		return (-1);
	}
	return (0);
}

void *controlRoutine(void *arg) {
	struct control_info *info = (struct control_info *) arg;
	int fd = info->fd;
	struct cmd command;
	struct state cstate = {0, NULL, info->configuration->root_dir, info->configuration->data_port, 0, info->client_addr};
	short abor = 0;
	while (readCmd(fd, &command) != -1) {
		printf("%s\n", command.name);
		executeCmd(&command, &abor, fd, &cstate, info->configuration);
		if (abor) break;
	}
	return (arg);
}

void *dataRoutine(void *arg){
	struct data_info *info = (struct data_info *) arg;
	int sck;
	struct sockaddr_un sa;
	bzero(&sa, sizeof (sa));
	char buf[128];

	strncpy(sa.sun_path, info->control_sock, sizeof (sa.sun_path));
	sa.sun_family = AF_UNIX;

	if ((sck = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("Error creating control socket.");
		return (arg);
	}
	if (connect(sck, (struct sockaddr *) &sa, sizeof (sa)) == -1) {
		perror("Error connecting to control socket.");
		return (arg);
	}
	read(sck, buf, 4);
	buf[4] = 0;
	printf("%s\n", buf);
	free(arg);
	return (arg);
}