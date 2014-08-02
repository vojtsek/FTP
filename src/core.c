#include "../headers/core.h"
#include "../headers/structures.h"
#include "../headers/commands.h"
#include "../headers/common.h"
#include "../headers/conf.h"
#include "../headers/networking.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/un.h>
#include <string.h>
#include <time.h>
#include <strings.h> // bzero
#include <errno.h>

// reads word from the fd
// the word is separated by white characters
char readWord(int fd, char *word) {
	int r, i = 0;
	char c = 0;
	while (1) {
		if ((r = read(fd, &c, 1)) != 1)
			break;
		if (isWhite(c)) break;
		word[i++] = c;
	}
	word[i] = 0;
	// returns the white character which separated the world
	return (c);
}

void freeCmd(struct cmd *command) {
	char **params = command->params;
	while (*params != NULL)
		free(*(params++));
	free(command->params);
}

// reads command from fd
int readCmd(int fd, struct cmd *command) {
	command->params = (char **)
	malloc((MAX_CMD_PARAMS + 1) * sizeof (char *));
	int i = 0;
	char c, r;
	// reads command name
	if ((r = readWord(fd, command->name)) == 0)
		return (-1);
	// loads parametres
	while (1) {
		command->params[i] = NULL;
		if (r == '\r') {
			if (read(fd, &c, 1) != 1) {
				freeCmd(command);
				return (-1);
			}
			if (c == '\n')
			break; // \r\n sequence
		}
		if (r == '\n')
			break;
		// read another parameter
		command->params[i] = (char *) malloc(256 * sizeof (char));
		if ((r = readWord(fd, command->params[i])) == 0) {
			freeCmd(command);
			return (-1);
		}
		++i;
		if (i == MAX_CMD_PARAMS)
			break;
	}
	// returns number of read parameters
	return (i);
}

// executes command
int executeCmd(struct cmd *command, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	// loads the pointer to the apropriate function
	initCmd(command, &user_cmd);
	// calls it
	if (command->func)
		command->func(command->params, abor, fd, cstate, configuration);
	else {
		printf("%s\n", "Unknown command!");
		respond(fd, 5, 0, 2, "Command not implemented.\n");
	}

	return (0);
}

// spwans the thread to control the data connection
int spawnDataRoutine(struct state *cstate,
	struct config *configuration, int *sock) {
	struct data_info *info = (struct data_info *)
	malloc(sizeof (struct data_info));
	char name[32];
	sprintf(name, "/control_sockets/%scsk%d", cstate->dir,
		cstate->transfer_count);
	int sck;
	struct sockaddr_un sa;
	pthread_t data_thread;

	// creates and starts listen on the socket
	// for communicating between the control and data thread
	unlink(name);
	bzero(&sa, sizeof (sa));
	printf("%s\n", name);
	strncpy(sa.sun_path, name, sizeof (sa.sun_path));
	sa.sun_family = AF_UNIX;

	if ((sck = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("Error creating control socket.");
		return (-1);
	}
	if (bind(sck, (struct sockaddr *) &sa, sizeof (sa)) == -1) {
		perror("Error binding control socket.");
		return (-1);
	}
	if (listen(sck, SOMAXCONN) == -1) {
		perror("Error listening on control socket.");
		return (-1);
	}
	// initiates the information for the data thread
	info->data_port = cstate->data_port;
	strcpy(info->user, cstate->user);
	strcpy(info->path, cstate->path);
	info->cstate = cstate;
	strcpy(info->control_sock, name);
	info->configuration = configuration;
	info->client_addr = &(cstate->client_addr);
	// spawns the thread
	if ((pthread_create(&data_thread, NULL,
		&dataRoutine, info)) != 0) {
		perror("Error while creating thread.");
		return (-1);
	}
	cstate->data_thread = data_thread;
	// waits until the spawned thread makes the connection
	// file descriptor to the connection is saved
	if ((*sock = accept(sck, NULL, 0)) == -1) {
		perror("Error accepting connection on control socket.");
		return (-1);
	}
	return (0);
}

// function running in separate thread, handling data connection
void *controlRoutine(void *arg) {
	struct control_info *info = (struct control_info *) arg;
	int fd = info->fd;
	struct cmd command;
	time_t now = time(NULL);
	struct state cstate = (struct state) { .logged = 0,
		.path = "/",
		.data_port = info->configuration->data_port,
		.transfer_count = 0,
		.data_sock = 0,
		.control_sock = 0,
		.port = 1,
		.last_accepted = 0,
		.data_thread = 0,
		.transfer_type = Image,
		.client_addr = *(info->client_addr) };
	sprintf(cstate.dir, "%d", (int)now);
	short abor = 0;
	if (isDir("/control_sockets") == -1)
		mkdir("/control_sockets", 0755);
	// reads commands from the accepted connection and executes it
	while (readCmd(fd, &command) != -1) {
		executeCmd(&command, &abor, fd, &cstate, info->configuration);
		freeCmd(&command);
		if (abor) break;
	}
	return (arg);
}

// function running in separate thread, handles data connection
void *dataRoutine(void *arg) {
	struct data_info *info = (struct data_info *) arg;
	int sck;
	struct sockaddr_un sa;
	struct sockaddr_in in;
	bzero(&in, sizeof (in));
	bzero(&sa, sizeof (sa));
	struct state cstate = *(info->cstate);
	strncpy(sa.sun_path, info->control_sock, sizeof (sa.sun_path));
	sa.sun_family = AF_UNIX;

	// tries to create a connection
	// for communicating with the control thread
	if ((sck = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("Error creating control socket.");
		return (arg);
	}
	if (connect(sck, (struct sockaddr *) &sa, sizeof (sa)) == -1) {
		perror("Error connecting to control socket.");
		return (arg);
	}
	struct cmd command;
	cstate.data_sock = 0;
	// reads commands from control thread and executes them
	while (1) {
		// printf("Data socket: %d\n", cstate.data_sock);
		readUntil(command.name, sck, 0);
		if (command.name[0] == 'Q') {
			printf("Q\n");
			break;
		}
		executeCmd(&command, NULL, sck, &cstate, info->configuration);
		// projects the changes to the control thread,
		info->cstate->data_sock = cstate.data_sock;
		info->cstate->last_accepted = cstate.last_accepted;
	}
	if (cstate.data_sock)
		close(cstate.data_sock);
	free(info);
	return (arg);
}