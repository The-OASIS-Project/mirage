#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DELAY 10 // milliseconds
#define BUFFERSIZE 1024

int main() {
    unsigned char msg[] = "F11\n"
                          "Ctrl+i\n"
                          "i\n"
                          "\n"
                          "    unsigned char msg[BUFFERSIZE];\n"
                          "    int msglen;\n"
                          "    int pos;\n"
                          "  /* Initialize starting address *\n"
                          "           *start = IMAGE_START;\n"
                          "count += len = nbread(fd, buf, BUFFERSIZE, 50);\n"
                          "       if (len == sizeof(msg) && !memcmp(buf, msg, sizeof(msg)))\n"
                          "         return RCX_OK; /* success */\n"
                          "   } while (timer_read(&timer) < (float)timeout / 1000.0f);\n"
                          "if ((file = fopen(name, \"r\"))\n"
                          "          fprintf\n"
                          "/* Failed. Possibly a long message? *\n"
                          "        /* long message if opconde is\n"
                          "        /* If long message, checksum\n"
                          "        for (sum = 0, len = 0, pos = 3\n"
                          "unsigned char fastdl_image[] = {\n"
                          "  121, 6, 0, 15,107,134,238,128,121,\n"
                          "  238,116, 94, 0, 59,154, 11, 135, 121,\n"
                          "  127,216,114, 80,254,103, 62,217, 24,\n"
                          "  106,142,239,  6,254, 13,106,142,238,\n"
                          "  111,117, 32, 98,121,116,101, 44, 32,119,104,101,110, 32, 73, 32,\n"
                          "  107,110,";

    int msglen = sizeof(msg) - 1;
    
    for (int i = 0; i < msglen; i++) {
        putchar(msg[i]);
        fflush(stdout);
        usleep(DELAY * 1000);
    }

    return 0;
}

