#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int outputformat = 0;

void
SynapticsReadPacket(int fd)
{
    int count = 0;
    int inSync = 0;
    unsigned char pBuf[7], u;

    while (read(fd,&u, 1) == 1) {
	pBuf[count++] = u;

	/* check first byte */
	if ((count == 1) && ((u & 0xC8) != 0x80)) {
	    inSync = 0;
	    count = 0;
	    printf("Synaptics driver lost sync at 1st byte\n");
	    continue;
	}

	/* check 4th byte */
	if ((count == 4) && ((u & 0xc8) != 0xc0)) {
	    inSync = 0;
	    count = 0;
	    printf("Synaptics driver lost sync at 4th byte\n");
	    continue;
	}

	if (count >= 6) { /* Full packet received */
	    if (!inSync) {
		inSync = 1;
		printf("Synaptics driver resynced.\n");
	    }
	    count = 0;
	    switch (outputformat) {
	    case 1:
		printf("Paket:%02X-%02X-%02X-%02X-%02X-%02X\n",
		       pBuf[0], pBuf[1], pBuf[2], pBuf[3], pBuf[4], pBuf[5]);
		break;
	    case 2:
		printf("x = %i, y = %i, z = %i, w = %i, l = %i, r = %i\n",
		       ((pBuf[3] & 0x10) << 8) | ((pBuf[1] & 0x0f) << 8) | pBuf[4],
		       ((pBuf[3] & 0x20) << 7) | ((pBuf[1] & 0xf0) << 4) | pBuf[5],
		       ((pBuf[0] & 0x30) >> 2) | ((pBuf[0] & 0x04) >> 1) | ((pBuf[3] & 0x04) >> 2),
		       ((pBuf[0] & 0x30) >> 2) | ((pBuf[0] & 0x04) >> 1) | ((pBuf[3] & 0x04) >> 2),
		       (pBuf[0] & 0x01) ? 1 : 0,
		       (pBuf[0] & 0x2) ? 1 : 0);
		break;
	    default:
		break;
	    }
	}
    }
}

int
main(int argc, char* argv[])
{
    int fd;

    if (argc > 1)
	outputformat = atoi(argv[1]);


    fd = open("/dev/psaux", O_RDONLY);
    if (fd == -1) {
	printf("Error opening /dev/psaux\n");
	exit(1);
    }

    SynapticsReadPacket(fd);

    close(fd);

    exit(0);
}
