#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <poll.h>
#include <arpa/inet.h>

struct {
	int verbose;
	int count;
	int lport, mport;
} conf;

int main(int argc, char **argv)
{
	struct sockaddr_in addr;
	int fd_in, fd_out, maxfds;
	struct pollfd *fds;
	int i, j, one = 1;
	ssize_t n;
	char buf[128];

	conf.count = 1000;
	conf.lport = 8433;
	conf.mport = 8434;

	while(argc > 1 && argv[1][0] == '-') {
		if(!strcmp(argv[1], "-h")) {
			printf("rundradio\n"
			       " -c N            maximum number of connections (1000)\n"
			       " -h              this help\n"
			       " -l PORT         listener port (8433)\n"
			       " -m PORT         messenger port (8434)\n"
			       " -v              verbose\n"
			       "\n"
			       "Sends messages from messengers to all listeners.\n"
			       "\n"
				);
			exit(0);
		}
		if(!strcmp(argv[1], "-v")) {
			conf.verbose = 1;			
		}
		if(!strcmp(argv[1], "-c")) {
			conf.count = atoi(argv[2]);
			argv++;
			argc--;
		}
		if(!strcmp(argv[1], "-l")) {
			conf.lport = atoi(argv[2]);
			argv++;
			argc--;
		}
		if(!strcmp(argv[1], "-m")) {
			conf.mport = atoi(argv[2]);
			argv++;
			argc--;
		}
		argv++;
		argc--;
	}

	fds = calloc(conf.count, sizeof(struct pollfd));
	if(!fds) exit(2);

	/* create two listening sockets. listen only on 127.0.0.1
	   the first socket is for messenger(s)
	   the second socket is for listeners
	   messages written by a messenger is sent to all listeners
	*/

	/* socket for messengers to connect */
	fd_in = socket( AF_INET, SOCK_STREAM, 0 );
	setsockopt(fd_in, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(conf.mport);
	if (bind(fd_in, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) < 0) {
		return -1;
	}
	listen(fd_in,3);

	/* socket for listeners to connect */
	fd_out = socket( AF_INET, SOCK_STREAM, 0 );
	setsockopt(fd_out, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(conf.lport);
	if (bind(fd_out, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) < 0) {
		return -1;
	}
	listen(fd_out,3);

	for(i=0;i<conf.count;i++) {
		fds[i].fd = -1;
		fds[i].events = POLLIN;
	}

	fds[0].fd = fd_in;
	fds[1].fd = fd_out;
	maxfds = 2;

	while(1) {
		poll(fds, maxfds, -1);

		if(fds[0].revents) {
			int fd;
			/* set non-blocking on accepted connections so we never block in read or write */
			fd = accept4(fds[0].fd, NULL, NULL, SOCK_NONBLOCK);
			if(fd >= 0) {
				for(i=2;i<conf.count;i++) {
					if(fds[i].fd == -1) {
						fds[i].fd = fd;
						/* Set events to POLLIN|POLLPRI for messenger.
						   We use POLLPRI only to keep track of which type of connection this is.
						   This way we do not need any additional data structure to keep track of
						   connection type.
						 */
						fds[i].events = POLLIN|POLLPRI;
						if(i >= maxfds) maxfds = i+1;
						break;
					}
					if(i == (conf.count-1)) {
						/* all possible connections used */
						fprintf(stderr, "All possible connections used: %d. Use -c to increase number of possible concurrent connections.\n", conf.count);
						close(fd);
					}
				}
			}
		}
		if(fds[1].revents) {
			int fd;
			/* set non-blocking on accepted connections so we never block in read or write */
			fd = accept4(fds[1].fd, NULL, NULL, SOCK_NONBLOCK);
			if(fd >= 0) {
				for(i=2;i<conf.count;i++) {
					if(fds[i].fd == -1) {
						fds[i].fd = fd;
						/* Set events to POLLIN for listener */
						fds[i].events = POLLIN;
						if(i >= maxfds) maxfds = i+1;
						break;
					}
					if(i == (conf.count-1)) {
						/* all possible connections used */
						fprintf(stderr, "All possible connections used: %d. Use -c to increase number of possible concurrent connections.\n", conf.count);
						close(fd);
					}
				}
			}
		}
		
		for(i=2;i<maxfds;i++) {
			if(fds[i].fd == -1) continue;
			
			if(fds[i].revents == 0) continue;

			/* if we are a listener */
			if(fds[i].events == POLLIN) {
				/* close listener on any event. they should always only listen */
				close(fds[i].fd);
				fds[i].fd = -1;
				continue;
			}

			/* if we are a messenger */
			if(fds[i].events == (POLLIN|POLLPRI)) {
				n = read(fds[i].fd, buf, sizeof(buf));
				if(n == 0) {
					/* end of file */
					close(fds[i].fd);
					fds[i].fd = -1;
					continue;
				}
				if(n == -1) {
					/* these errors on a nonblocking socket means we should wait and try again */
					if(errno == EAGAIN) continue;
					if(errno == EWOULDBLOCK) continue;
				}
				/* spread message to all listeners */
				for(j=2;j<maxfds;j++) {
					if(fds[j].events != POLLIN) continue;
					
					/* close listener connection on any error when writing to listener */
					if(write(fds[j].fd, buf, n) <= 0) {
						close(fds[j].fd);
						fds[j].fd = -1;
					}
				}
			}
		}
	}

	return 0;
}
