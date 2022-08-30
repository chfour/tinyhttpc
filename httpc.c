#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "%s addr:[port]\n", argv[0]);
        return 1;
    }

    char host[NI_MAXHOST];
    strcpy(host, argv[1]);
    char port[NI_MAXSERV];
    memset(port, 0, sizeof(port));
    for (int i = 0; i < strlen(argv[1]); i++) {
        if (argv[1][i] == ':') {
            memcpy(host, argv[1], i); host[i] = 0;
            memcpy(port, argv[1]+i+1, strlen(argv[1]));
            break;
        }
    }
    if (strlen(port) == 0) memcpy(port, "80", 2);
    printf("host='%s' port='%s'\n", host, port);

    struct addrinfo hints;
    struct addrinfo *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // allow ipv4 or v6
    hints.ai_socktype = SOCK_STREAM;

    int gaires;
    if ((gaires = getaddrinfo(host, port, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo() failed: (%d) %s\n", gaires, gai_strerror(gaires));
        exit(1);
    }

    char addrstr[INET6_ADDRSTRLEN];
    struct addrinfo *cresult;
    int sockfd;
    for (cresult = result; cresult != NULL; cresult = cresult->ai_next) {
        if (result->ai_addr->sa_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)result->ai_addr;
            inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr));
        } else if (result->ai_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)result->ai_addr;
            inet_ntop(AF_INET6, &p->sin6_addr, addrstr, sizeof(addrstr));
        }
        fprintf(stderr, "trying '%s'...\n", addrstr);
        if ((sockfd = socket(result->ai_addr->sa_family, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "socket() failed: (%d) %s\n", errno, strerror(errno));
            if (close(sockfd) < 0)
                fprintf(stderr, "close() error: (%d) %s\n", errno, strerror(errno));
            sockfd = -1;
            continue;
        }

        if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
            fprintf(stderr, "connect() failed: (%d) %s\n", errno, strerror(errno));
            if (close(sockfd) < 0)
                fprintf(stderr, "close() error: (%d) %s\n", errno, strerror(errno));
            sockfd = -1;
            continue;
        }
        fprintf(stderr, "connected!\n");
        break;
    }

    if (sockfd == -1) {
        fprintf(stderr, "connecting failed\n");
        return 1;
    }

    char writebuf[5] = "hi!\n";
    if (write(sockfd, writebuf, sizeof(writebuf)-1) < 0) {
        fprintf(stderr, "write() error: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    char recvbuf[1024];
    memset(recvbuf, '0', sizeof(recvbuf));
    int n = 0;
    while ((n = read(sockfd, recvbuf, sizeof(recvbuf)-1)) > 0) {
        recvbuf[n] = 0;
        if (fputs(recvbuf, stdout) == EOF) {
            fprintf(stderr, "fputs() error\n");
        }
    }

    if (n < 0) {
        fprintf(stderr, "\nread() error: (%d) %s\n", errno, strerror(errno));
    }

    if (close(sockfd) < 0) {
        fprintf(stderr, "\nclose() error: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    return 0;
}
