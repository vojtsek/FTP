#include "../headers/conf.h"
#include "../headers/errors.h"
#include "../headers/common.h"
#include "../headers/networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <errno.h>


int main(int argc, char **argv) {

	// initialize configuration
	struct config configuration;
	readConfiguration(&configuration, argc, argv);
	printConfiguration(&configuration);
	if(chroot(ROOT_DIR) == -1){
		err(1, "Change root failed.");
	}
	// create a socket and start listening
	startServer(&configuration);
	// do cleanup
	free(configuration.listen_on);
	free(configuration.root_dir);
	free(configuration.user_db);
	return (0);
}