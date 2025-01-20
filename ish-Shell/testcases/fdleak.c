#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int i, fd;
    for (i = 3; i < 100; i++) {
	fd = open("/dev/null", O_RDONLY); 
	if (fd != i) {
	    printf("FD #%d leaked.\n", i);
	    close(fd);
	}
    }
}
