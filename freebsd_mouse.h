#ifndef _FREEBSD_MOUSE_H_
#define _FREEBSD_MOUSE_H_


typedef struct mousehw {
    int buttons;            /* -1 if unknown */
    int iftype;             /* MOUSE_IF_XXX */
    int type;               /* mouse/track ball/pad... */
    int model;              /* I/F dependent model ID: MOUSE_MODEL_XXX */
    int hwid;               /* I/F dependent hardware ID
			     * for the PS/2 mouse, it will be PSM_XXX_ID
			     */
} mousehw_t;

/* ioctls */
#define MOUSE_GETSTATUS      _IOR('M', 0, mousestatus_t)
#define MOUSE_GETHWINFO      _IOR('M', 1, mousehw_t)
#define MOUSE_GETMODE        _IOR('M', 2, mousemode_t)
#define MOUSE_SETMODE        _IOW('M', 3, mousemode_t)
#define MOUSE_GETLEVEL       _IOR('M', 4, int)
#define MOUSE_SETLEVEL       _IOW('M', 5, int)
#define MOUSE_GETVARS        _IOR('M', 6, mousevar_t)
#define MOUSE_SETVARS        _IOW('M', 7, mousevar_t)
#define MOUSE_READSTATE      _IOWR('M', 8, mousedata_t)
#define MOUSE_READDATA       _IOWR('M', 9, mousedata_t)
#define MOUSE_SYN_GETHWINFO  _IOR('M', 100, synapticshw_t)


typedef struct synapticshw {
  int infoMajor;
  int infoMinor;
  int infoRot180;
  int infoPortrait;
  int infoSensor;
  int infoHardware;
  int infoNewAbs;
  int capPen;
  int infoSimplC;
  int infoGeometry;
  int capExtended;
  int capSleep;
  int capFourButtons;
  int capMultiFinger;
  int capPalmDetect;
  int capPassthrough;
} synapticshw_t;


#define MOUSE_MODEL_SYNAPTICS	13

/* Synaptics Touchpad */
#define MOUSE_SYNAPTICS_PACKETSIZE	6


#endif /* _FREEBSD_MOUSE_H_ */
