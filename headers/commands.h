#ifndef COMMANDS_H
#define	COMMANDS_H

#define BUFSIZE 2048
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
void syst(char **, short *, int, struct state *, struct config *);
void type(char **, short *, int, struct state *, struct config *);
void feat(char **, short *, int, struct state *, struct config *);
void noop(char **, short *, int, struct state *, struct config *);
void port(char **, short *, int, struct state *, struct config *);
void list(char **, short *, int, struct state *, struct config *);
void pasv(char **, short *, int, struct state *, struct config *);
void stor(char **, short *, int, struct state *, struct config *);
void retr(char **, short *, int, struct state *, struct config *);
void abor(char **, short *, int, struct state *, struct config *);

void d_pasv(char **, short *, int, struct state *, struct config *);
void d_list(char **, short *, int, struct state *, struct config *);
void d_retr(char **, short *, int, struct state *, struct config *);
void d_stor(char **, short *, int, struct state *, struct config *);
void quit(char **, short *, int, struct state *, struct config *);


static struct cmd_function quit_cmd = {NULL, quit, "QUIT"};
static struct cmd_function paswd_cmd = {&quit_cmd, paswd, "PASS"};
static struct cmd_function cwd_cmd = {&paswd_cmd, cwd, "CWD"};
static struct cmd_function pwd_cmd = {&cwd_cmd, pwd, "PWD"};
static struct cmd_function rmd_cmd = {&pwd_cmd, rmd, "RMD"};
static struct cmd_function mkd_cmd = {&rmd_cmd, mkd, "MKD"};
static struct cmd_function cdup_cmd = {&mkd_cmd, cdup, "CDUP"};
static struct cmd_function stor_cmd = {&cdup_cmd, stor, "STOR"};
static struct cmd_function retr_cmd = {&stor_cmd, retr, "RETR"};
static struct cmd_function syst_cmd = {&retr_cmd, syst, "SYST"};
static struct cmd_function type_cmd = {&syst_cmd, type, "TYPE"};
static struct cmd_function feat_cmd = {&type_cmd, feat, "FEAT"};
static struct cmd_function noop_cmd = {&feat_cmd, noop, "NOOP"};
static struct cmd_function port_cmd = {&noop_cmd, port, "PORT"};
static struct cmd_function list_cmd = {&port_cmd, list, "LIST"};
static struct cmd_function pasv_cmd = {&list_cmd, pasv, "PASV"};
static struct cmd_function abor_cmd = {&pasv_cmd, abor, "ABOR"};

static struct cmd_function d_pasv_cmd = {&abor_cmd, d_pasv, "DPASV"};
static struct cmd_function d_list_cmd = {&d_pasv_cmd, d_list, "DLIST"};
static struct cmd_function d_retr_cmd = {&d_list_cmd, d_retr, "DRETR"};
static struct cmd_function d_stor_cmd = {&d_retr_cmd, d_stor, "DSTOR"};
static struct cmd_function user_cmd = {&d_stor_cmd, user, "USER"};
#endif