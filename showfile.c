#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void main() {
    FILE* file = fopen("penguin-received.gif", "rb");
    if (file == NULL) {
        perror("Error opening file\n");
        return;
    }
    while (1) {
        unsigned char buf[1];
        if (fread(buf, 1, 1, file) < 1) {
            break;
        }
        printf("%02X ", buf[0]);
    }
    fclose(file);

}