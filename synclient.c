#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM {
	int x, y;
	int z;								/* pressure value */
	int w;								/* finger width value */
	int left, right, up, down;			/* left/right/up/down buttons */
} SynapticsSHM;

static int is_equal(SynapticsSHM* s1, SynapticsSHM* s2)
{
	return ((s1->x     == s2->x) &&
			(s1->y     == s2->y) &&
			(s1->z     == s2->z) &&
			(s1->w     == s2->w) &&
			(s1->left  == s2->left) &&
			(s1->right == s2->right) &&
			(s1->up    == s2->up) &&
			(s1->down  == s2->down));
}

int main() {
	SynapticsSHM *synshm;
	int shmid;
	SynapticsSHM old;

	if((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1) 
		printf("shmget Fehler\n");
	if((synshm = (SynapticsSHM*) shmat(shmid, NULL, 0)) == NULL)
		printf("shmat Fehler\n");

	while(1) {
		SynapticsSHM cur = *synshm;
		if (!is_equal(&old, &cur)) {
			printf("x:%4d y:%4d z:%3d w:%2d left:%d right:%d up:%d down:%d\n",
				   cur.x, cur.y, cur.z, cur.w, cur.left, cur.right, cur.up, cur.down);
			old = cur;
		}
		usleep(100000);
	}

	exit(0);
}

/* Local Variables: */
/* tab-width: 4 */
/* End: */
