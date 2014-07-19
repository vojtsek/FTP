#include "../headers/core.h"
#include "../headers/structures.h"
#include "../headers/commands.h"
#include "../headers/common.h"
#include "../headers/conf.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

void *controlRoutine(void *arg) {
	struct thread_info *info = (struct thread_info *) arg;
	int fd = info->fd;
	struct cmd command;
	struct state cstate = {0, NULL, info->configuration->root_dir, info->configuration->data_port};
	short abor = 0;
	while (readCmd(fd, &command) != -1) {
		executeCmd(&command, &abor, fd, &cstate, info->configuration);
		if (abor) break;
	}
	return (arg);
}