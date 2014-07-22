#ifndef COMMON_H
#define	COMMON_H

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

int lookupUser(char *, char *, char *);

char *readUntil(int, char);

int getFullPath(char *, struct state *, struct config *, char *);

int getHostIp(char *, struct in_addr *);
#endif