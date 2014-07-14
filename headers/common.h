#ifndef COMMON_H
#define	COMMON_H

#include "conf.h"
#include "structures.h"

// reads configuratuin into struct, using optional parametres
void readConfiguration(struct config *, int, char **);
// prints the configuration
void printConfiguration(struct config *);
// converts the response to printable form
int struct2response(char *, struct response *);
// print the given message to the logfile
void logReport(char *msg);
// true if param is white char
short isWhite(char);
// change the path to point to a given directory
int change_dir(char *, char *);

#endif