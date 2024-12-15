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
                        printf("Failed to parse response code\n");
                        return -1;
                    }

                    if (buffer[3] == '-') {
                        state = MULTIPLE_LINES;
                        strncpy(temp, buffer, 3);
                    } else {
                        state = END;
                    }
                    break;

                case MULTIPLE_LINES:
                    if (strncmp(buffer, temp, 3) == 0 && buffer[3] == ' ') {
                        state = END;
                    }
                    break;

                case END:
                    break;

                default:
                    printf("Unexpected state\n");
                    return -1;
            }

            index = 0;

            if (state == END) {
                return code;
            }
        }
    }

    printf("Failed to read a complete response\n");
    return -1;
}

int authenticate(const int socket, const char *user, const char *pass) {
    char commandUser[5 + strlen(user) + 3];
    char commandPass[5 + strlen(pass) + 3];
    char answer[LENGTH];

    sprintf(commandUser, "USER %s\r\n", user);
    sprintf(commandPass, "PASS %s\r\n", pass);

    if (write(socket, commandUser, strlen(commandUser)) < 0) {
        printf("write USER command failed\n");
        return -1;
    }

    if (getReply(socket, answer) != 331) {
        printf("Failed to authenticate\n");
        return -1;
    }

    if (write(socket, commandPass, strlen(commandPass)) < 0) {
        printf("write PASS command failed\n");
        return -1;
    }

    return getReply(socket, answer);
}

int passive(const int socket, char *ip, int *port) {
    char answer[LENGTH];
    int port1, port2, ip1, ip2, ip3, ip4;

    printf("Sending PASV command...\n");
    if (write(socket, "PASV\r\n", 6) < 0) {
        printf("Failed to send PASV command\n");
        return -1;
    }

    int code = getReply(socket, answer);
    if (code != 227) {
        printf("Unexpected PASV reply code: %d\n", code);
        return -1;
    }

    if (sscanf(answer, "%*[^(](%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6) {
        printf("Failed to parse PASV response: %s\n", answer);
        return -1;
    }

    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    printf("Passive mode: IP = %s, Port = %d\n", ip, *port);

    return code;
}

int requestFileTransfer(int socket, const char *path) {
    char answer[LENGTH];
    char command[strlen(path) + 5 + 3];

    snprintf(command, sizeof(command), "RETR %s\r\n", path);
    printf("Requesting file transfer: %s", command);

    if (write(socket, command, strlen(command)) < 0) {
        perror("Failed to send RETR command");
        return -1;
    }

    int code = getReply(socket, answer);
    printf("Server response to RETR: %d\n", code);

    if (code != 150 && code != 125) {
        printf("Failed to initiate file transfer. Server response: %s\n", answer);
        return -1;
    }

    return 0;
}


char *getFilename(const char *path) {
    const char *lastSlash = strrchr(path, '/');
    if (lastSlash == NULL) {
        return strdup(path);
    }

    return strdup(lastSlash + 1);
}

int getFile(int socket1, int socket2, char *filename) {
    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("ERROR OPENING FILE\n");
        return -1;
    }

    char buffer[LENGTH];
    int bytes;
    while ((bytes = read(socket2, buffer, LENGTH)) > 0) {
        printf("Read %d bytes from data socket\n", bytes);
        if (fwrite(buffer, 1, bytes, fd) != bytes) {
            printf("File write error\n");
            fclose(fd);
            return -1;
        }
    }

    if (bytes < 0) {
        printf("Error reading from data socket\n");
        return -1;
    }


    fclose(fd);

    if (close(socket2) < 0) {
        printf("Failed to close data socket\n");
        return -1;
    }
    return getReply(socket1, buffer);
}

int closeConnection(const int socket) {
    char answer[LENGTH];

    printf("Sending QUIT command...\n");
    if (write(socket, "QUIT\r\n", 6) < 0) {
        perror("Failed to send QUIT command");
        return -1;
    }

    if (getReply(socket, answer) != 221) {
        printf("ERROR ENDING CONNECTION: %s\n", answer);
        return -1;
    }

    printf("Closing connection...\n");
    if (close(socket) < 0) {
        perror("Failed to close control socket");
        return -1;
    }

    return 0;
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
        printf("User: %s\n", url.user);
        printf("Password: %s\n", url.password);
        printf("Host: %s\n", url.host);
        printf("Path: %s\n", url.path);
        printf("IP: %s\n", url.ip);
    } else {
        printf("Failed to parse URL\n");
        return -1;
    }

    printf("----------------------\nPARSED URL\n----------------------\n");

    int socket1 = newSocket(21, url.ip);
    char reply[LENGTH];
    
    if (socket1 < 0 || getReply(socket1, reply) != 220) {
        printf("CONNECTION FAIL!\n");
        return -1;
    }

    printf("----------------------\nCONNECTED TO SERVER\n----------------------\n");

    if (authenticate(socket1, url.user, url.password) == -1) {
        printf("AUTHENTICATION FAIL!\n");
        return -1;
    }

    printf("----------------------\nAUTHENTICATED\n----------------------\n");

    char ip[LENGTH];
    int port;

    if (passive(socket1, ip, &port) == -1)  {
        printf("PASSIVE FAIL!\n");
        return -1;
    }

    printf("----------------------\nENABLED PASSIVE MODE\n----------------------\n");

    int socket2 = newSocket(port, ip);

    if (socket2 < 0) {
        printf("SOCKET 2 FAIL!");
        return -1;
    }

    if (requestFileTransfer(socket1, url.path) == -1) {
        printf("REQUEST FILE FAIL!\n");
        return -1;
    }

    char *file = getFilename(url.path);

    if (getFile(socket1, socket2, file) == -1) {
        printf("GET FILE FAIL!\n");
        return -1;
    }

    if (closeConnection(socket1) == -1) {
        printf("CLOSE CONNECTION FAIL!\n");
        return -1;
    }

    printf("----------------------\nSUCCESS!\n----------------------\n");
    return 0;
}
