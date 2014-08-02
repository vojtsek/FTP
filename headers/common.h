#ifndef COMMON_H
#define	COMMON_H

#define PATH_LENGTH 256
#define USER_LENGTH 32
#define DIR_LENGTH 32
#define STR_LENGTH 256
#define DEBUG
#ifdef DEBUG
#define REP(WHAT) printf("%s\n", WHAT); fflush(stdout);
#else
#define REP(WHAT) do { } while(0);
#endif

#include "conf.h"
#include "structures.h"

// reads configuratuin into struct, using optional parametres
void readConfiguration(struct config *, int, char **);
// prints the configuration
void printConfiguration(struct config *);
// print the given message to the logfile
void logReport(char *msg);
// true if param is white char
short isWhite(char);
// change the path to point to a given directory
char *changeDir(char *, char *);

short isDir(char *);

short isFileOk(char *);

int lookupUser(char *, char *, char *);

int readUntil(char*, int, char);

int getFullPath(char *, struct state *, struct config *, char *);

int getHostIp(char *, struct in_addr *);

int im2as(char *, char *, int);

int as2im(char *, char *, int);

short spawnConnection(struct state *, int *);

int listDir(char *, char*);
#endif