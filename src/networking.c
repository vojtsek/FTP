#include "../headers/networking.h"
#include "../headers/common.h"
#include "../headers/structures.h"
#include "../headers/core.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <strings.h> // bzero
#include <unistd.h> // fork
#include <pthread.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

int startServer(struct config *configuration) {
	size_t sock, newsock, psize, optval = 1;
	pid_t pid;
	struct sockaddr_in6 in6;
	psize = sizeof (struct sockaddr);
	struct sockaddr *peer_addr = (struct sockaddr *) malloc(psize);
	// initialize IPv4 option
	bzero(&in6, sizeof (in6));

	bzero(peer_addr, psize);
	in6.sin6_family = AF_INET6;
	in6.sin6_port = htons(configuration->ctrl_port);
	bzero(&in6.sin6_addr.s6_addr, 16);
	// open main listening stream socket, IPv4, TCP
	if ((sock = socket(AF_INET6, SOCK_STREAM, 6)) == -1)
		err(1, "Problem occured while creating the socket.");

	// make the used port reusable immediately
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1)
		err(1, "Problem occured while creating the socket.");

	// bind the socket with the port
	if (bind(sock, (struct sockaddr *)&in6, sizeof (in6)) == -1)
		err(1, "Problem occured while binding the socket.");

	// lsiten (maximum possible number of connections in the queue)
	if (listen(sock, SOMAXCONN) == -1)
		err(1, "Problem occured while trying to listen.");

	// infinite accepting loop
	// O_NONBLOCK not set, so accept shall block while waiting for the peer
	for (;;) {
		if ((newsock = accept(sock,
			peer_addr, &psize)) == -1)
			err(1, "Accepting the connection failed.");
		switch (pid = fork()) {
			// child
			case 0:
				runInstance(configuration, peer_addr, newsock);
				exit(0);
			break;
			case -1:
				err(1, "Error while forking.");
			break;
			// parent
			default:
				close(newsock);
				logReport("Connection accepted");
			break;
		}
	}

	return (0);
}

int runInstance(struct config *configuration,
	struct sockaddr *client_addr, int sock) {
	char numeric_addr[INET6_ADDRSTRLEN];
	char cmd[256];
	pthread_t control_thread;
	bzero(numeric_addr, INET6_ADDRSTRLEN);
	bzero(cmd, 256);

	inet_ntop(((struct sockaddr_storage *)client_addr)->ss_family,
		&(((struct sockaddr_in6 *)client_addr)->sin6_addr.s6_addr),
		numeric_addr, sizeof (numeric_addr));
	printf("Recognized peer with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in6 *)client_addr)->sin6_port);
	respond(sock, 2, 2, 0, "Service ready");
	struct control_info info;
	info.fd = sock;
	info.configuration = configuration;
	info.client_addr = client_addr;
	if ((pthread_create(&control_thread, NULL,
		&controlRoutine, &info)) != 0)
		err(1, "Error while creating thread.");
	pthread_join(control_thread, NULL);
	printf("Closed connection with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in6 *)client_addr)->sin6_port);
	close(sock);
	return (0);
}