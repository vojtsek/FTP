#include "../headers/structures.h"
#include "../headers/commands.h"
#include "../headers/conf.h"
#include "../headers/core.h"
#include <stdio.h>
#include <string.h>

void initCmd(struct cmd *command, struct cmd_function *f) {
	struct cmd_function *func;
	for (func = f; func != NULL; func = func->next) {
		if (strcmp(command->name, func->name) == 0) {
			command->func = func->func;
			break;
		}
		else
			command->func = NULL;
	}
}

void user(char **params, short *abor, int fd) {
	if (params[0] == NULL){
		printf("%s\n", "Require username!");
		return;
	}
	printf("User: %s\n", params[0]);
	char response[100] = "Please insert password.\n";
	write(fd, response, strlen(response));
	struct cmd psswd;
	readCmd(fd, &psswd);
	if (strcmp(psswd.name, "PASWD") != 0){
		char response2[100] = "I said password.\n";
		write(fd, response2, strlen(response2));
	}
	executeCmd(&psswd, NULL, fd);
}

void paswd(char **params, short *abor, int fd) {
	printf("Passwd\n");
}

void quit(char **params, short *abor, int fd) {
	printf("Bye.\n");
	*abor = 1;
}