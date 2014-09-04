#ifndef COMMON_H
#define	COMMON_H

#define	PATH_LENGTH 256
#define	USER_LENGTH 32
#define	DIR_LENGTH 32
#define	STR_LENGTH 256
#define	DEBUG
#ifdef DEBUG
#define	REP(WHAT) printf("%s\n", WHAT); fflush(stdout);
#else
#define	REP(WHAT) do { } while (0);
#endif

#include "conf.h"
#include "structures.h"

void readConfiguration(struct config *, int, char **);
void printConfiguration(struct config *);
void logReport(char *msg);
void *allocate(size_t size);
short isWhite(char);
char *changeDir(char *, char *);
short isDir(char *);
short isFileOk(char *);
int lookupUser(char *, char *, char *, size_t max);
int readUntil(char *, int, char, size_t max);
int getFullPath(char *, struct state *, struct config *, char *);
int getHostIp(char *, int);
int im2as(char *, char *, int);
int as2im(char *, char *, int);
short spawnConnection(struct state *, int *);
void doCleanup(struct config *);
int listDir(char *, char *);
int rmrDir(char *);
#endif