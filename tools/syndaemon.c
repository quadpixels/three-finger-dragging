/*
 * Copyright © 2003-2004 Peter Osterlund
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *      Peter Osterlund (petero2@telia.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/record.h>

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
static int ignore_modifier_combos;
static int ignore_modifier_keys = 0;
static int background;
static const char *pid_file;

#define KEYMAP_SIZE 32
static unsigned char keyboard_mask[KEYMAP_SIZE];

static void
usage(void)
{
    fprintf(stderr, "Usage: syndaemon [-i idle-time] [-m poll-delay] [-d] [-t] [-k]\n");
    fprintf(stderr, "  -i How many seconds to wait after the last key press before\n");
    fprintf(stderr, "     enabling the touchpad. (default is 2.0s)\n");
    fprintf(stderr, "  -m How many milli-seconds to wait until next poll.\n");
    fprintf(stderr, "     (default is 200ms)\n");
    fprintf(stderr, "  -d Start as a daemon, ie in the background.\n");
    fprintf(stderr, "  -p Create a pid file with the specified name.\n");
    fprintf(stderr, "  -t Only disable tapping and scrolling, not mouse movements.\n");
    fprintf(stderr, "  -k Ignore modifier keys when monitoring keyboard activity.\n");
    fprintf(stderr, "  -K Like -k but also ignore Modifier+Key combos.\n");
    fprintf(stderr, "  -R Don't use the XRecord extension.\n");
    exit(1);
}

static int
enable_touchpad(void)
{
    int ret = 0;
    if (pad_disabled) {
	synshm->touchpad_off = 0;
	pad_disabled = 0;
	ret = 1;
    }
    return ret;
}

static void
signal_handler(int signum)
{
    enable_touchpad();
    if (pid_file)
	unlink(pid_file);
    kill(getpid(), signum);
}

static void
install_signal_handler(void)
{
    static int signals[] = {
	SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
	SIGBUS, SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE,
	SIGALRM, SIGTERM,
#ifdef SIGPWR
	SIGPWR
#endif
    };
    int i;
    struct sigaction act;
    sigset_t set;

    sigemptyset(&set);
    act.sa_handler = signal_handler;
    act.sa_mask = set;
#ifdef SA_ONESHOT
    act.sa_flags = SA_ONESHOT;
#else
    act.sa_flags = 0;
#endif

    for (i = 0; i < sizeof(signals) / sizeof(int); i++) {
	if (sigaction(signals[i], &act, NULL) == -1) {
	    perror("sigaction");
	    exit(2);
	}
    }
}

/**
 * Return non-zero if the keyboard state has changed since the last call.
 */
static int
keyboard_activity(Display *display)
{
    static unsigned char old_key_state[KEYMAP_SIZE];
    unsigned char key_state[KEYMAP_SIZE];
    int i;
    int ret = 0;

    XQueryKeymap(display, (char*)key_state);

    for (i = 0; i < KEYMAP_SIZE; i++) {
	if ((key_state[i] & ~old_key_state[i]) & keyboard_mask[i]) {
	    ret = 1;
	    break;
	}
    }
    if (ignore_modifier_combos) {
	for (i = 0; i < KEYMAP_SIZE; i++) {
	    if (key_state[i] & ~keyboard_mask[i]) {
		ret = 0;
		break;
	    }
	}
    }
    for (i = 0; i < KEYMAP_SIZE; i++)
	old_key_state[i] = key_state[i];
    return ret;
}

/**
 * Return non-zero if any physical touchpad button is currently pressed.
 */
static int
touchpad_buttons_active(void)
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

static double
get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void
main_loop(Display *display, double idle_time, int poll_delay)
{
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
	    if (enable_touchpad()) {
		if (!background)
		    printf("Enable\n");
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

static void
clear_bit(unsigned char *ptr, int bit)
{
    int byte_num = bit / 8;
    int bit_num = bit % 8;
    ptr[byte_num] &= ~(1 << bit_num);
}

static void
setup_keyboard_mask(Display *display, int ignore_modifier_keys)
{
    XModifierKeymap *modifiers;
    int i;

    for (i = 0; i < KEYMAP_SIZE; i++)
	keyboard_mask[i] = 0xff;

    if (ignore_modifier_keys) {
	modifiers = XGetModifierMapping(display);
	for (i = 0; i < 8 * modifiers->max_keypermod; i++) {
	    KeyCode kc = modifiers->modifiermap[i];
	    if (kc != 0)
		clear_bit(keyboard_mask, kc);
	}
	XFreeModifiermap(modifiers);
    }
}

/* ---- the following code is for using the xrecord extension ----- */
#ifdef HAVE_XRECORD

#define MAX_MODIFIERS 16

/* used for exchanging information with the callback function */
struct xrecord_callback_results {
    XModifierKeymap *modifiers;
    Bool key_event;
    Bool non_modifier_event;
    KeyCode pressed_modifiers[MAX_MODIFIERS];
};

/* test if the xrecord extension is found */
Bool check_xrecord(Display *display) {

    Bool   found;
    Status status;
    int    major_opcode, minor_opcode, first_error;
    int    version[2];

    found = XQueryExtension(display,
			    "RECORD",
			    &major_opcode,
			    &minor_opcode,
			    &first_error);

    status = XRecordQueryVersion(display, version, version+1);
    if (!background && status) {
	printf("X RECORD extension version %d.%d\n", version[0], version[1]);
    }
    return found;
}

/* called by XRecordProcessReplies() */
void xrecord_callback( XPointer closure, XRecordInterceptData* recorded_data) {

    struct xrecord_callback_results *cbres;
    xEvent *xev;
    int nxev;

    cbres = (struct xrecord_callback_results *)closure;
    /*printf("something happend, category=%d\n", recorded_data->category); */

    if (recorded_data->category != XRecordFromServer) {
	XRecordFreeData(recorded_data);
	return;
    }

    nxev = recorded_data->data_len / 8;
    xev = (xEvent *)recorded_data->data;
    while(nxev--) {

	if ( (xev->u.u.type == KeyPress) || (xev->u.u.type == KeyRelease)) {
	    int i;
	    int is_modifier = 0;

	    cbres->key_event = 1; /* remember, a key was pressed. */

	    /* test if it was a modifier */
	    for (i = 0; i < 8 * cbres->modifiers->max_keypermod; i++) {
		KeyCode kc = cbres->modifiers->modifiermap[i];

		if (kc == xev->u.u.detail) {
		    is_modifier = 1; /* yes, it is a modifier. */
		    break;
		}
	    }

	    if (is_modifier) {
		if (xev->u.u.type == KeyPress) {
		    for (i=0; i < MAX_MODIFIERS; ++i)
			if (!cbres->pressed_modifiers[i]) {
			    cbres->pressed_modifiers[i] = xev->u.u.detail;
			    break;
			}
		} else { /* KeyRelease */
		    for (i=0; i < MAX_MODIFIERS; ++i)
			if (cbres->pressed_modifiers[i] == xev->u.u.detail)
			    cbres->pressed_modifiers[i] = 0;
		}

	    } else {
		/* remember, a non-modifier was pressed. */
		cbres->non_modifier_event = 1;
	    }
	}

	xev++;
    }

    XRecordFreeData(recorded_data); /* cleanup */
}

static int is_modifier_pressed(const struct xrecord_callback_results *cbres) {
    int i;

    for (i = 0; i < MAX_MODIFIERS; ++i)
	if (cbres->pressed_modifiers[i])
	    return 1;

    return 0;
}

void record_main_loop(Display* display, double idle_time) {

    struct xrecord_callback_results cbres;
    XRecordContext context;
    XRecordClientSpec cspec = XRecordAllClients;
    Display *dpy_data;
    XRecordRange *range;
    int i;

    pad_disabled = 0;

    dpy_data = XOpenDisplay(NULL); /* we need an additional data connection. */
    range  = XRecordAllocRange();

    range->device_events.first = KeyPress;
    range->device_events.last  = KeyRelease;

    context =  XRecordCreateContext(dpy_data, 0,
				    &cspec,1,
				    &range, 1);

    XRecordEnableContextAsync(dpy_data, context, xrecord_callback, (XPointer)&cbres);

    cbres.modifiers  = XGetModifierMapping(display);
    /* clear list of modifiers */
    for (i = 0; i < MAX_MODIFIERS; ++i)
	cbres.pressed_modifiers[i] = 0;

    while (1) {

	int fd = ConnectionNumber(dpy_data);
	fd_set read_fds;
	int ret;
	int disable_event = 0;
	struct timeval timeout;

	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);

	ret = select(fd+1, &read_fds, NULL, NULL,
		     pad_disabled ? &timeout : NULL /* timeout only required for enabling */ );

	if (FD_ISSET(fd, &read_fds)) {

	    cbres.key_event = 0;
	    cbres.non_modifier_event = 0;

	    XRecordProcessReplies(dpy_data);

	    if (!ignore_modifier_keys && cbres.key_event) {
		disable_event = 1;
	    }

	    if (cbres.non_modifier_event &&
		!(ignore_modifier_combos && is_modifier_pressed(&cbres)) ) {
		disable_event = 1;
	    }
	}

	if (disable_event) {
	    /* adjust the enable_time */
	    timeout.tv_sec  = (int)idle_time;
	    timeout.tv_usec = (idle_time-(double)timeout.tv_sec) * 100000.;

	    if (!pad_disabled) {
		pad_disabled=1;
		if (!background) printf("disable touchpad\n");

		if (!synshm->touchpad_off)
		    synshm->touchpad_off = disable_taps_only ? 2 : 1;
	    }
	}

	if (ret == 0 && pad_disabled) { /* timeout => enable event */
	    enable_touchpad();
	    if (!background) printf("enable touchpad\n");
	}

    } /* end while(1) */

    XFreeModifiermap(cbres.modifiers);
}
#endif // HAVE_XRECORD

int
main(int argc, char *argv[])
{
    double idle_time = 2.0;
    int poll_delay = 200000;	    /* 200 ms */
    Display *display;
    int c;
    int shmid;
    int use_xrecord = 1;


    /* Parse command line parameters */
    while ((c = getopt(argc, argv, "i:m:dtp:kKR?")) != EOF) {
	switch(c) {
	case 'i':
	    idle_time = atof(optarg);
	    break;
	case 'm':
	    poll_delay = atoi(optarg) * 1000;
	    break;
	case 'd':
	    background = 1;
	    break;
	case 't':
	    disable_taps_only = 1;
	    break;
	case 'p':
	    pid_file = optarg;
	    break;
	case 'k':
	    ignore_modifier_keys = 1;
	    break;
	case 'K':
	    ignore_modifier_combos = 1;
	    ignore_modifier_keys = 1;
	    break;
	case 'R':
	    use_xrecord = 0;
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
	if (pid_file) {
	    FILE *fd = fopen(pid_file, "w");
	    if (!fd) {
		perror("Can't create pid file");
		exit(2);
	    }
	    fprintf(fd, "%d\n", getpid());
	    fclose(fd);
	}
    }
#ifdef HAVE_XRECORD
    if (use_xrecord && check_xrecord(display)) {
	record_main_loop(display, idle_time);
    } else
#endif HAVE_XRECORD
      {
	setup_keyboard_mask(display, ignore_modifier_keys);

	/* Run the main loop */
	main_loop(display, idle_time, poll_delay);
      }
    return 0;
}
