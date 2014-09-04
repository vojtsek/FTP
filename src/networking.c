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
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <wait.h>

void *join_routine(void *info) {
	int status;
	wait(&status);
	return (info);
}

// initiate the server to the listening state
int startServer(struct config *configuration) {
	size_t sock, newsock, optval = 1;
	pid_t pid, chpid;
	int status;
	pthread_t thr;
	struct sockaddr_in6 in6;
	socklen_t psize = sizeof (struct sockaddr);
	struct sockaddr peer_addr;
	bzero(&in6, sizeof (in6));

	// initialize option
	bzero(&peer_addr, psize);
	in6.sin6_family = AF_INET6;
	in6.sin6_port = htons(configuration->ctrl_port);
	bzero(&in6.sin6_addr.s6_addr, 16);
	in6.sin6_addr.s6_addr[0] = 0;
	// open main listening stream socket, IPv6, TCP
	if ((sock = socket(AF_INET6, SOCK_STREAM, 6)) == -1)
		err(1, "Problem occured while creating the socket.");

	// make the used port reusable immediately
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1)
		err(1, "Problem occured while creating the socket.");

	optval = 0;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
		&optval, sizeof (optval)) == -1)
		err(1, "Problem occured while creating the socket.");

	// bind the socket with the port
	if (bind(sock, (struct sockaddr *)&in6, sizeof (in6)) == -1)
		err(1, "Problem occured while binding the socket.");

	// listen (maximum possible number of connections in the queue)
	if (listen(sock, SOMAXCONN) == -1)
		err(1, "Problem occured while trying to listen.");

	// infinite accepting loop
	// O_NONBLOCK not set, so accept shall block while waiting for the peer
	for (;;) {
		if ((newsock = accept(sock,
			&peer_addr, &psize)) == -1) {
			if (errno != EINTR)
				perror("Accepting the connection failed.");

			if (signaled)
				break;
		}
		switch (pid = fork()) {
			// child
			case 0:
				runInstance(configuration, &peer_addr, newsock);
				doCleanup(configuration);
				exit(0);
			break;
			case -1:
				err(1, "Error while forking.");
			break;
			// parent
			default:
				pthread_create(&thr, NULL, &join_routine, NULL);
				pthread_detach(thr);
				close(newsock);
			break;
		}
	}
	// after the loop, so signal was delivered
	// wait for all children
	while ((chpid = wait(&status)) > 0);
	if (signaled) {
		doCleanup(configuration);
		return (1);
	}
	return (0);
}

// handles the accepted control connection
int runInstance(struct config *configuration,
	struct sockaddr *client_addr, int sock) {
	char numeric_addr[INET6_ADDRSTRLEN];
	char msg[STR_LENGTH];
	int ret = 0;
	struct control_info info;
	bzero(numeric_addr, INET6_ADDRSTRLEN);

	inet_ntop(((struct sockaddr_storage *)client_addr)->ss_family,
		&(((struct sockaddr_in6 *)client_addr)->sin6_addr.s6_addr),
		numeric_addr, INET6_ADDRSTRLEN);
	snprintf(msg, STR_LENGTH,
		"Connected peer with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in6 *)client_addr)->sin6_port);
	logReport(msg);
	respond(sock, 2, 2, 0, "Service ready");
	// make the unfo structure ready
	info.fd = sock;
	info.configuration = configuration;
	info.client_addr = (struct sockaddr_storage *) client_addr;
	// spawn the control routine
	ret = controlRoutine(&info);
	// final cleanup
	snprintf(msg, STR_LENGTH,
		"Closed connection with address '%s' on port %d.\n",
		numeric_addr, ((struct sockaddr_in6 *)
			client_addr)->sin6_port);
	logReport(msg);
	close(sock);

	return (ret);
}