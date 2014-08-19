#ifndef NETWORKING_H
#define	NETWORKING_H

#include "../headers/structures.h"
#include <sys/socket.h>

int signaled;
int startServer(struct config *);
int runInstance(struct config *, struct sockaddr *, int);

#endif