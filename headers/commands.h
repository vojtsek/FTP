#ifndef COMMANDS_H
#define	COMMANDS_H

#include "structures.h"


void initCmd(struct cmd *, struct cmd_function *);
void user(char **, short *, int);
void paswd(char **, short *, int);
void quit(char **, short *, int);


static struct cmd_function quit_cmd = {NULL, quit, "QUIT"};
static struct cmd_function paswd_cmd = {&quit_cmd, paswd, "PASWD"};
static struct cmd_function user_cmd = {&paswd_cmd, user, "USER"};

#endif