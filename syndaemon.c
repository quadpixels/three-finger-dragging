/*
 *   Copyright 2003-2004 Peter Osterlund <petero2@telia.com>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "synaptics.h"

static SynapticsSHM *synshm;
static int pad_disabled;
static int disable_taps_only;
static int background = 0;


static void usage()
{
    fprintf(stderr, "Usage: syndaemon [-i idle-time] [-d]\n");
    fprintf(stderr, "  -i How many seconds to wait after the last key press before\n");
    fprintf(stderr, "     enabling the touchpad. (default is 2s)\n");
    fprintf(stderr, "  -d Start as a daemon, ie in the background.\n");
    fprintf(stderr, "  -t Only disable tapping, not mouse movements.\n");
    exit(1);
}

static void signal_handler(int signum)
{
    if (pad_disabled) {
	synshm->touchpad_off = 0;
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
    if (synshm->guest_left || synshm->guest_mid || synshm->guest_right)
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
		if (!background)
		    printf("Enable\n");
		synshm->touchpad_off = 0;
		pad_disabled = 0;
	    }
	} else {			    /* Disable touchpad */
	    if (!pad_disabled && !synshm->touchpad_off) {
		if (!background)
		    printf("Disable\n");
		pad_disabled = 1;
		if (disable_taps_only)
		    synshm->touchpad_off = 2;
		else
		    synshm->touchpad_off = 1;
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
    while ((c = getopt(argc, argv, "i:dt?")) != EOF) {
	switch(c) {
	case 'i':
	    idle_time = atof(optarg);
	    break;
	case 'd':
	    background = 1;
	    break;
	case 't':
	    disable_taps_only = 1;
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

    if (background) {
	pid_t pid;
	if ((pid = fork()) < 0) {
	    perror("fork");
	    exit(3);
	} else if (pid != 0)
	    exit(0);

	/* Child (daemon) is running here */
	setsid();	/* Become session leader */
	chdir("/");	/* In case the file system gets unmounted */
	umask(0);	/* We don't want any surprises */
    }

    /* Run the main loop */
    main_loop(display, idle_time);

    return 0;
}
