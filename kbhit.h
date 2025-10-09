#ifndef KBHIT_H
#define KBHIT_H

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stddef.h>

static inline int kbhit(void)
{
    struct timeval tv;
    fd_set read_fd;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&read_fd);
    FD_SET(STDIN_FILENO, &read_fd);

    if (select(STDIN_FILENO + 1, &read_fd, NULL, NULL, &tv) == -1)
        return 0;

    return FD_ISSET(STDIN_FILENO, &read_fd);
}




#endif