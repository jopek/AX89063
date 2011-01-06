#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "report.h"
#include "ax89063.h"


// Use hardcoded interface for now
#define AX89063_DEFAULT_DEVICE "/dev/ttyS0" //TODO
#define AX89063_DEAFULT_SPEED B9600 
#define AX89063_WIDTH //TODO
#define AX89063_HEIGHT //TODO
#define AX89063_CELLWIDTH //TODO
#define AX89063_CELLHEIGHT //TODO

typedef struct driver_private_data {
	char device[256];
	int speed;
	int fd;
	int width;
	int height;
	int cellwidth;
	int cellheight;
	char *framebuf;
} PrivateData;

MODULE_EXPORT char * api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 1;
MODULE_EXPORT int supports_multiple = 0;
MODULE_EXPORT char *symbol_prefix = "ax89063_";

MODULE_EXPORT int
ax89063_init(Driver *drvthis) {
  PrivateData *p;
  struct termios portset;

  p = (PrivateData *) calloc(1, sizeof(PrivateData));
  if ( (p == NULL) || (drvthis->store_private_ptr(drvthis, p)) )
    return -1;

  p->fd         = -1; 
  p->framebuf   = NULL;
  p->speed      = B9600;
  p->width      = AX89063_WIDTH;
  p->height     = AX89063_HEIGHT;
  p->cellwidth  = AX89063_CELLWIDTH;
  p->cellheight = AX89063_CELLHEIGHT;

  /* Get device name, use default if it cannot be retrieved.*/
  strncpy(p->device, drvthis->config_get_string(drvthis->name, "Device", 0,
        AX89063_DEFAULT_DEVICE), sizeof(p->device));
  p->device[sizeof(p->device)-1] = '\0';
  report(RPT_INFO, "%s: using Device %s", drvthis->name, p->device);

  /* Get device speed. */
  if (p->speed != AX89063_DEFAULT_SPEED)
  {
      report(RPT_WARNING, "%s: Illegal speed (%d) detected in config file. Using default (%d).", drvthis->name, p->speed, AX89063_DEFAULT_SPEED);
  } else 
  {
    p->speed = AX89063_DEFAULT_SPEED;
  }
  
  /* Set up serial port and open it. */
  p->fd = open(p->device, 0_RDWR | 0_NOCTTY, 0_NDELAY);
  if (p->fd == -1) {
      report(RPT_ERR, "AX89063: serial: could not open device %s (%s)", device, strerror(errno));
      return -1;
  }

  /* Get serial device parameters */
  tcgetattr(f->fd, &portset);
  
  /* RAW mode TODO: why? */
  #ifdef HAVE_CFMAKERAW
    cfmakeraw(&portset);
  #else
    portset.c_iflag &= ~( IGNBRK | BRKINT | PARMRK | ISTRIP
                          | INLCR | IGNCR | ICRNL | IXON );
    portset.c_oflag &= ~OPOST;
    portset.c_lflag &= ~( ECHO | ECHONL | ICANON | ISIG | IEXTEN );
    portset.c_cflag &= ~( CSIZE | PARENB | CRTSCTS );
    portset.c_cflag |= CS8 | CREAD | CLOCAL ;
  #endif

  /* Set port speed */
  cfsetospeed(&portset, p->speed);
  cfsetispeed(&portset, p->speed);
}


MODULE_EXPORT void
ax89063_flush(Driver *drvthis) {
  PrivateData *p = drvthis->private_data;
  
  // Flush all 80 chars at once
  write(p->fd, p->framebuf, 80);
}

MODULE_EXPORT void
ax89063_close(Driver *drvthis) {
  PrivateData *p = drvthis->private_data;

  if (p != NULL) {
    if (p->fd >= 0) {
      close(p->fd);
    }

    if (p->framebuf != NULL)
      free(p->framebuf);

    free(p);
  }
  drvthis-> store_private_ptr(drvthis, NULL);
}
