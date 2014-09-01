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

// iterate through command functions
// and assigns the apropriate function to the given command
void initCmd(struct cmd *command,
	const struct cmd_function all_commands[], int cmd_length) {
	int i;
	for (i = 0; i < cmd_length; ++i) {
		if (strcasecmp(command->name, all_commands[i].name) == 0) {
			command->func = all_commands[i].func;
			break;
		}
		else
			command->func = NULL;
	}
}

// writes a response consisting of
// three-digit code and the given text to the fd.
int respond(int fd, int fst, int snd, int thd, char *response) {
	char code[4];
	code[0] = '0' + fst;
	code[1] = '0' + snd;
	code[2] = '0' + thd;
	code[3] = ' ';
	if (write(fd, code, 4) == -1)
		return (-1);
	if (write(fd, response, strlen(response)) == -1)
		return (-1);
	if (write(fd, "\r\n", 2) == -1)
		return (-1);
	return (0);
}

// interprets the user command;
// try to find the username in the database,
// forces the next command to be PASS
void user(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Require username.");
		return;
	}
	switch (lookupUser(configuration->user_db, params[0], NULL, 0)) {
		// some error occured while reading the database
		case -1:
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		break;
		// user exists
		case 0:
			respond(fd, 3, 3, 1, "OK, awaiting password.");
		break;
		// user does not exist
		case 1:
			respond(fd, 4, 3, 0, "Unknown user.");
			return;
		break;
	}
	// waits for the next command and check if it is PASS
	struct cmd psswd;
	readCmd(fd, &psswd);
	if (strcasecmp(psswd.name, "PASS") != 0) {
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	// sets the username and initial path
	strcpy(cstate->user, params[0]);
	(*cstate->path) = '/';
	(*(cstate->path + 1)) = 0;
	cstate->logged = 0;
	executeCmd(&psswd, NULL, fd, cstate, configuration);
}

// checks whether the combination
// of the username and password is correct
void paswd(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (cstate->logged) {
		respond(fd, 5, 0, 3, "User already logged in.");
		return;
	}
	// wrong order
	if (cstate->user == NULL) {
		respond(fd, 5, 0, 3, "Need username before password.");
		return;
	}
	char correct_passwd[USER_LENGTH];
	// error occured...
	if (lookupUser(configuration->user_db,
		cstate->user, correct_passwd, USER_LENGTH) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	// password is OK
	if (strcmp(params[0], correct_passwd) == 0) {
		respond(fd, 2, 3, 0, "User logged in.");
		cstate->logged = 1;
		return;
	}
	// wrong password
	char str[USER_LENGTH];
	snprintf(str, USER_LENGTH,
		"User %s failed to login: Wrong password.", cstate->user);
	respond(fd, 4, 0, 0, "Wrong password.");
}

// disconnects from the server
void quit(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	pthread_detach(cstate->data_thread);
	respond(fd, 2, 2, 1, "Closing connection.");
	*abor = 1;
}

// prints the current working directory
void pwd(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	respond(fd, 2, 5, 7, cstate->path);
}

// changes the working directory
void cwd(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}

	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}

	char old_path[PATH_LENGTH];
	char dir[PATH_LENGTH];
	strncpy(old_path, cstate->path, PATH_LENGTH - 1);
	old_path[PATH_LENGTH - 1] = '\0';
	// loads the full path to the given directory into dir variable
	if (getFullPath(dir, cstate, configuration, params[0]) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	// checks the directory existence
	if (isDir(dir) == -1) {
		respond(fd, 4, 5, 1, "Directory does not exist.");
		return;
	}
	// changes the "chrooted" part of path
	if (changeDir(cstate->path, params[0]) == NULL) {
		strcpy(cstate->path, old_path);
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	respond(fd, 2, 5, 7, cstate->path);
}

// creates new direcotry
void mkd(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}
	char fpath[PATH_LENGTH];
	char response[STR_LENGTH];
	// loads the full path to the given directory into fpath variable
	if (getFullPath(fpath, cstate, configuration, params[0]) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	// creates the directory
	if (mkdir(fpath, 0755) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	snprintf(response, STR_LENGTH, "Directory %s created.", fpath);
	respond(fd, 2, 5, 7, response);
}

// removes the given directory
void rmd(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need directory name.");
		return;
	}

	char fpath[PATH_LENGTH];
	char response[STR_LENGTH];

	// loads the full path to the given directory into fpath variable
	if (getFullPath(fpath, cstate, configuration, params[0]) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	if (rmrDir("") == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	snprintf(response, STR_LENGTH, "Directory %s removed.", fpath);
	respond(fd, 2, 5, 0, response);
}

// deletes the given file
void dele(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need file name.");
		return;
	}

	char fpath[PATH_LENGTH];
	char response[STR_LENGTH];

	// loads the full path to the given file into fpath variable
	if (getFullPath(fpath, cstate, configuration, params[0]) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	if (unlink(fpath) == -1) {
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	snprintf(response, STR_LENGTH, "File %s removed.", fpath);
	respond(fd, 2, 5, 0, response);
}

// changes the working directory one level up by invoking the cwd command
void cdup(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	char old_path[PATH_LENGTH];
	strcpy(old_path, cstate->path);
	// checks the directory existence
	// changes the "chrooted" part of path
	if (changeDir(cstate->path, "..") == NULL) {
		strcpy(cstate->path, old_path);
		respond(fd, 4, 5, 1, "Internal server error.");
		return;
	}
	respond(fd, 2, 5, 7, cstate->path);
}

// system info
void syst(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 5, "UNIX Type: L8");
}

// setting transfer type
void type(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need type.");
		return;
	}
	if (params[0][0] == 'I')
		cstate->transfer_type = Image;
	if (params[0][0] == 'A')
		cstate->transfer_type = ASCII;
	respond(fd, 2, 2, 0, "OK. Type changed");
}

// should give information about unusual commands supported by the server
void feat(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	respond(fd, 2, 1, 1, "Sorry.");
	write(fd, "NONE", 4);
	respond(fd, 2, 1, 1, "end");
}

// no operation; only responds
void noop(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	respond(fd, 2, 2, 0, "OK.");
}

// aborts the previously spawned data transfer
void abor(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (!cstate->last_accepted) {
		respond(fd, 2, 2, 6, "OK.");
		return;
	}
	close(cstate->last_accepted);
	cstate->last_accepted = 0;
	respond(fd, 4, 2, 6, "Canceled.");
	respond(fd, 2, 2, 6, "OK.");

}

// sets the communication port and IP adress server should connect to
void port(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need address and port.");
		return;
	}
	char ip_str[STR_LENGTH];
	char port_str[STR_LENGTH];
	char port_upper[4];
	char port_lower[4];
	char *comma, *port_st = NULL;
	struct sockaddr_in *in = (struct sockaddr_in *) &(cstate->client_addr);
	bzero(&(cstate->client_addr), sizeof (cstate->client_addr));
	int port_no, pts = 0, size = 0;

	// converts the commas in the given address to dots
	while ((comma = strchr(params[0], ',')) != NULL) {
		// read 4 delimited strings; port now starts at
		// port_st; size equals the length of the IP adrress
		if (++pts == 4) {
			size = comma - params[0];
			port_st = comma + 1;
			break;
		}
		*comma = '.';
	}
	if ((!size) || (port_st == NULL)) {
		respond(fd, 5, 0, 1, "Invalid data supplied.");
		return;
	}
	// copies the IP address and port
	strncpy(ip_str, params[0], size);
	strncpy(port_str, port_st, strlen(params[0]) - size);
	comma = strchr(port_str, ',');
	// converts the port number
	strncpy(port_upper, port_str, comma - port_str);
	strncpy(port_lower, comma + 1, strlen(port_str) - (comma - port_str));
	port_no = (atoi(port_upper) * 256) + atoi(port_lower);
	// converts the IP address
	if (inet_pton(AF_INET, ip_str, &(in->sin_addr)) == -1) {
		respond(fd, 5, 0, 1, "Invalid address.");
		return;
	}
	// sets proper values and saves it in the cstate structure
	in->sin_port = htons(port_no);
	in->sin_family = AF_INET;
	cstate->port = 1;
	if (cstate->control_sock) {
		write(cstate->control_sock, "Q\0", 2);
		cstate->control_sock = 0;
	}
	cstate->addr_family = 1;
	respond(fd, 2, 0, 0, "Port changed.");
}

// sets the communication port and IP adress
// server should connect to - extended version
void eprt(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "Need address and port.");
		return;
	}
	char ip_str[STR_LENGTH], buf[STR_LENGTH];
	char *del, *n_del, del_char;
	bzero(&(cstate->client_addr), sizeof (cstate->client_addr));
	int port, family;

	// parse the parameter
	del_char = params[0][0];
	del = strchr(params[0], del_char);
	strncpy(buf, del + 1, 1);
	buf[1] = '\0';
	family = atoi(buf);
	del = strchr(del + 1, del_char);
	n_del = strchr(del + 1, del_char);
	strncpy(ip_str, del + 1, n_del - del - 1);
	ip_str[n_del - del - 1] = '\0';
	del = n_del;
	n_del = strchr(del + 1, del_char);
	strncpy(buf, del + 1, n_del - del - 1);
	buf[n_del - del - 1] = '\0';
	port = atoi(buf);
	if (family == 1) { // IPv4
		struct sockaddr_in *in = (struct sockaddr_in *)
		&(cstate->client_addr);
		// converts the IP address
		if (inet_pton(AF_INET, ip_str, &(in->sin_addr)) == -1) {
			respond(fd, 5, 0, 1, "Invalid address.");
			return;
		}
		// sets proper values and saves it in the cstate structure
		in->sin_port = htons(port);
		in->sin_family = AF_INET;
	} else if (family == 2) { // IPv6
		struct sockaddr_in6 *in = (struct sockaddr_in6 *)
		&(cstate->client_addr);
		// converts the IP address
		if (inet_pton(AF_INET6, ip_str, &(in->sin6_addr)) == -1) {
			respond(fd, 5, 0, 1, "Invalid address.");
			return;
		}
		// sets proper values and saves it in the cstate structure
		in->sin6_port = htons(port);
		in->sin6_family = AF_INET6;
	}
	if (cstate->control_sock) {
		write(cstate->control_sock, "Q\0", 2);
		cstate->control_sock = 0;
	}
	cstate->port = 1;
	cstate->addr_family = family;
	respond(fd, 2, 0, 0, "Port changed.");
}

// sends the listing of the current directory
// through data connection - client part
void list(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int retcode;
	// connection was not initialized yet
	++cstate->transfer_count;
	if (!cstate->control_sock) {
		if (spawnDataRoutine(cstate, configuration,
			&(cstate->control_sock)) == -1) {
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		}
	}
	if (!cstate->data_sock && !cstate->port) {
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	// invokes the DLIST command; gives the path
	write(cstate->control_sock, "DLIST\0", 6);
	write(cstate->control_sock, cstate->path, strlen(cstate->path));
	write(cstate->control_sock, "\0", 1);

	respond(fd, 1, 5, 0, "Ok, sending listing of the directory.");
	// provides info about the result
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6,
				"Directory listing succcessful.");
			break;
		case 5:
			respond(fd, 5, 5, 1,
				"File transfer aborted.");
			break;
	}

}

// sends the listing of the directory - data part
void d_list(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int accepted, retcode, subn, r;
	char buf[BUFSIZE], dir[32], path[128], tmp[2 * BUFSIZE];

	// accept connection / connect to the client
	if (spawnConnection(cstate, &accepted)) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	// read path to dir from the control thread
	readUntil(dir, fd, 0, 32);
	// join the path
	if (getFullPath(path, cstate, configuration, dir) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	// loads the listing of the directory
	// determined by path to string pointed to by buf
	listDir(path, buf);
	// sends the list in ASCII mode
	r = strlen(buf);
	// converts \n to \r\n sequence,
	// return how much is the result longer
	if ((subn = im2as(tmp, buf, BUFSIZE)) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	r += subn;
	memcpy(buf, tmp, BUFSIZE * 2);
	if (write(accepted, buf, r) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	close(accepted);
	// report the result
	retcode = 2;
	write(fd, &retcode, sizeof (int));
}

// sets the passive mode - client part
void pasv(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int resp, i = 0;
	char c, buf[64], msg[STR_LENGTH];

	if (cstate->control_sock && cstate->port) {
		// terminate the spawned thread for active transfer
		write(cstate->control_sock, "Q\0", 2);
		cstate->control_sock = 0;
		cstate->port = 0;
	}
	// data transfer process was not spawned yet
	if (!cstate->control_sock) {
		if (spawnDataRoutine(cstate, configuration,
			&(cstate->control_sock)) == -1) {
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		}
	}
	// invokes the DPASV command
	write(cstate->control_sock, "DPASV\0", 6);
	write(cstate->control_sock, &fd, sizeof (int));
	read(cstate->control_sock, &resp, sizeof (int));
	if (resp == 5) {
		respond(fd, 5, 0, 1, "Internal server error");
		return;
	}
	// reads the port and IP adress and port
	// the data transfer process stated to listen on
	while (read(cstate->control_sock, &c, 1) > 0) {
		if (c == '.')
			buf[i++] = ',';
		else
			buf[i++] = c;
		if (c == 0)
			break;
	}
	// informs the client
	snprintf(msg, STR_LENGTH, "OK, entering passive mode (%s).", buf);
	respond(fd, 2, 2, 7, msg);
}

void epsv(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int port = 0;
	char msg[STR_LENGTH];

	if (cstate->control_sock && cstate->port) {
		// terminate the spawned thread for active transfer
		write(cstate->control_sock, "Q\0", 2);
		cstate->control_sock = 0;
		cstate->port = 0;
	}
	// data transfer process was not spawned yet
	if (!cstate->control_sock) {
		if (spawnDataRoutine(cstate, configuration,
			&(cstate->control_sock)) == -1) {
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		}
	}
	// invokes the DPASV command
	write(cstate->control_sock, "DEPSV\0", 6);
	// reads the port and IP adress and port
	// the data transfer process stated to listen on
	read(cstate->control_sock, &port, sizeof (int));
	if (port == 5) {
		respond(fd, 5, 0, 1, "Internal server error");
		return;
	}
	read(cstate->control_sock, &port, sizeof (int));
	// informs the client
	snprintf(msg, STR_LENGTH,
		"OK, entering extended passive mode (|||%d|).", port);
	respond(fd, 2, 2, 9, msg);
}

// sets the passive mode - data part
void d_pasv(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int csockfd, newsock, port, retcode, optval = 1;
	struct sockaddr_in in;
	bzero(&in, sizeof (in));
	char ip[INET_ADDRSTRLEN], msg[STR_LENGTH];
	// default new port
	port = 30000;
	in.sin_family = AF_INET;
	// gets the IP adress the OS provides
	read(fd, &csockfd, sizeof (int));
	if (getHostIp(ip, csockfd) == -1)
		return;
	if (inet_pton(AF_INET, ip, &(in.sin_addr)) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	in.sin_port = htons(port);
	// server already listens - abort
	if (cstate->data_sock) {
		close(cstate->data_sock);
		// shutdown(cstate->data_sock, SHUT_RDWR);
		cstate->data_sock = 0;
	}
	// create new socket
	if ((newsock = socket(AF_INET, SOCK_STREAM, 6)) == -1) {
		perror("Error creating data socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	if (setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1) {
		perror("Problem occured while creating the socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	errno = 0;
	// tries to bind to different ports
	while ((bind(newsock, (struct sockaddr *) &in,
		sizeof (in)) == -1) && (errno == EADDRINUSE)) {
		if (++port > 32768)
			break;
		in.sin_port = htons(port);
		errno = 0;
	}
	if (errno) {
		perror("Error binding data socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	if (listen(newsock, SOMAXCONN) == -1) {
		perror("Error listening on control socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	// writes info about new IP address and socket to the control thread
	snprintf(msg, STR_LENGTH, "%s,%d,%d", ip, (port / 256), (port % 256));
	retcode = 2;
	write(fd, &retcode, sizeof (int));
	write(fd, msg, strlen(msg) + 1);
	cstate->data_sock = newsock;
	cstate->addr_family = 1;
}

void d_epsv(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int newsock, port, retcode, optval = 1;
	struct sockaddr_in6 in;
	bzero(&in, sizeof (in));

	// default new port
	port = 30000;
	in.sin6_family = AF_INET6;
	// listens on wildcard socket, suitable for IPv6
	bzero(&in.sin6_addr.s6_addr, 16);
	in.sin6_port = htons(port);
	// server already listens - abort
	if (cstate->data_sock) {
		close(cstate->data_sock);
		cstate->data_sock = 0;
	}
	// create new socket
	if ((newsock = socket(AF_INET6, SOCK_STREAM, 6)) == -1) {
		perror("Error creating data socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	if (setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof (optval)) == -1) {
		perror("Problem occured while creating the socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	errno = 0;
	// tries to bind to different ports
	while ((bind(newsock, (struct sockaddr *) &in,
		sizeof (in)) == -1) && (errno == EADDRINUSE)) {
		if (++port > 32768)
			break;
		in.sin6_port = htons(port);
		errno = 0;
	}
	if (errno) {
		perror("Error binding data socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	if (listen(newsock, SOMAXCONN) == -1) {
		perror("Error listening on data socket.");
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(newsock);
		return;
	}
	// writes info about new IP address and socket to the control thread
	retcode = 2;
	write(fd, &retcode, sizeof (int));
	write(fd, &port, sizeof (int));
	cstate->data_sock = newsock;
	cstate->addr_family = 2;
}

// retrieves the data from the server - control part
void retr(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	char filepath[PATH_LENGTH];
	int retcode;
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "No filename given.");
		return;
	}
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (!cstate->control_sock) {
		if (spawnDataRoutine(cstate, configuration,
			&(cstate->control_sock)) == -1) {
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		}
	}
	if (!cstate->data_sock && cstate->port == 0) {
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	// loads the full path to the given
	// directory into the filepath variable
	getFullPath(filepath, cstate, configuration, params[0]);
	// invokes the DRETR command
	write(cstate->control_sock, "DRETR\0", 6);
	write(cstate->control_sock, filepath, strlen(filepath));
	write(cstate->control_sock, "\0", 1);

	// gives information to the client
	respond(fd, 1, 5, 0, "Ok, about to open data connection.");
	// reads and reports the result
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6, "File succcessfuly transfered.");
			break;
		case 5:
			respond(fd, 5, 5, 1, "File transfer aborted.");
			break;
	}
}

// retrieves the data from the server - control path
void d_retr(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int accepted, retcode, file, r, subn = 0;
	char filepath[PATH_LENGTH], buf[2 * BUFSIZE];
	// accepts / connects to the client
	if (spawnConnection(cstate, &accepted)) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	// loads filepath of the given file
	readUntil(filepath, fd, 0, PATH_LENGTH);
	// check the file status
	if (isFileOk(filepath) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	// opens the file
	if ((file = open(filepath, O_RDONLY)) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	// read blocks of data from the file
	// and sends it through the data connection
	while ((r = read(file, buf, BUFSIZE)) > 0) {
		// ASCII type transfer
		if (cstate->transfer_type == ASCII) {
			char tmp[2 * BUFSIZE];
			// converts \n to \r\n sequence,
			// return how much is the result longer
			if ((subn = im2as(tmp, buf, BUFSIZE)) == -1) {
				retcode = 5;
				write(fd, &retcode, sizeof (int));
				close(accepted);
				break;
			}
			r += subn;
			memcpy(buf, tmp, BUFSIZE * 2);
		}
		if (write(accepted, buf, r) == -1) {
			retcode = 5;
			write(fd, &retcode, sizeof (int));
			close(accepted);
			break;
		}
	}
	// closes the connection
	close(file);
	close(accepted);
	// reads and reports the result
	retcode = 2;
	write(fd, &retcode, sizeof (int));

}

// saves the received file - control part
void stor(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	char filepath[PATH_LENGTH], fn[PATH_LENGTH];
	int retcode;
	if (params[0] == NULL) {
		respond(fd, 5, 0, 4, "No filename given.");
		return;
	}
	if (!cstate->logged) {
		respond(fd, 5, 3, 0, "Not logged in.");
		return;
	}
	if (!cstate->control_sock) {
		if (spawnDataRoutine(cstate, configuration,
			&(cstate->control_sock)) == -1) {
			respond(fd, 4, 5, 1, "Internal server error.");
			return;
		}
	}
	if (!cstate->data_sock && cstate->port == 0) {
		respond(fd, 5, 0, 3, "Bad command sequence.");
		return;
	}
	// loads the full path to the given
	// directory into the filepath variable
	strcpy(fn, params[0]);
	params++;
	while (*params != NULL) {
		strcat(fn, "_");
		strcat(fn, *(params++));
	}
	getFullPath(filepath, cstate, configuration, fn);
	// invokes the DSTOR command
	write(cstate->control_sock, "DSTOR\0", 6);
	write(cstate->control_sock, filepath, strlen(filepath));
	write(cstate->control_sock, "\0", 1);

	// informs the client
	respond(fd, 1, 5, 0, "Ok, about to open data connection.");
	// reads and reports the result
	read(cstate->control_sock, &retcode, sizeof (int));
	switch (retcode) {
		case 2:
			respond(fd, 2, 2, 6, "File succcessfuly transfered.");
			break;
		case 5:
			respond(fd, 5, 5, 1, "File transfer aborted.");
			break;
	}
}

// strores the received file on the server - data part
void d_stor(char **params, short *abor, int fd,
	struct state *cstate, struct config *configuration) {
	int accepted, retcode, file, r, subn = 0;
	char filepath[PATH_LENGTH], buf[BUFSIZE];
	accepted = 0;
	// accepts/ connects to the client
	if (spawnConnection(cstate, &accepted)) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		return;
	}
	// reads the filepath from the ctrl thread
	readUntil(filepath, fd, 0, PATH_LENGTH);
	// if (isFileOk(filepath) == -1){
	// 	retcode = 5;
	// 	write(fd, &retcode, sizeof (int));
	// 	return;
	// }
	// creates the file
	if ((file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0700)) == -1) {
		retcode = 5;
		write(fd, &retcode, sizeof (int));
		close(accepted);
		return;
	}
	// reads the data and saves it to the file
	while ((r = read(accepted, buf, BUFSIZE)) > 0) {
		// ASCII data transfer
		if (cstate->transfer_type == 6) {
			char tmp[BUFSIZE];
			// converts \r\n sequence to \n,
			// return how much is the result shorter
			if ((subn = as2im(tmp, buf, BUFSIZE)) == -1) {
				retcode = 5;
				write(fd, &retcode, sizeof (int));
				close(accepted);
				break;
			}
			r -= subn;
			memcpy(buf, tmp, BUFSIZE);
		}
		if (write(file, buf, r) == -1) {
			retcode = 5;
			write(fd, &retcode, sizeof (int));
			close(accepted);
			break;
		}
	}
	// closes the file
	close(file);
	// reports result and closes the connection
	retcode = 2;
	close(accepted);
	write(fd, &retcode, sizeof (int));
}