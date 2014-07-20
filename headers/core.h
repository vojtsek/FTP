#ifndef CORE_H
#define	CORE_H

#include "../headers/structures.h"

char readWord(int, char *);
int readCmd(int, struct cmd *);
int executeCmd(struct cmd *, short *, int, struct state *, struct config *);
void *controlRoutine(void *);
void *dataRoutine(void *);
int spawnDataRoutine(struct state *, struct config *, int *);
#endif