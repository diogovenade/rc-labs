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

int newSocket(int port, char *ip) {
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

int getReply(const int socket, char *buffer) {
    char byte;
    int index = 0;
    int code = 0;
    State state = START;
    char temp[4] = {0};

    memset(buffer, 0, LENGTH);

    while (read(socket, &byte, 1) > 0) {
        if (index >= LENGTH - 1) {
            return -1;
        }

        buffer[index++] = byte;

        if (byte == '\n') {
            buffer[index] = '\0';
            printf("Response line: %s", buffer);

            switch (state) {
                case START:
                    if (sscanf(buffer, "%3d", &code) != 1) {
                        fprintf(stderr, "Failed to parse response code.\n");
                        return -1;
                    }

                    if (buffer[3] == '-') {
                        state = MULTIPLE;
                        strncpy(temp, buffer, 3);
                    } else {
                        state = END;
                    }
                    break;

                case MULTIPLE:
                    if (strncmp(buffer, temp, 3) == 0 && buffer[3] == ' ') {
                        state = END;
                    }
                    break;

                case END:
                    break;

                default:
                    fprintf(stderr, "Unexpected state.\n");
                    return -1;
            }

            index = 0;

            if (state == END) {
                return code;
            }
        }
    }

    fprintf(stderr, "Failed to read a complete response.\n");
    return -1;
}

int authenticate(const int socket, const char *user, const char *pass) {
    char userCommand[5 + strlen(user) + 3];
    char passCommand[5 + strlen(pass) + 3];
    char answer[LENGTH];

    sprintf(userCommand, "USER %s\r\n", user);
    sprintf(passCommand, "PASS %s\r\n", pass);

    if (write(socket, userCommand, strlen(userCommand)) < 0) {
        perror("write USER command failed");
        return -1;
    }

    if (getReply(socket, answer) != 331) {
        printf("Unknown user '%s'. Abort.\n", user);
        return -1;
    }

    if (write(socket, passCommand, strlen(passCommand)) < 0) {
        perror("write PASS command failed");
        return -1;
    }

    return getReply(socket, answer);
}

int passive(const int socket, char *ip, int *port) {
    char answer[LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;

    printf("Sending PASV command...\n");
    if (write(socket, "PASV\r\n", 6) < 0) {
        perror("Failed to send PASV command");
        return -1;
    }

    int code = getReply(socket, answer);
    if (code != 227) {
        fprintf(stderr, "Unexpected PASV reply code: %d\n", code);
        return -1;
    }

    printf("PASV response: %s\n", answer);

    if (sscanf(answer, "%*[^(](%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6) {
        fprintf(stderr, "Failed to parse PASV response: %s\n", answer);
        return -1;
    }

    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    printf("Passive mode: IP = %s, Port = %d\n", ip, *port);

    return code;
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

    int socket1 = newSocket(21, url.ip);
    char *reply = malloc(100);
    
    if (socket1 < 0 || getReply(socket1, reply) != 220) {
        printf("ERROR!\n");
        return -1;
    }

    printf("CONNECTION\n");

    authenticate(socket1, url.user, url.password);
    printf("AUTHENTICATION\n");

    char ip[LENGTH];
    int port;

    if (passive(socket1, ip, &port) == -1)  {
        printf("PASSIVE FAIL!\n");
        return -1;
    }

    printf("SUCCESS!\n");
    return 0;
}
