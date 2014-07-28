#include "../headers/structures.h"
#include "../headers/common.h"
#include "../headers/commands.h"
#include "../headers/conf.h"
#include "../headers/core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <strings.h> // bzero
#include <err.h>
#include <errno.h>
#include <stropts.h>

void initCmd(struct cmd *command, struct cmd_function *f) {
	struct cmd_function *func;
	for (func = f; func != NULL; func = func->next) {
		if (strcasecmp(command->name, func->name) == 0) {
			command->func = func->func;
			break;
		}
		else
			command->func = NULL;
	}
}

int respond(int fd, int fst, int snd, int thd, char *response){
	char code[4];
	code[0] = '0' + fst;
	code[1] = '0' + snd;
	code[2] = '0' + thd;
	code[3] = ' ';
	if (write(fd, code, 4) == -1) return (-1);
	if (write(fd, response, strlen(response)) == -1) return (-1);
	if (write(fd, "\r\n", 2) == -1) return (-1);
	return (0);
}

void user(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Require username.");
		return;
	}
	printf("User: %s\n", params[0]);
	switch (lookupUser(configuration->user_db, params[0], NULL)){
		case -1:
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		break;
		case 0:
			printf("%s\n", "ok");
			respond(fd, 3, 3, 1, "OK, awaiting password.");
		break;
		case 1:
			respond(fd, 4, 3, 0, "Unknown user.");
			return;
		break;
	}
	struct cmd psswd;
	readCmd(fd, &psswd);
	if (strcasecmp(psswd.name, "PASS") != 0){
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	strcpy(cstate->user, params[0]);
	(*cstate->path) = '/';
	//(*(cstate->path + 1)) = 0;
	//strcpy(cstate->path + 1, params[0]);
	cstate->logged = 0;
	executeCmd(&psswd, NULL, fd, cstate, configuration);
}

void paswd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(cstate->logged){
		respond(fd, 5, 0, 3, "User already logged in.");
		return;
	}
	if (cstate->user == NULL){
		respond(fd, 5, 0, 3, "Need username before password.");
		return;
	}
	char *correct_passwd = (char *) malloc(256 * sizeof (char));
	if (lookupUser(configuration->user_db, cstate->user, correct_passwd) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(correct_passwd);
		return;
	}
	if (strcmp(params[0], correct_passwd) == 0){
		respond(fd, 2, 3, 0, "User logged in.");
		cstate->logged = 1;
		free(correct_passwd);
		return;
	}
	char *str = (char *) malloc(256 * sizeof (char));
	sprintf(str, "User %s failed to login: Wrong password.", cstate->user);
	logReport(str);
	free(str);
	respond(fd, 4, 0, 0, "Wrong password.");
	free(correct_passwd);
}

void quit(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 1, "Closing connection.");
	*abor = 1;
}

void pwd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	respond(fd, 2, 5, 7, cstate->path);
}

void cwd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char *old_path = (char *) malloc (256 * sizeof (char));
	char *dir = (char *) malloc (256 * sizeof (char));
	strcpy(old_path, cstate->path);
	if(getFullPath(dir, cstate, configuration, params[0]) == -1){
		free(old_path);
		free(dir);
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	if(isDir(dir) == -1){
		free(old_path);
		free(dir);
		respond(fd, 4, 5, 1, "Directory does not exist.");
		return;
	}
	free(dir);
	if (changeDir(cstate->path, params[0]) == NULL){
		strcpy(cstate->path, old_path);
		free(old_path);
		respond(fd, 4, 5, 1, "Internal server error.");	
		return;
	}
	free(old_path);
	respond(fd, 2, 5, 7, cstate->path);
}

void mkd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char *fpath = (char *) malloc (256 * sizeof (char));
	if(getFullPath(fpath, cstate, configuration, params[0]) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;
	}
	if(mkdir(fpath, 0755) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;	
	}
	char *response = (char *) malloc (256 * sizeof (char));
	sprintf(response, "Directory %s created.", fpath);
	respond(fd, 2, 5, 7, response);
	free(response);
	free(fpath);
}

void rmd(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char *fpath = (char *) malloc (256 * sizeof (char));
	if(getFullPath(fpath, cstate, configuration, params[0]) == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;
	}
	printf(fpath);
	if(rmdir("") == -1){
		respond(fd, 4, 5, 1, "Internal server error.");
		free(fpath);
		return;	
	}
	char *response = (char *) malloc (256 * sizeof (char));
	sprintf(response, "Directory %s removed.", fpath);
	respond(fd, 2, 5, 0, response);
	free(response);
	free(fpath);
}

void cdup(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->logged){
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	free(params[0]);
	params[0] = (char *) malloc (3 * sizeof(char));
	params[0][0] = '.';
	params[0][1] = '.';
	params[0][2] = 0;
	cwd(params, abor, fd, cstate, configuration);
}

void syst(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 5, "UNIX Type: L8");
}

void type(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 0, "OK.");
}

void feat(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 1, "Sorry.");
	respond(fd, 2, 1, 1, "End.");
}

void noop(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 0, "OK.");
}

void abor(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if(!cstate->last_accepted){
		respond(fd, 2, 2, 6, "OK.");
		return;
	}
	close(cstate->last_accepted);
	cstate->last_accepted = 0;
	respond(fd, 4, 2, 6, "Canceled.");
	respond(fd, 2, 2, 6, "OK.");

}

void port(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "Need address and port.");
		return;
	}
	char *ip_str = (char *) malloc (256 * sizeof (char));
	char *port_str = (char *) malloc (256 * sizeof (char));
	char *port_upper = (char *) malloc (4 * sizeof (char));
	char *port_lower = (char *) malloc (4 * sizeof (char));
	char *comma, *port_st;
	struct sockaddr_in *sa = (struct sockaddr_in *) malloc(sizeof (struct sockaddr_in));
	bzero(sa, sizeof (*sa));
	int size, port_no, pts = 0;
	while((comma = strchr(params[0], ',')) != NULL){
		if (++pts == 4){
			size = comma - params[0];
			port_st = comma + 1;
			break;
		}
		*comma = '.';
	}
	strncpy(ip_str, params[0], size);
	strncpy(port_str, port_st, strlen(params[0]) - size);
	comma = strchr(port_str, ',');
	strncpy(port_upper, port_str, comma - port_str);
	strncpy(port_lower, comma + 1, strlen(port_str) - (comma - port_str));
	port_no = 0;
	port_no = (atoi(port_upper) * 256) + atoi(port_lower);
	printf("Port: %d\n", port_no);
	if(inet_pton(AF_INET, ip_str, &(sa->sin_addr)) == -1){
		respond(fd, 5, 0, 1, "Invalid address.");
		return;
	} 
	sa->sin_port = htons(port_no);
	sa->sin_family = AF_INET;
	cstate->client_addr = *sa;
	cstate->port = 1;
	if(!cstate->control_sock){
		spawnDataRoutine(cstate, configuration, &(cstate->control_sock));
	}
	respond(fd, 2, 0, 0, "Port changed.");
}

void list(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int retcode;
	if(!cstate->data_sock && !cstate->port){
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	++cstate->transfer_count;
	write(cstate->control_sock, "DLIST\0", 6);
	write(cstate->control_sock, cstate->path, strlen(cstate->path));
	write(cstate->control_sock, "\0", 1);

	respond(fd, 1, 5, 0, "Ok, sending listing of the directory.");
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6, "Directory listing succcessful."); break;
		case 5:
			respond(fd, 5, 5, 1, "File transfer aborted."); break;
	}

}

void pasv(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int sock, i = 0;
	char c, buf[64], msg[128];
	if(!cstate->control_sock){
		spawnDataRoutine(cstate, configuration, &sock);
		cstate->control_sock = sock;
	}
	write(cstate->control_sock, "DPASV\0", 6);
	while(read(cstate->control_sock, &c, 1) > 0){
		if (c == '.')
			buf[i++] = ',';
		else
			buf[i++] = c;
		if(c == 0) break;
	}
	sprintf(msg, "OK, entering passive mode (%s).", buf);
	respond(fd, 2, 2, 7, msg);	
}

void d_pasv(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int newsock, port, optval = 1;
	struct sockaddr_in in;
	bzero(&in, sizeof (in));
	char ip[INET_ADDRSTRLEN], msg[64];

	port = 16384;
	in.sin_family = AF_INET;
	getHostIp(ip, &(in.sin_addr));
	in.sin_port = htons(port);
	// printf("%d\n", cstate->data_sock);
	if(cstate->data_sock){
		close(cstate->data_sock);
		cstate->data_sock = 0;
	}
	if ((newsock = socket(AF_INET, SOCK_STREAM, 6)) == -1) {
		perror("Error creating data socket.");
		return;
	}
	if (setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1)
		err(1, "Problem occured while creating the socket.");
	
	if(fcntl(newsock, F_SETFL, fcntl(newsock, F_GETFL) | O_NONBLOCK) == -1){
		perror("Error creating data socket.");
		return;
	}
	while((bind(newsock, (struct sockaddr *) &in, sizeof (in)) == -1) && (errno == EADDRINUSE)){
		++port;
		in.sin_port = htons(port);
		errno = 0;
	}
	if (errno){
		perror("Error binding control socket.");
		return;
	}
	if (listen(newsock, SOMAXCONN) == -1){
		perror("Error listening on control socket.");
		return;
	}
	sprintf(msg, "%s,%d,%d", ip, (port / 256), (port % 256));
	write(fd, msg, strlen(msg) + 1);
	cstate->data_sock = newsock;
	REP("Awaiting connection...");
}

void d_list(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int accepted, retcode;
	spawnConnection(cstate, &accepted);
	// ioctl(accepted, I_FLUSH, FLUSHRW);
	pid_t pid;
	int pd[2], r;
	pipe(pd);
	char buf[256], dir[32], path[128];
	readUntil(dir, fd, 0);
	sprintf(path, "%s%s", configuration->root_dir, dir);
	REP(path);
	switch(pid = fork()){
		case 0:
			close(1);
			close(pd[0]);
			dup(pd[1]);
			execlp("ls", "ls", "-l", path, NULL);
		break;
		default:
			close(pd[1]);
			while((r = readUntil(buf, pd[0], '\n')) != -1){
				write(accepted, buf, strlen(buf));
				REP(buf);
				write(accepted, "\n", 1);
				if (r == 1) break;
			}
			close(pd[0]);
		break;
	}
	REP("Closing");
	close(accepted);
	retcode = 2;
	write(fd, &retcode, sizeof (int));
	//cstate->data_sock = 0;
}

void retr(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	// char filepath[128];
	char *filepath = (char *) malloc(128*sizeof(char));
	int retcode;
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "No filename given.");
		return;
	}
	if(!cstate->logged) {
		respond(fd, 5, 3, 0, "No logged in.");
	}
	if(!cstate->data_sock && cstate->port == 0){
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	getFullPath(filepath, cstate, configuration, params[0]);
	write(cstate->control_sock, "DRETR\0", 6);
	write(cstate->control_sock, filepath, strlen(filepath));
	write(cstate->control_sock, "\0", 1);

	respond(fd, 1, 5, 0, "Ok, about to open data connection.");
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6, "File succcessfuly transfered."); break;
		case 5:
			respond(fd, 5, 5, 1, "File transfer aborted."); break;
	}
}

void d_retr(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int accepted, retcode, file, r;
	char filepath[128], buf[BUFSIZE];
	spawnConnection(cstate, &accepted);
	readUntil(filepath, fd, 0);
	if (isFileOk(filepath) == -1){
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	REP("accepted");
	REP(filepath);
	if ((file = open(filepath, O_RDONLY)) == -1){
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	while((r = read(file, buf, BUFSIZE)) > 0){
		usleep(500000);
		if (write(accepted, buf, r) == -1) {
			retcode = 5;
			write(fd, &retcode, sizeof (int));
			close(accepted);
			break;
		}
	}
	close(file);
	close(accepted);
	retcode = 2;
	write(fd, &retcode, sizeof (int));

}

void stor(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	// char filepath[128];
	char filepath[256];
	int retcode;
	if (params[0] == NULL){
		respond(fd, 5, 0, 4, "No filename given.");
		return;
	}
	if(!cstate->logged) {
		respond(fd, 5, 3, 0, "No logged in.");
	}
	if(!cstate->data_sock && cstate->port == 0){
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	getFullPath(filepath, cstate, configuration, params[0]);
	write(cstate->control_sock, "DSTOR\0", 6);
	write(cstate->control_sock, filepath, strlen(filepath));
	write(cstate->control_sock, "\0", 1);

	respond(fd, 1, 5, 0, "Ok, about to open data connection.");
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6, "File succcessfuly transfered."); break;
		case 5:
			respond(fd, 5, 5, 1, "File transfer aborted."); break;
	}
}

void d_stor(char **params, short *abor, int fd, struct state *cstate, struct config *configuration) {
	int accepted, retcode, file, r;
	char filepath[128], buf[BUFSIZE];
	accepted = 0;
	spawnConnection(cstate, &accepted);
	REP("Spawned!");
	readUntil(filepath, fd, 0);
	// if (isFileOk(filepath) == -1){
	// 	retcode = 5;
	// 	write(fd, &retcode, sizeof (int));
	// 	return;
	// }
	if ((file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0700)) == -1){
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	REP("accepted");
	while((r = read(accepted, buf, BUFSIZE)) > 0){
		printf("%d\n", r);
		if (write(file, buf, r) == -1) {
			retcode = 5;
			write(fd, &retcode, sizeof (int));
			close(accepted);
			break;
		}
	}
	REP("After...");
	close(file);
	retcode = 2;
	write(fd, &retcode, sizeof (int));
	close(accepted);
}