#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include "synaptics.h"

static SynapticsSHM *synshm;
static int pad_disabled;
static Bool saved_touchpad_off;


static void usage()
{
    fprintf(stderr, "Usage: syndaemon [-i idle-time]\n");
    fprintf(stderr, "  -i How many seconds to wait after the last key press before\n");
    fprintf(stderr, "     enabling the touchpad.\n");
    exit(1);
}

static void signal_handler(int signum)
{
    if (pad_disabled) {
	synshm->touchpad_off = saved_touchpad_off;
	pad_disabled = 0;
    }
    kill(getpid(), signum);
}

static void install_signal_handler()
{
    static int signals[] = {
	SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
	SIGBUS, SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE,
	SIGALRM, SIGTERM, SIGPWR
    };
    int i;
    struct sigaction act;
    sigset_t set;

    sigemptyset(&set);
    act.sa_handler = signal_handler;
    act.sa_mask = set;
    act.sa_flags = SA_ONESHOT;

    for (i = 0; i < sizeof(signals) / sizeof(int); i++) {
	if (sigaction(signals[i], &act, 0) == -1) {
	    perror("sigaction");
	    exit(2);
	}
    }
}

/**
 * Return non-zero if the keyboard state has changed since the last call.
 */
static int keyboard_activity(Display *display)
{
    #define KEYMAP_SIZE 32
    static char old_key_state[KEYMAP_SIZE];
    char key_state[KEYMAP_SIZE];
    int i;

    XQueryKeymap(display, key_state);

    for (i = 0; i < KEYMAP_SIZE; i++) {
	if (key_state[i] != old_key_state[i]) {
	    for (i = 0; i < KEYMAP_SIZE; i++)
		old_key_state[i] = key_state[i];
	    return 1;
	}
    }
    return 0;
}

/**
 * Return non-zero if any physical touchpad button is currently pressed.
 */
static int touchpad_buttons_active()
{
    int i;

    if (synshm->left || synshm->right || synshm->up || synshm->down)
	return 1;
    for (i = 0; i < 8; i++)
	if (synshm->multi[i])
	    return 1;
    return 0;
}

static double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void main_loop(Display *display, double idle_time)
{
    const int poll_delay = 20000;	    /* 20 ms */
    double last_activity = 0.0;
    double current_time;

    pad_disabled = 0;
    keyboard_activity(display);
	
    for (;;) {
	current_time = get_time();
	if (keyboard_activity(display))
	    last_activity = current_time;
	if (touchpad_buttons_active())
	    last_activity = 0.0;

	if (current_time > last_activity + idle_time) {	/* Enable touchpad */
	    if (pad_disabled) {
		printf("Enable\n");
		synshm->touchpad_off = saved_touchpad_off;
		pad_disabled = 0;
	    }
	} else {			    /* Disable touchpad */
	    if (!pad_disabled) {
		printf("Disable\n");
		saved_touchpad_off = synshm->touchpad_off;
		synshm->touchpad_off = 1;
		pad_disabled = 1;
	    }
	}

	usleep(poll_delay);
    }
}

int main(int argc, char *argv[])
{
    double idle_time = 2.0;
    Display *display;
    int c;
    int shmid;

    /* Parse command line parameters */
    while ((c = getopt(argc, argv, "i:?")) != EOF) {
	switch(c) {
	case 'i':
	    idle_time = atof(optarg);
	    break;
	default:
	    usage();
	    break;
	}
    }
    if (idle_time <= 0.0)
	usage();

    /* Open a connection to the X server */
    display = XOpenDisplay(NULL);
    if (!display) {
	fprintf(stderr, "Can't open display.\n");
	exit(2);
    }

    /* Connect to the shared memory area */
    if ((shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0)) == -1) {
	if ((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1) {
	    fprintf(stderr, "Can't access shared memory area. SHMConfig disabled?\n");
	    exit(2);
	} else {
	    fprintf(stderr, "Incorrect size of shared memory area. Incompatible driver version?\n");
	    exit(2);
	}
    }
    if ((synshm = (SynapticsSHM*) shmat(shmid, NULL, 0)) == NULL) {
	perror("shmat");
	exit(2);
    }

    /* Install a signal handler to restore synaptics parameters on exit */
    install_signal_handler();

    /* Run the main loop */
    main_loop(display, idle_time);

    return 0;
}
