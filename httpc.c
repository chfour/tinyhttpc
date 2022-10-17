#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LINE_MAXLEN 8192
#define RECVBUF_REALLOC_CHUNK 256

int remote_connect(char* host, char* port, int family)
{
    struct addrinfo hints;
    struct addrinfo *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;

    int gaires;
    if ((gaires = getaddrinfo(host, port, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo() failed: (%d) %s\n", gaires, gai_strerror(gaires));
        exit(1);
    }

    char addrstr[INET6_ADDRSTRLEN];
    int res_port;
    struct addrinfo *cresult;
    int sockfd;
    for (cresult = result; cresult != NULL; cresult = cresult->ai_next) {
        if (cresult->ai_addr->sa_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)cresult->ai_addr;
            inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr));
            res_port = htons(p->sin_port);
        } else if (cresult->ai_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)cresult->ai_addr;
            inet_ntop(AF_INET6, &p->sin6_addr, addrstr, sizeof(addrstr));
            res_port = htons(p->sin6_port);
        }
        fprintf(stderr, "trying '%s' port %i...\n", addrstr, res_port);

        if ((sockfd = socket(cresult->ai_addr->sa_family, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "socket() failed: (%d) %s\n", errno, strerror(errno));
            if (close(sockfd) < 0)
                fprintf(stderr, "close() error: (%d) %s\n", errno, strerror(errno));
            sockfd = -1;
            continue;
        }

        if (connect(sockfd, cresult->ai_addr, cresult->ai_addrlen) < 0) {
            fprintf(stderr, "connect() failed: (%d) %s\n", errno, strerror(errno));
            if (close(sockfd) < 0)
                fprintf(stderr, "close() error: (%d) %s\n", errno, strerror(errno));
            sockfd = -1;
            continue;
        }
        fprintf(stderr, "connected!\n");
        break;
    }
    return sockfd;
}

ssize_t write_helper(int fd, const void* buf, size_t count) {
    ssize_t retval;
    if ((retval = write(fd, buf, count)) < 0) {
        fprintf(stderr, "error: in write_helper(): write(sock) error: (%d) %s\n", errno, strerror(errno));
        exit(1);
    }

    char* tmpbuf = malloc(count+2);
    strcpy(tmpbuf, "> ");
    memcpy(tmpbuf+2, buf, count);
    tmpbuf[2+count] = 0;

    if (fputs(tmpbuf, stderr) == EOF) {
        fprintf(stderr, "error: in write_helper(): fputs() error\n");
    }

    free(tmpbuf);
    return retval;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "%s addr[:port][/path]\n", argv[0]);
        return 1;
    }

    char host[NI_MAXHOST];
    memset(host, 0, sizeof(host));

    char path[NI_MAXHOST];
    memset(path, 0, sizeof(path));

    char port[NI_MAXSERV];
    memset(port, 0, sizeof(port));

    for (int i = 0; i < strlen(argv[1]); i++) {
        if (argv[1][i] == ':' && (argv[1][0] != '[' || argv[1][i-1] == ']')) {
            unsigned int j;
            char* j_ = strchr(argv[1]+i, '/');
            if (j_ == NULL) j = strlen(argv[1]);
            else j = j_ - argv[1];
            //fprintf(stderr, "i=%i j=%i len=%i\n", i, j, strlen(argv[1]));

            if (i+1 >= j) {
                fprintf(stderr, "error: invalid URI: expected port after ':'\n%s\n%*s^here\n", argv[1], i+1, "");
                exit(1);
            }

            if (strlen(host) > 0) continue;

            if (argv[1][0] == '[') {
                memcpy(host, argv[1]+1, i-1); host[i-2] = 0;
            } else {
                memcpy(host, argv[1], i); host[i] = 0;
            }
            memcpy(port, argv[1]+i+1, j-i-1);
        } else if (argv[1][i] == '/') {
            if (strlen(path) > 0) continue;

            if (strlen(host) == 0) memcpy(host, argv[1], i);
            strcpy(path, argv[1]+i);
        }
    }
    if (strlen(host) == 0) strcpy(host, argv[1]);
    if (strlen(port) == 0) memcpy(port, "80", 2);
    if (strlen(path) == 0) path[0] = '/';
    fprintf(stderr, "parsed host='%s' port='%s' path='%s'\n", host, port, path);

    int sockfd = remote_connect(host, port, AF_UNSPEC);

    if (sockfd == -1) {
        fprintf(stderr, "connecting failed\n");
        return 1;
    }

    char writebuf[LINE_MAXLEN];
    memset(writebuf, 0, sizeof(writebuf));
    sprintf(writebuf, "GET %s HTTP/1.1\r\n", path);
    write_helper(sockfd, writebuf, strlen(writebuf));

    memset(writebuf, 0, sizeof(writebuf));
    if (strcmp(port, "80") == 0)
        sprintf(writebuf, "Host: %s\r\n", host);
    else
        sprintf(writebuf, "Host: %s:%s\r\n", host, port);
    write_helper(sockfd, writebuf, strlen(writebuf));

    strcpy(writebuf, "Accept: */*\r\n");
    write_helper(sockfd, writebuf, strlen(writebuf));

    write_helper(sockfd, "\r\n", 2);

    unsigned int recvline_s = 0;
    char* recvline = NULL;
    char recv, last = 0;
    unsigned int linepos = 0;
    ssize_t nread;
    while ((nread = read(sockfd, &recv, 1)) > 0) {
        if (recvline_s == 0 || linepos > recvline_s - 1) {
            //fprintf(stderr, "realloc to %i (+%i)\n", recvline_s + RECVBUF_REALLOC_CHUNK, RECVBUF_REALLOC_CHUNK);
            recvline = realloc(recvline, recvline_s + RECVBUF_REALLOC_CHUNK);
            recvline_s += RECVBUF_REALLOC_CHUNK;
            memset(recvline+recvline_s-RECVBUF_REALLOC_CHUNK, 0, RECVBUF_REALLOC_CHUNK);
        }
        if (recv == 0) continue;
        else if (recv == '\r') {}
        else if (recv == '\n') {
            if (last == '\r') {
                last = recv;
                continue;
            }
        } else {
            recvline[linepos++] = recv;
            last = recv;
            continue;
        }
        last = recv;

        // BEGIN handle line
        fputs("< ", stderr);
        for (int i = 0; i < strlen(recvline); i++)
            fputc(recvline[i] <= '~' && recvline[i] >= ' ' ? recvline[i] : '.', stderr);
        fputc('\n', stderr);
        // END handle line

        free(recvline); recvline = NULL; recvline_s = 0;
        linepos = 0;
    }

    if (nread < 0)
        fprintf(stderr, "read() error: (%d) %s\n", errno, strerror(errno));

    if (close(sockfd) < 0) {
        fprintf(stderr, "close() error: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    return 0;
}
