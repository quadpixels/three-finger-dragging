#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int
getbyte(int fd, unsigned char *b)
{
    return(read(fd, b, 1) == 1);
}

int
putbyte(int fd, unsigned char b)
{
    unsigned char ack;

    printf("write %02X\n", b);
    if (write(fd, &b, 1) != 1) {
	fprintf(stderr, "error write: %s\n", strerror(errno));
	return 0;
    }

    if (!getbyte(fd, &ack)) {
	fprintf(stderr, "error read: %s\n", strerror(errno));
	return 0;
    }
    printf("read %02X\n", ack);

    if (ack != 0xFA) {
	fprintf(stderr, "error ack\n");
	return 0;
    }

    return 1;
}

int
special_cmd(int fd, unsigned char cmd)
{
    int i;

    if (putbyte(fd, 0xE6))
	for (i = 0; i < 4; i++) {
	    printf("special_cmd %i\n", i);
	    if ((!putbyte(fd, 0xE8)) || (!putbyte(fd, (cmd>>6)&0x3)))
		return 0;
	    cmd<<=2;
	}
    else
	return 0;
    return 1;
}

int
send_cmd(int fd, unsigned char cmd)
{
    return (special_cmd(fd, cmd) &&
	    putbyte(fd, 0xE9));
}

int
identify(int fd, unsigned long int *ident)
{
    unsigned char id[3];

    if (send_cmd(fd, 0x00) &&
	getbyte(fd, &id[0]) &&
	getbyte(fd, &id[1]) &&
	getbyte(fd, &id[2])) {
	*ident = (id[0]<<16)|(id[1]<<8)|id[2];
	printf("ident %06X\n", *ident);
	return 1;
    } else {
	fprintf(stderr, "error identify\n");
	return 0;
    }
}

int
reset(int fd)
{
    unsigned char r[2];

    if (!putbyte(fd, 0xFF)) {
	fprintf(stderr, "error reset\n");
	return 0;
    }

    sleep(5);

    if (getbyte(fd, &r[0]) && getbyte(fd, &r[1]))
	if (r[0] == 0xAA && r[1] == 0x00) {
	    fprintf(stderr, "reset done\n");
	    return 1;
	}
    fprintf(stderr, "error reset ack\n");
    return 0;
}

int
main(int argc, char* argv[])
{
    int fd;
    unsigned long int ident;

    fd = open("/dev/psaux", O_RDWR);
    if (fd == -1) {
	fprintf(stderr, "error open: %s\n", strerror(errno));
	exit(0);
    }

    reset(fd);
    identify(fd, &ident);

    close(fd);

    exit(0);
}
