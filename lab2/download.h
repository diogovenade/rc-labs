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

#define LENGTH 100

struct URL {
    char host[LENGTH];
    char path[LENGTH];
    char user[LENGTH];
    char password[LENGTH];
    char ip[LENGTH];
};

int parseURL(char *inputUrl, struct URL *url);

#endif