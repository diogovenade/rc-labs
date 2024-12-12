#include "download.h"

int parseURL(char *inputUrl, struct URL *url) {
    regex_t regex;
    regmatch_t matches[6];

    // Regular expression to parse FTP URLs
    const char *pattern = 
        "^ftp://([^/]+)(/.*)?$"; // Match user:pass@host/path

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error compiling regex\n");
        return -1;
    }

    if (regexec(&regex, inputUrl, 6, matches, 0) != 0) {
        fprintf(stderr, "Invalid URL format\n");
        regfree(&regex);
        return -1;
    }

    // Extract user
    if (matches[1].rm_so != -1) {
        size_t len = matches[1].rm_eo - matches[1].rm_so;
        strncpy(url->user, inputUrl + matches[1].rm_so, len);
        url->user[len] = '\0';
    } else {
        strcpy(url->user, "anonymous");
    }

    // Extract password
    if (matches[2].rm_so != -1) {
        size_t len = matches[2].rm_eo - matches[2].rm_so;
        strncpy(url->password, inputUrl + matches[2].rm_so, len);
        url->password[len] = '\0';
    } else {
        strcpy(url->password, "anonymous");
    }

    // Extract host
    if (matches[3].rm_so != -1) {
        size_t len = matches[3].rm_eo - matches[3].rm_so;
        strncpy(url->host, inputUrl + matches[3].rm_so, len);
        url->host[len] = '\0';
    }

    // Extract path
    if (matches[4].rm_so != -1) {
        size_t len = matches[4].rm_eo - matches[4].rm_so;
        strncpy(url->path, inputUrl + matches[4].rm_so, len);
        url->path[len] = '\0';
    } else {
        strcpy(url->path, "/");
    }

    // Resolve IP address
    struct hostent *host_entry;
    if ((host_entry = gethostbyname(url->host)) == NULL) {
        herror("gethostbyname");
        strcpy(url->ip, "Unknown");
    } else {
        strcpy(url->ip, inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])));
    }

    regfree(&regex);
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

    printf("%s\n", inputUrl);

    struct URL url;
    if (parseURL(inputUrl, &url) == 0) {
        printf("Parsed URL:\n");
        printf("  User: %s\n", url.user);
        printf("  Password: %s\n", url.password);
        printf("  Host: %s\n", url.host);
        printf("  Path: %s\n", url.path);
        printf("  IP: %s\n", url.ip);
    } else {
        fprintf(stderr, "Failed to parse URL\n");
    }

    return 0;
}