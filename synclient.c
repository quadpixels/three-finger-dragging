#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#include <X11/Xdefs.h>
#include "synaptics.h"

static void show_hw_info(SynapticsSHM* synshm)
{
    printf("Hardware properties:\n");
    if (synshm->isSynaptics) {
	printf("    Model Id     = %08lx\n", synshm->model_id);
	printf("    Capabilities = %08lx\n", synshm->capabilities);
	printf("    Identity     = %08lx\n", synshm->identity);
    } else {
	printf("    No touchpad found\n");
	printf("    Do you use a newer kernel than 2.4?\n");
	printf("    Then browse the messages or boot.msg for the hardware info\n");
    }
}

/* ---------------------------------------------------------------------- */

enum ParaType {
    PT_INT,
    PT_BOOL,
    PT_DOUBLE
};

struct Parameter {
    char* name;				    /* Name of parameter */
    int offset;				    /* Offset in shared memory area */
    enum ParaType type;			    /* Type of parameter */
    double min_val;			    /* Minimum allowed value */
    double max_val;			    /* Maximum allowed value */
};

#define DEFINE_PAR(name, memb, type, min_val, max_val) \
{ name, offsetof(SynapticsSHM, memb), (type), (min_val), (max_val) }

static struct Parameter params[] = {
    DEFINE_PAR("LeftEdge",             left_edge,               PT_INT,    0, 10000),
    DEFINE_PAR("RightEdge",            right_edge,              PT_INT,    0, 10000),
    DEFINE_PAR("TopEdge",              top_edge,                PT_INT,    0, 10000),
    DEFINE_PAR("BottomEdge",           bottom_edge,             PT_INT,    0, 10000),
    DEFINE_PAR("FingerLow",            finger_low,              PT_INT,    0, 255),
    DEFINE_PAR("FingerHigh",           finger_high,             PT_INT,    0, 255),
    DEFINE_PAR("MaxTapTime",           tap_time,                PT_INT,    0, 1000),
    DEFINE_PAR("MaxTapMove",           tap_move,                PT_INT,    0, 2000),
    DEFINE_PAR("EmulateMidButtonTime", emulate_mid_button_time, PT_INT,    0, 1000),
    DEFINE_PAR("VertScrollDelta",      scroll_dist_vert,        PT_INT,    5, 1000),
    DEFINE_PAR("HorizScrollDelta",     scroll_dist_horiz,       PT_INT,    5, 1000),
    DEFINE_PAR("MinSpeed",             min_speed,               PT_DOUBLE, 0, 1.0),
    DEFINE_PAR("MaxSpeed",             max_speed,               PT_DOUBLE, 0, 1.0),
    DEFINE_PAR("AccelFactor",          accl,                    PT_DOUBLE, 0, 0.2),
    DEFINE_PAR("EdgeMotionSpeed",      edge_motion_speed,       PT_INT,    0, 400),
    DEFINE_PAR("UpDownScrolling",      updown_button_scrolling, PT_BOOL,   0, 1),
    DEFINE_PAR("TouchpadOff",          touchpad_off,            PT_BOOL,   0, 1),
    DEFINE_PAR("LockedDrags",          locked_drags,            PT_BOOL,   0, 1),
    { 0, 0, 0, 0, 0 }
};

static void show_settings(SynapticsSHM* synshm)
{
    int i;

    printf("Parameter settings:\n");
    for (i = 0; params[i].name; i++) {
	struct Parameter* par = &params[i];
	switch (par->type) {
	case PT_INT:
	    printf("    %-20s = %d\n", par->name, *(int*)((char*)synshm + par->offset));
	    break;
	case PT_BOOL:
	    printf("    %-20s = %d\n", par->name, *(Bool*)((char*)synshm + par->offset));
	    break;
	case PT_DOUBLE:
	    printf("    %-20s = %g\n", par->name, *(double*)((char*)synshm + par->offset));
	    break;
	}
    }
}

static void set_variables(SynapticsSHM* synshm, int argc, char* argv[], int first_cmd)
{
    int i;
    for (i = first_cmd; i < argc; i++) {
	char* cmd = argv[i];
	char* eqp = index(cmd, '=');
	if (eqp) {
	    int j;
	    int found = 0;
	    *eqp = 0;
	    for (j = 0; params[j].name; j++) {
		if (strcmp(cmd, params[j].name) == 0) {
		    found = 1;
		    break;
		}
	    }
	    if (found) {
		double val = atof(&eqp[1]);
		struct Parameter* par = &params[j];

		if (val < par->min_val)
		    val = par->min_val;
		if (val > par->max_val)
		    val = par->max_val;

		switch (par->type) {
		case PT_INT:
		    *(int*)((char*)synshm + par->offset) = (int)rint(val);
		    break;
		case PT_BOOL:
		    *(Bool*)((char*)synshm + par->offset) = (Bool)rint(val);
		    break;
		case PT_DOUBLE:
		    *(double*)((char*)synshm + par->offset) = val;
		    break;
		}
	    } else {
		printf("Unknown parameter %s\n", cmd);
	    }
	} else {
	    printf("Invalid command: %s\n", cmd);
	}
    }
}

static int is_equal(SynapticsSHM* s1, SynapticsSHM* s2)
{
    return ((s1->x           == s2->x) &&
	    (s1->y           == s2->y) &&
	    (s1->z           == s2->z) &&
	    (s1->numFingers  == s2->numFingers) &&
	    (s1->fingerWidth == s2->fingerWidth) &&
	    (s1->left        == s2->left) &&
	    (s1->right       == s2->right) &&
	    (s1->up          == s2->up) &&
	    (s1->down        == s2->down));
}

static void monitor(SynapticsSHM* synshm, int delay)
{
    SynapticsSHM old;
    memset(&old, 0, sizeof(SynapticsSHM));
    old.x = -1;				    /* Force first equality test to fail */

    while(1) {
	SynapticsSHM cur = *synshm;
	if (!is_equal(&old, &cur)) {
	    printf("x:%4d y:%4d z:%3d f:%1d w:%2d left:%d right:%d up:%d down:%d\n",
		   cur.x, cur.y, cur.z, cur.numFingers, cur.fingerWidth,
		   cur.left, cur.right, cur.up, cur.down);
	    old = cur;
	}
	usleep(delay * 1000);
    }
}

static void usage()
{
    fprintf(stderr, "Usage: synclient [-m interval] [-h] [-l] [-?] [var1=value1 [var2=value2] ...]\n");
    fprintf(stderr, "  -m monitor changes to the touchpad state.\n"
	    "     interval specifies how often (in ms) to poll the touchpad state\n");
    fprintf(stderr, "  -h Show detected hardware properties\n");
    fprintf(stderr, "  -l List current user settings\n");
    fprintf(stderr, "  -? Show this help message\n");
    fprintf(stderr, "  var=value  Set user parameter 'var' to 'value'.\n");
    exit(1);
}

int main(int argc, char* argv[])
{
    SynapticsSHM *synshm;
    int shmid;

    int c;
    int delay = -1;
    int do_monitor = 0;
    int dump_hw = 0;
    int dump_settings = 0;
    int first_cmd;

    /* Parse command line parameters */
    while ((c = getopt(argc, argv, "m:hl")) != -1) {
	switch (c) {
	case 'm':
	    do_monitor = 1;
	    if ((delay = atoi(optarg)) < 0)
		usage();
	    break;
	case 'h':
	    dump_hw = 1;
	    break;
	case 'l':
	    dump_settings = 1;
	    break;
	default:
	    usage();
	}
    }
    first_cmd = optind;
    if (!do_monitor && !dump_hw && !dump_settings && first_cmd == argc)
	usage();

    /* Connect to the shared memory area */
    if((shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0)) == -1) {
	if ((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1) {
	    fprintf(stderr, "Can't access shared memory area. SHMConfig disabled?\n");
	    exit(1);
	} else {
	    fprintf(stderr, "Incorrect size of shared memory area. Incompatible driver version?\n");
	    exit(1);
	}
    }
    if((synshm = (SynapticsSHM*) shmat(shmid, NULL, 0)) == NULL) {
	perror("shmat");
	exit(1);
    }

    /* Perform requested actions */
    if (dump_hw) {
	show_hw_info(synshm);
    }
    set_variables(synshm, argc, argv, first_cmd);
    if (dump_settings) {
	show_settings(synshm);
    }
    if (do_monitor) {
	monitor(synshm, delay);
    }

    exit(0);
}
