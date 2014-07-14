#include "../headers/conf.h"
#include "../headers/errors.h"
#include "../headers/common.h"
#include "../headers/networking.h"
#include <stdio.h>

int main(int argc, char **argv) {

	// initialize configuration
	struct config configuration;
	readConfiguration(&configuration, argc, argv);
	printConfiguration(&configuration);

	// create a socket and star listening
	startServer(&configuration);
	return (0);
}