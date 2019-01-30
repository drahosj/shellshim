#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#define LOGIN_OFFSET 0
#define LOGIN_LENGTH 16

#define HOSTNAME_OFFSET 16
#define HOSTNAME_LENGTH 16

#define DAEMON_OFFSET 32
#define DAEMON_LENGTH 16

int main(int argc, char ** argv)
{
      uint16_t port = 8080;
      if (argv[1]) {
            port = atoi(argv[1]);
      }

      int listensock = socket(AF_INET, SOCK_STREAM, 0);
      if (listensock < 0) {
            perror("socket");
            return -1;
      }

      struct sockaddr_in sa;
      memset(&sa, 0, sizeof(sa));

      sa.sin_family = AF_INET;
      inet_aton("0.0.0.0", &sa.sin_addr);
      sa.sin_port = htons(port);

      if (bind(listensock, (struct sockaddr *) &sa, sizeof(sa))) { 
            perror("bind");
            return -1;
      }

      if (listen(listensock, 8)) {
            perror("listen");
            return -1;
      }

      while (1) {
            struct sockaddr_in sa_conn;
            socklen_t ssize;
            int sock_conn = accept(
                        listensock, 
                        (struct sockaddr *) &sa_conn, 
                        &ssize);

            if (sock_conn < 0) {
                  perror("accept");
            }

            pid_t pid = fork();
            if (pid) {
                  close(sock_conn);
            } else {
			int ptm = getpt();
			grantpt(ptm);
			unlockpt(ptm);

			char control[128];
			read(sock_conn, control, 128);

			char u[17];
			char h[17];
			char d[17];

			memset(u, 0, 17);
			memset(h, 0, 17);
			memset(d, 0, 17);

			memcpy(u, control + LOGIN_OFFSET, LOGIN_LENGTH);
			memcpy(h, control + HOSTNAME_OFFSET, HOSTNAME_LENGTH);
			memcpy(d, control + DAEMON_OFFSET, DAEMON_LENGTH);

			printf("%s: %s@%s (%s)\n", ptsname(ptm), u, h, d);

			char buf[1024];
			for (;;) {
				ssize_t len = read(sock_conn, buf, 1024);
				if (len > 0) {
					write(ptm, buf, len);
				}
			}
            }
      }

      return 0;
}

