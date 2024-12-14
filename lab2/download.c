#include "download.h"

#include "download.h"

int parseURL(char *inputUrl, struct URL *url) {
    regex_t regex;
    int hasCredentials;

    regcomp(&regex, "@", 0);
    hasCredentials = (regexec(&regex, inputUrl, 0, NULL, 0) == 0);

    if (hasCredentials) {
        if (sscanf(inputUrl, "%*[^/]//%[^:]:%[^@]@%[^/]/%s", 
                   url->user, url->password, url->host, url->path) != 4) {
            regfree(&regex);
            return -1;
        }
    } else {
        if (sscanf(inputUrl, "%*[^/]//%[^/]/%s", url->host, url->path) != 2) {
            regfree(&regex);
            return -1;
        }
        strcpy(url->user, "anonymous");
        strcpy(url->password, "anonymous");
    }

    regfree(&regex);

    struct hostent *h;
    memset(url->ip, 0, LENGTH);
    if ((h = gethostbyname(url->host)) == NULL) {
        herror("gethostbyname");
        return -1;
    }

    strncpy(url->ip, inet_ntoa(*((struct in_addr *)h->h_addr_list[0])), LENGTH - 1);
    return 0;
}

int connectToServer(int port, char *ip) {
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port); /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Usage: %s <URL>\n", argv[0]);
        return 1;
    }

    if (argc > 2) {
        printf("**** Only one argument needed: URL. The rest will be ignored. Carrying ON.\n");
    }

    char *inputUrl = argv[1];

    struct URL url;
    if (parseURL(inputUrl, &url) == 0) {
        printf("Parsed URL:\n");
        printf("User: %s\n", url.user);
        printf("Password: %s\n", url.password);
        printf("Host: %s\n", url.host);
        printf("Path: %s\n", url.path);
        printf("IP: %s\n", url.ip);
    } else {
        printf("Failed to parse URL\n");
        return -1;
    }

    return 0;
}
