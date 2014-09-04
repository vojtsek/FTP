#include "../headers/common.h"
#include "../headers/structures.h"
#include "../headers/errors.h"
#include "../headers/conf.h"
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
#include <err.h>
#include <time.h>
#define	MAXHOST 256

// read configuaration from the conf.h
// and process parametres, fills the struct configuration
void readConfiguration(struct config *configuration, int argc, char **argv) {
	size_t opt;

	// initialize the configuration using default macros
	configuration->ctrl_port = CTRL_PORT;
	configuration->data_port = DATA_PORT;
	configuration->max_conns = MAX_CONNS;
	configuration->root_dir = (char *) allocate(PATH_LENGTH);
	configuration->user_db = (char *) allocate(PATH_LENGTH);
	strncpy(configuration->root_dir, DATA_DIR, PATH_LENGTH);
	strncpy(configuration->user_db, USER_DB, PATH_LENGTH);

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
data port: %lu\n\
control port: %lu\n\
data directory: %s\n",
(unsigned long) configuration->data_port,
(unsigned long) configuration->ctrl_port,
configuration->root_dir);
}

// reports the message
void logReport(char *msg) {
	fprintf(stdout, "%s\n", msg);
}

// recognizes white characters
short isWhite(char c) {
	return ((c == ' ') || (c == '\n') || (c == '\r') || (c == '\t'));
}

// change the given path to point to the directory
char *changeDir(char *path, char *dir) {
	char *last_slash;
	// step up one directory
	if (strcmp(dir, "..") == 0) {
		last_slash = strrchr(path, '/');
		if ((strcmp(path, "/") == 0) || (last_slash == NULL)) {
		// root dir or invalid path
			return (NULL);
		}
		// shorten the string to last slash -> one dir up
		(*last_slash) = 0;
		return (path);
	}
	// the given path is absolute
	if (dir[0] == '/') {
		if (strncpy(path, dir, PATH_LENGTH - 1) == NULL) {
			return (NULL);
		}
		return (path);
	}
	size_t path_l = strlen(path);
	// checks the length and concatenates the path and the dir
	if ((path_l + strlen(dir) + 1) > PATH_LENGTH)
		return (NULL);
	path[path_l] = '/';
	path[path_l + 1] = 0;
	strncat(path, dir, PATH_LENGTH - 1);

	return (path);
}

// true if dir is pointing to directory name
short isDir(char *dir) {
	if (opendir(dir) == NULL) {
		return (-1);
	}
	return (0);
}

// checks the file existence and status
short isFileOk(char *fn) {
	struct stat st;
	if (stat(fn, &st) == -1) {
		return (-1);
	}
	return (0);
}

// reads from fd until sep character or end appears
int readUntil(char *buf, int fd, char sep, size_t max) {
	char c = 0;
	// read...
	while ((read(fd, &c, 1) > 0) && (c != sep)) {
		(*buf++) = c;
		if (!(--max)) {
			printf("Too long string to read.\n");
			return (-1);
		}
	}
	// terminating zero
	if (c == sep) {
		*buf = 0;
		return (0);
	}
	// end reached
	else if (c == 0) {
		return (1);
	}
	return (-1);
}

// reads the database specified by path and searches for username
// if found, returns nonzero and passwd
int lookupUser(char *path, char *username, char *passwd, size_t max) {
	char user[64], pswd[max];
	user[0] = pswd[0] = 0;
	int fd, res = 1;
	if ((fd = open(path, O_RDONLY)) == -1)
		return (-1);
	// reads username and password
	while ((readUntil(user, fd, ':', 64)) != -1) {
		if (readUntil(pswd, fd, '\n', 64) == -1) {
			res = 1;
			break;
		}
		// match
		if (strcmp(username, user) == 0) {
			res = 0;
			break;
		}
	}
	// nothing found
	if (pswd[0] == 0) {
		return (1);
	}
	// bad parameter provided
	if (passwd == NULL)
		return (res);
	strcpy(passwd, pswd);
	return (res);
}

// loads the full system path to the given file or directory to fpath
int getFullPath(char *fpath, struct state *cstate,
	struct config *configuration, char *dirname) {
	if (strstr(dirname, "..")) {
		return (-1);
	}
	// the name is determined by absolute path
	// only joins root
	if (dirname[0] == '/') {
		if (strncpy(fpath, configuration->root_dir,
			PATH_LENGTH - 1) == NULL)
			return (-1);
		fpath[PATH_LENGTH - 1] = '\0';
		if (strncat(fpath, dirname, PATH_LENGTH - 1) == NULL)
			return (-1);
		return (0);
	}
	// relative path
	// joins root, cwd and dirname
	if (strncpy(fpath, configuration->root_dir, PATH_LENGTH - 1) == NULL)
		return (-1);
	fpath[PATH_LENGTH - 1] = '\0';
	if (strncat(fpath, cstate->path, PATH_LENGTH - 1) == NULL)
		return (-1);
	if ((fpath = changeDir(fpath, dirname)) == NULL)
		return (-1);
	return (0);
}

// gets the local IP address the socket with descriptor sock fd is bounded to
int getHostIp(char *ip, int sockfd) {
	struct sockaddr_in6 in;
	socklen_t size = sizeof (in);
	char buf[STR_LENGTH], *ddots;
	if (getsockname(sockfd, (struct sockaddr *)&in, &size) == -1) {
		perror("Error getting host address.");
		return (-1);
	}
	if (inet_ntop(AF_INET6, &(in.sin6_addr), buf, STR_LENGTH) == NULL) {
		perror("inet_ntop failed to get the local address.");
		return (-1);
	}
	ddots = strrchr(buf, ':');
	strncpy(ip, ddots + 1, INET_ADDRSTRLEN);
	return (0);
}

// tries either connect to current socket(active)
// or accept the connection on local socket (passive)
// loads the file descriptor of
// the created socket in accepted
short spawnConnection(struct state *cstate, int *accepted) {
	*accepted = 0;

	if (cstate->addr_family == 1) {
		struct sockaddr_in *in = (struct sockaddr_in *)
		&(cstate->client_addr);
		char str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(in->sin_addr), str, INET_ADDRSTRLEN);
		if (!cstate->data_sock) {
			if ((*accepted = socket(AF_INET,
				SOCK_STREAM, 6)) == -1) {
				perror("Error creating data socket.");
				return (-1);
			}
			if ((connect(*accepted, (struct sockaddr *) in,
				sizeof (struct sockaddr_in))) == -1) {
				perror("Error connecting to data socket.");
				return (-1);
			}
		} else {
			if ((*accepted = accept(cstate->data_sock,
				NULL, 0)) == -1) {
				perror("Error accepting \
					connection on data socket.");
				return (-1);
			}
		}
	} else if (cstate->addr_family == 2) {
		struct sockaddr_in6 *in = (struct sockaddr_in6 *)
		&(cstate->client_addr);
		char str[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(in->sin6_addr), str, INET6_ADDRSTRLEN);
		if (!cstate->data_sock) {
			if ((*accepted =
				socket(AF_INET6, SOCK_STREAM, 6)) == -1) {
				perror("Error creating data socket.");
				return (-1);
			}
			if ((connect(*accepted, (struct sockaddr *) in,
				sizeof (struct sockaddr_in))) == -1) {
				perror("Error connecting to data socket.");
				return (-1);
			}
		} else {
			if ((*accepted =
				accept(cstate->data_sock, NULL, 0)) == -1) {
				perror("Error accepting\
					connection on data socket.");
				return (-1);
			}
		}
	}
	if (!(*accepted))
		return (-1);
	cstate->last_accepted = *accepted;
	return (0);
}

// substitute LF characters for CRLF sequence in string pointed to by src
// returns number of substitutions
int im2as(char *dest, char *src, int size) {
	int i, j = 0, subn = 0;
	for (i = 0; i < size; ++i) {
		if (src[i] == '\n') {
			dest[j++] = '\r';
			++subn;
		}
		dest[j++] = src[i];
	}
	return (subn);
}

// substitute CRLF sequences for LF characters in string pointed to by src
// returns number of substitutions
int as2im(char *dest, char *src, int size) {
	int i = 0, j = 0, subn = 0;
	while (i < size) {
		if ((src[i] == '\r') && (src[i + 1] == '\n')) {
			dest[j++] = '\n';
			i += 2;
			++subn;
			continue;
		}
		dest[j++] = src[i++];
	}
	return (subn);
}

void loadPerms(short perms, char *r, char *w, char *x) {
	if (perms & 04)
		*r = 'r';
	if (perms & 02)
		*w = 'w';
	if (perms & 01)
		*x = 'x';
}

void *allocate(size_t size) {
	void *ret;
	ret = malloc(size);
	if (ret == NULL) {
		err(1, "Malloc failed.");
	}
	return (ret);
}

int listDir(char *dir, char *result) {
	DIR *d;
	struct dirent *entry = (struct dirent *)
	allocate(sizeof (struct dirent));

	struct stat *finfo = (struct stat *)
	allocate(sizeof (struct stat));
	char line[STR_LENGTH], path[PATH_LENGTH], mode[11], tm[13];


	if ((d = opendir(dir)) == NULL)
		return (-1);
	while ((entry = readdir(d))) {
		if ((strcmp(entry->d_name, ".") == 0) ||
			(strcmp(entry->d_name, "..") == 0))
			continue;
		snprintf(path, PATH_LENGTH, "%s/%s", dir, entry->d_name);
		if (stat(path, finfo))
			continue;
		memset(mode, '-', 10);
		if ((finfo->st_mode & S_IFMT) == S_IFDIR)
			mode[0] = 'd';
		loadPerms((finfo->st_mode & S_IRWXU) >> 6,
			mode + 1, mode + 2, mode + 3);
		loadPerms((finfo->st_mode & S_IRWXG) >> 3,
			mode + 4, mode + 5, mode + 6);
		loadPerms((finfo->st_mode & S_IRWXO),
			mode + 7, mode + 8, mode + 9);
		mode[10] = 0;
		struct tm *timeinfo;
	    timeinfo = localtime(&(finfo->st_mtim.tv_sec));
	    if (!(strftime(tm, sizeof (tm), "%b %e %H:%M", timeinfo))) {
			continue;
	    }
	    tm[12] = 0;
		snprintf(line, STR_LENGTH, "%s %d %s %s %d %s %s\n", mode, 1,
			"FTP", "FTP", (int) finfo->st_size, tm, entry->d_name);
		strncpy(result, line, STR_LENGTH - 1);
		result[STR_LENGTH - 1] = '\0';
		result += strlen(line);
	}
	if (closedir(d))
		return (-1);
	return (0);
}

// recursively removes given directory
int rmrDir(char *dir) {
	DIR *d, *t;
	struct dirent *entry;
	char abs_fn[PATH_LENGTH];
	d = opendir(dir);
	if (d == NULL)
		return (-1);
	while ((entry = readdir(d))) {
		if ((strcmp(entry->d_name, ".") == 0) ||
			(strcmp(entry->d_name, "..") == 0))
			continue;
		snprintf(abs_fn, PATH_LENGTH, "%s/%s", dir, entry->d_name);
		if ((t = opendir(abs_fn))) {
			closedir(t);
			rmrDir(abs_fn);
		} else {
			unlink(abs_fn);
		}
	}
	closedir(d);
	rmdir(dir);
	return (0);
}

void doCleanup(struct config *configuration) {
	free(configuration->root_dir);
	free(configuration->user_db);
	rmrDir("/control_sockets");
}