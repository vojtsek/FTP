#ifndef CORE_H
#define	CORE_H

#include "../headers/structures.h"

char readWord(int, char *, size_t max);
int readCmd(int, struct cmd *);
int executeCmd(struct cmd *, short *, int, struct state *, struct config *);
int controlRoutine(struct control_info *info);
void *dataRoutine(void *);
int spawnDataRoutine(struct state *, struct config *, int *);
void freeCmd(struct cmd *);
#endif