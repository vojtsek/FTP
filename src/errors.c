#include "../headers/errors.h"
#include <stdio.h>

void usage() {
	fprintf(stderr, "Usage: ./run [-c control_port] [-d data_port]\
[-r fs_root] [-f configuration_file]\n");
}