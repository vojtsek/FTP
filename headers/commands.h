#ifndef COMMANDS_H
#define	COMMANDS_H

#include "structures.h"


void initCmd(struct cmd *, struct cmd_function *);
int respond(int, int, int, int, char *);
void user(char **, short *, int, struct state *, struct config *);
void paswd(char **, short *, int, struct state *, struct config *);
void cwd(char **, short *, int, struct state *, struct config *);
void pwd(char **, short *, int, struct state *, struct config *);
void rmd(char **, short *, int, struct state *, struct config *);
void mkd(char **, short *, int, struct state *, struct config *);
void cdup(char **, short *, int, struct state *, struct config *);
void quit(char **, short *, int, struct state *, struct config *);


static struct cmd_function quit_cmd = {NULL, quit, "QUIT"};
static struct cmd_function paswd_cmd = {&quit_cmd, paswd, "PASS"};
static struct cmd_function cwd_cmd = {&paswd_cmd, cwd, "CWD"};
static struct cmd_function pwd_cmd = {&cwd_cmd, pwd, "PWD"};
static struct cmd_function rmd_cmd = {&pwd_cmd, pwd, "RMD"};
static struct cmd_function mkd_cmd = {&rmd_cmd, pwd, "MKD"};
static struct cmd_function cdup_cmd = {&mkd_cmd, cdup, "CDUP"};
static struct cmd_function user_cmd = {&cdup_cmd, user, "USER"};


#endif