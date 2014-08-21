#include "../headers/conf.h"
#include "../headers/errors.h"
#include "../headers/common.h"
#include "../headers/networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

// initialize configuration
struct config configuration;
int signaled = 0;

void
sig_handler(int s) {
	signaled = 1;
}

int main(int argc, char **argv) {
	// register signal handler
	struct sigaction s_action;
	sigemptyset(&s_action.sa_mask);
	s_action.sa_handler = sig_handler;
	s_action.sa_flags = 0;
	sigaction(SIGINT, &s_action, NULL);

	readConfiguration(&configuration, argc, argv);
	printConfiguration(&configuration);
	if (getuid()) {
		fprintf(stderr, "Run with root privileges to chroot.\n");
		exit(1);
	}
	if (chdir(ROOT_DIR) == -1) {
		err(1, "Change root failed.");
	}
	if (chroot(ROOT_DIR) == -1) {
		err(1, "Change root failed.");
	}
	setgid(UID);
	if (setuid(UID) == -1) {
		err(1, "Change root failed.");
	}
	// create a socket and start listening
	printf("Starting the server, quit with Ctrl + C\n");
	startServer(&configuration);

	return (0);
}