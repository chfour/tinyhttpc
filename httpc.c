#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("%s addr port\n", argv[0]);
        return 1;
    }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket() failed: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(atol(argv[2]));

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("connect() failed: (%d) %s\n", errno, strerror(errno));
        return 1;
    }


    char recvbuf[1024];
    memset(recvbuf, '0', sizeof(recvbuf));
    int n = 0;
    while ((n = read(sockfd, recvbuf, sizeof(recvbuf)-1)) > 0) {
        recvbuf[n] = 0;
        if (fputs(recvbuf, stdout) == EOF) {
            printf("fputs() error\n");
        }
    }

    if (n < 0) {
        printf("\nread() error: (%d) %s\n", errno, strerror(errno));
    }

    if (close(sockfd) < 0) {
        printf("\nclose() error: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    return 0;
}
