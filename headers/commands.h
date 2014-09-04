#ifndef COMMANDS_H
#define	COMMANDS_H

#define	BUFSIZE 2048
#include "structures.h"


void initCmd(struct cmd *,
	const struct cmd_function all_commands[], int cmd_length);
int respond(int, int, int, int, char *);
void user(char **, short *, int, struct state *, struct config *);
void paswd(char **, short *, int, struct state *, struct config *);
void cwd(char **, short *, int, struct state *, struct config *);
void pwd(char **, short *, int, struct state *, struct config *);
void rmd(char **, short *, int, struct state *, struct config *);
void mkd(char **, short *, int, struct state *, struct config *);
void dele(char **, short *, int, struct state *, struct config *);
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
void epsv(char **, short *, int, struct state *, struct config *);
void eprt(char **, short *, int, struct state *, struct config *);


void d_pasv(char **, short *, int, struct state *, struct config *);
void d_list(char **, short *, int, struct state *, struct config *);
void d_retr(char **, short *, int, struct state *, struct config *);
void d_stor(char **, short *, int, struct state *, struct config *);
void d_epsv(char **, short *, int, struct state *, struct config *);
void quit(char **, short *, int, struct state *, struct config *);


static const struct cmd_function all_commands[] = {

{&quit, "QUIT"},
{&paswd, "PASS"},
{&cwd, "CWD"},
{&pwd, "PWD"},
{&rmd, "RMD"},
{&mkd, "MKD"},
{&dele, "DELE"},
{&cdup, "CDUP"},
{&stor, "STOR"},
{&retr, "RETR"},
{&syst, "SYST"},
{&type, "TYPE"},
{&feat, "FEAT"},
{&noop, "NOOP"},
{&port, "PORT"},
{&list, "LIST"},
{&pasv, "PASV"},
{&abor, "ABOR"},
{&epsv, "EPSV"},
{&eprt, "EPRT"},
{&d_pasv, "DPASV"},
{&d_list, "DLIST"},
{&d_retr, "DRETR"},
{&d_stor, "DSTOR"},
{&d_epsv, "DEPSV"},
{&user, "USER"}
};
#endif