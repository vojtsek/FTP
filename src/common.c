#include "../headers/common.h"
#include "../headers/structures.h"
#include "../headers/errors.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/un.h>
#include <strings.h> // bzero
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#define MAXHOST 256

void readConfiguration(struct config *configuration, int argc, char **argv) {
	size_t opt;

	// initialize the configuration using default macros
	configuration->ctrl_port = CTRL_PORT;
	configuration->data_port = DATA_PORT;
	configuration->max_conns = MAX_CONNS;
	configuration->listen_on = (char *) malloc(256);
	configuration->root_dir = (char *) malloc(256);
	configuration->user_db = (char *) malloc(256);
	strcpy(configuration->listen_on, LISTEN_ON);
	strcpy(configuration->root_dir, ROOT_DIR);
	strcpy(configuration->user_db, USER_DB);

	// process optional parametres
	while ((opt = getopt(argc, argv, "c:d:r:f:")) != -1) {
		switch (opt) {
			case 'r':
				strcpy(configuration->root_dir, optarg);
			break;
			case 'c':
				configuration->ctrl_port = atoi(optarg);
			break;
			case 'd':
				configuration->data_port = atoi(optarg);
			break;
			case '?':
				usage();
			break;
		}
	}
}

void printConfiguration(struct config *configuration) {
	printf("using configuration:\n\
data port: %d\n\
control port: %d\n\
maximum number of connections: %d\n\
root directory: %s\n",
configuration->data_port, configuration->ctrl_port,
configuration->max_conns, configuration->root_dir);
}


void logReport(char *msg) {
	fprintf(stdout, "%s\n", msg);
}

short isWhite(char c) {
	return ((c == ' ') || (c == '\n') || (c == '\r') || (c == '\t'));
}

char *changeDir(char *path, char *dir) {
	char *last_slash;
	// step up one directory
	if (strcmp(dir, "..") == 0){
		if (((last_slash = strrchr(path, '/')) == path) || (last_slash == NULL)) { // root dir or invalid path
			return (path);
		}
		(*last_slash) = 0; // shorten the string
		return path;
	}
	if(dir[0] == '/'){
		if (strcpy(path, dir) == NULL){
			return (NULL);
		}
		return path;
	}
	size_t path_l = strlen(path);
	if ((path_l + strlen(dir) + 1) > PATH_LENGTH) return (NULL);
	path[path_l] = '/';
	path[path_l + 1] = 0;
	strcat(path, dir);

	return path;
}

short isDir(char *dir){
	if(opendir(dir) == NULL){
		return (-1);
	}
	return (0);
}

short isFileOk(char *fn){
	struct stat st;
	if(stat(fn, &st) == -1){
		return (-1);
	}
	return (0);
}

int readUntil(char *buf, int fd, char sep){
	char c = 0;
	while((read(fd, &c, 1) > 0) && (c != sep)){
		(*buf++) = c;
	}
	if(c == sep){
		*buf = 0;
		return (0);
	}
	else if (c == 0) {
		return (1);
	}
	return (-1);
}

int lookupUser(char *path, char *username, char *passwd){
	char user[64], pswd[64];
	user[0] = pswd[0] = 0;
	int fd, res = 1;
	if((fd = open(path, O_RDONLY)) == -1 )
		return (-1);
	while((readUntil(user, fd, ':')) != -1){
		if(readUntil(pswd, fd, '\n') == -1){
			res = 1;
			break;
		}
		if(strcmp(username, user) == 0){
			res = 0;
			break;
		}
	}
	if (pswd[0] == 0){
		return (1);
	}
	if(passwd == NULL) return (res);
	strcpy(passwd, pswd);
	return (res);
}

int getFullPath(char *fpath, struct state *cstate, struct config *configuration, char *dirname){
	// if the directory is determined by absolute path
	if(dirname[0] == '/'){
		if (strcpy(fpath, configuration->root_dir) == NULL)
			return (-1);
		if (strcat(fpath, dirname) == NULL)
			return (-1);
		return (0);
	}
	if (strcpy(fpath, configuration->root_dir) == NULL)
		return (-1);
	if (strcat(fpath, cstate->path) == NULL)
		return (-1);
	if ((fpath = changeDir(fpath, dirname)) == NULL)
		return (-1);
	REP(fpath);
	return (0);
}

int getHostIp(char *ip, struct in_addr *addr){

	struct ifaddrs *ifAddrStruct;
    struct ifaddrs *ifa;
    void *tmpAddrPtr;
    char msg[128];

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
            if(strcmp(ip, "127.0.0.1") != 0){
            	sprintf(msg, "Using interface %s", ifa->ifa_name);
            	REP(msg);
            	break;
            }
        }
            // printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer); 
        // } else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
        //     // is a valid IP6 Address
        //     tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
        //     char addressBuffer[INET6_ADDRSTRLEN];
        //     inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
        //     printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer); 
        // } 
    }
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    else return (-1);

    *addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
	return (0);
}

short spawnConnection(struct state *cstate, int *accepted){
	*accepted = 0;
	char str[INET_ADDRSTRLEN];


// now get it back and print it
	inet_ntop(AF_INET, &(cstate->client_addr.sin_addr), str, INET_ADDRSTRLEN);
	REP(str);
	if(!cstate->data_sock){
		if ((*accepted = socket(AF_INET, SOCK_STREAM, 6)) == -1) {
			perror("Error creating data socket.");
			return (-1);
		}
		if ((connect(*accepted, (struct sockaddr *)&(cstate->client_addr), sizeof(cstate->client_addr))) == -1) {
			perror("Error creating data socket.");
			return (-1);
		}
		REP("Connected!");
	}else{
		if ((*accepted = accept(cstate->data_sock, NULL, 0)) == -1){
			perror("Error accepting connection on data socket.");
			return (-1);
		}
	}
	if(!(*accepted)) return (-1);
	if(fcntl(*accepted, F_SETFL, fcntl(*accepted, F_GETFL) | O_NONBLOCK) == -1){
		perror("Error accepting connection.");
		return (-1);
	}
	cstate->last_accepted = *accepted;
	return (0);
}