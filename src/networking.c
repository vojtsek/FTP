#include "../headers/networking.h"
#include "../headers/common.h"
#include "../headers/commands.h"
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

// initiate the server to the listening state
int startServer(struct config *configuration) {
	size_t sock, newsock, psize, optval = 1;
	pid_t pid;
	// struct sockaddr_in6 in6;
	// psize = sizeof (struct sockaddr);
	// struct sockaddr *peer_addr = (struct sockaddr *) malloc(psize);
	// bzero(&in6, sizeof (in6));

	// // initialize option
	// bzero(peer_addr, psize);
	// in6.sin6_family = AF_INET6;
	// in6.sin6_port = htons(configuration->ctrl_port);
	// bzero(&in6.sin6_addr.s6_addr, 16);
	// // open main listening stream socket, IPv6, TCP
	// if ((sock = socket(AF_INET6, SOCK_STREAM, 6)) == -1)
	struct sockaddr_in in;
	psize = sizeof (struct sockaddr);
	struct sockaddr *peer_addr = (struct sockaddr *) malloc(psize);
	bzero(&in, sizeof (in));

	// initialize option
	bzero(peer_addr, psize);
	in.sin_family = AF_INET;
	in.sin_port = htons(configuration->ctrl_port);
	if (inet_pton(AF_INET, configuration->listen_on,
		&(in.sin_addr)) == -1)
		err(1, "Problem occured while creating the socket.");
	// bzero(&in6.sin6_addr.s6_addr, 16);
	// open main listening stream socket, IPv6, TCP
	if ((sock = socket(AF_INET, SOCK_STREAM, 6)) == -1)
		err(1, "Problem occured while creating the socket.");

	// make the used port reusable immediately
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1)
		err(1, "Problem occured while creating the socket.");

	// bind the socket with the port
	if (bind(sock, (struct sockaddr *)&in, sizeof (in)) == -1)
		err(1, "Problem occured while binding the socket.");

	// listen (maximum possible number of connections in the queue)
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
			break;
		}
	}

	return (0);
}

// handles the accepted control connection
int runInstance(struct config *configuration,
	struct sockaddr *client_addr, int sock) {
	char numeric_addr[INET_ADDRSTRLEN];
	char cmd[256];
	char msg[128];
	pthread_t control_thread;
	struct control_info info;
	bzero(numeric_addr, INET_ADDRSTRLEN);
	bzero(cmd, 256);

	inet_ntop(((struct sockaddr_storage *)client_addr)->ss_family,
		&(((struct sockaddr_in *)client_addr)->sin_addr.s_addr),
		numeric_addr, sizeof (numeric_addr));
	sprintf(msg, "Connected peer with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in *)client_addr)->sin_port);
	logReport(msg);
	respond(sock, 2, 2, 0, "Service ready");
	// make the unfo structure ready
	info.fd = sock;
	info.configuration = configuration;
	info.client_addr = (struct sockaddr_storage *) client_addr;
	// spawn the control thread
	if ((pthread_create(&control_thread, NULL,
		&controlRoutine, &info)) != 0)
		err(1, "Error while creating thread.");
	pthread_join(control_thread, NULL);
	// final cleanup
	sprintf(msg, "Closed connection with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in *)client_addr)->sin_port);
	logReport(msg);
	close(sock);
	free(client_addr);
	return (0);
}