#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define LENGTH 256

struct URL {
    char host[LENGTH];
    char path[LENGTH];
    char user[LENGTH];
    char password[LENGTH];
    char ip[LENGTH];
};

typedef enum {
    START,
    MULTIPLE_LINES,
    END
} State;

int parseURL(char *inputUrl, struct URL *url);
int newSocket(int port, char *ip);
int getReply(const int socket, char *buffer);
int authenticate(const int socket, const char *user, const char *pass);
int passive(const int socket, char *ip, int *port);
int requestFileTransfer(int socket, const char *path);
char *getFilename(const char *path);
int getFile(int socket1, int socket2, char *filename);
int closeConnection(const int socket);

#endif