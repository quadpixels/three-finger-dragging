#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM {
	int x, y;
} SynapticsSHM;

int main() {
	SynapticsSHM *synshm;
	int shmid;

	if((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1) 
		printf("shmget Fehler\n");
	if((synshm = (SynapticsSHM*) shmat(shmid, NULL, 0)) == NULL)
		printf("shmat Fehler\n");

	while(1) {
		printf("x:%d y:%d\n", synshm->x, synshm->y);
		usleep(100000);
	}	

	exit(0);
}

/* Local Variables: */
/* tab-width: 4 */
/* End: */
