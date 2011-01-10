#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lcd.h"
#include "report.h"
#include "ax89063.h"

typedef struct driver_private_data {
	char device[256];
	int speed;
	int fd;
	int width;
	int height;
	int cellwidth;
	int cellheight;
	char *framebuf;
	char *framebuf_hw;
} PrivateData;

MODULE_EXPORT char * api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 1;
MODULE_EXPORT int supports_multiple = 0;
MODULE_EXPORT char *symbol_prefix = "ax89063_";

MODULE_EXPORT int ax89063_init(Driver *drvthis) {
	PrivateData *p;
	struct termios portset;

	p = (PrivateData *) calloc(1, sizeof(PrivateData));
	if ((p == NULL) || (drvthis->store_private_ptr(drvthis, p)))
		return -1;

	p->fd = -1;
	p->framebuf = NULL;
	p->framebuf_hw = NULL;
	p->speed = AX89063_SPEED;
	p->width = AX89063_WIDTH;
	p->height = AX89063_HEIGHT;
	p->cellwidth = AX89063_CELLWIDTH;
	p->cellheight = AX89063_CELLHEIGHT;

	/* Get device name, use default if it cannot be retrieved.*/
	strncpy(p->device, drvthis->config_get_string(drvthis->name, "Device", 0,
			AX89063_DEFAULT_DEVICE), sizeof(p->device));
	p->device[sizeof(p->device) - 1] = '\0';
	report(RPT_INFO, "%s: using Device %s", drvthis->name, p->device);

	/* Get device speed. */
	if (p->speed != AX89063_SPEED) {
		report(
				RPT_WARNING,
				"%s: Illegal speed (%d) detected in config file. Using default (%d).",
				drvthis->name, p->speed, AX89063_SPEED);
	} else {
		p->speed = AX89063_SPEED;
	}

	/* Set up serial port and open it. */
	p->fd = open(p->device, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (p->fd == -1) {
		report(RPT_ERR, "AX89063: serial: could not open device %s (%s)",
				p->device, strerror(errno));
		return -1;
	}

	/* Get serial device parameters */
	tcgetattr(p->fd, &portset);
	cfsetospeed(&portset, p->speed);
	cfsetispeed(&portset, p->speed);

#ifdef HAVE_CFMAKERAW
	cfmakeraw(&portset);
#else
	portset.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR
			| ICRNL | IXON);
	portset.c_oflag &= ~OPOST;
	portset.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	portset.c_cflag &= ~(CSIZE | PARENB | CRTSCTS);
	portset.c_cflag |= CS8 | CREAD | CLOCAL;
#endif

	portset.c_cc[VMIN] = 0;
	portset.c_cc[VTIME] = 0;

	tcsetattr(p->fd, TCSANOW, &portset);

	/* Make sure the frame buffer is there... */
	p->framebuf = (char *) malloc(p->width * p->height);
	if (p->framebuf == NULL) {
		report(RPT_ERR, "%s: unable to create framebuffer", drvthis->name);
		return -1;
	}
	ax89063_clear(drvthis);

	/* Make sure the frame buffer is there... */
	p->framebuf_hw = (char *) malloc(AX89063_HWFRAMEBUFLEN + 1);
	if (p->framebuf_hw == NULL) {
		report(RPT_ERR, "%s: unable to create LCD framebuffer", drvthis->name);
		return -1;
	}

	report(RPT_DEBUG, "%s: init() done", drvthis->name);
	return 0;
}

MODULE_EXPORT void ax89063_flush(Driver *drvthis) {
	int x, y, result;
	PrivateData *p = drvthis->private_data;
	char *str = p->framebuf;
	static int csum_s = 0;
	int csum = 0;

	int ret = 0;
	struct timeval selectTimeout = { 0, 0 };
	fd_set fdset;

	FD_ZERO(&fdset);
	FD_SET(p->fd, &fdset);

	//p->width * p->height
	for (y = 0; y < p->height; y++)
		for (x = 0; x < p->width; x++)
			p->framebuf_hw[y * (AX89063_HWFRAMEBUFLEN / 2) + x + 1] = *(str++);
	p->framebuf_hw[0] = (char) 0x0d;

	if ((ret = select(FD_SETSIZE, NULL, &fdset, NULL, &selectTimeout)) < 0) {
		report(RPT_ERR, "%s: get_key: select() failed (%s)", drvthis->name,
				strerror(errno));
		return;
	}

	// select() timed out
	if (!ret) {
		FD_SET(p->fd, &fdset);
		return;
	}

	if (!FD_ISSET(p->fd, &fdset)) {
		report(RPT_INFO, "%s: flush: select() nothing was written",
				drvthis->name);
		return;
	}

	// Minimize the number of writing cycles to max the responsiveness
	for (x = 0; x < AX89063_HWFRAMEBUFLEN; x++)
		csum += p->framebuf_hw[x];

	if (csum == csum_s)
		return;

	csum_s = csum;

	// Flush all 80 chars at once
	result = write(p->fd, p->framebuf_hw, AX89063_HWFRAMEBUFLEN + 1);
}

MODULE_EXPORT void ax89063_close(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;

	if (p != NULL) {
		if (p->fd >= 0) {
			close(p->fd);
		}

		if (p->framebuf != NULL)
			free(p->framebuf);

		if (p->framebuf_hw != NULL)
			free(p->framebuf_hw);

		free(p);
	}
	report(RPT_DEBUG, "%s: close: freed all dynamically allocated memory",
			drvthis->name);
	drvthis->store_private_ptr(drvthis, NULL);
}

MODULE_EXPORT int ax89063_width(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->width;
}

MODULE_EXPORT int ax89063_height(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->height;
}

MODULE_EXPORT int ax89063_cellwidth(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->cellwidth;
}

MODULE_EXPORT int ax89063_cellheight(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->cellheight;
}

MODULE_EXPORT void ax89063_clear(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	memset(p->framebuf, ' ', p->width * p->height);
}

MODULE_EXPORT void ax89063_string(Driver *drvthis, int x, int y,
		const char string[]) {
	int i = 0;
	PrivateData *p = drvthis->private_data;

	/* Convert 1-based coords to 0-based... */
	x--;
	y--;

	if ((y < 0) || (y >= p->height))
		return;

	for (i = 0; (string[i] != '\0') && (x < p->width); i++, x++) {
		/* Check for buffer overflows... */
		if (x >= 0)
			p->framebuf[(y * p->width) + x] = string[i];
	}
}

MODULE_EXPORT void ax89063_chr(Driver *drvthis, int x, int y, char c) {
	PrivateData *p = drvthis->private_data;
	y--;
	x--;

	if ((x >= 0) && (y >= 0) && (x < p->width) && (y < p->height))
		p->framebuf[(y * p->width) + x] = c;
}

MODULE_EXPORT const char *ax89063_get_key(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	char key;
	char *str = NULL;
	int ret = 0;
	struct timeval selectTimeout = { 0, 500E3 };
	fd_set fdset;

	FD_ZERO(&fdset);
	FD_SET(p->fd, &fdset);

	if ((ret = select(FD_SETSIZE, &fdset, NULL, NULL, &selectTimeout)) < 0) {
		report(RPT_ERR, "%s: get_key: select() failed (%s)", drvthis->name,
				strerror(errno));
		return NULL;
	}

	// select() timed out
	if (!ret) {
		FD_SET(p->fd, &fdset);
		return NULL;
	}

	if (!FD_ISSET(p->fd, &fdset)) {
		report(RPT_INFO, "%s: get_key: select() no keys pressed", drvthis->name);
		return NULL;
	}

	if ((ret = read(p->fd, &key, 1)) < 0) {
		report(RPT_ERR, "%s: get_key: read() failed and returned %d (%s)",
				drvthis->name, ret, strerror(errno));
		return NULL;
	}

	if (ret == 1) {
		switch (key) {
		case 'U':
			str = "up";
			break;
		case 'D':
			str = "down";
			break;
		case 'L':
			str = "left";
			break;
		case 'R':
			str = "right";
			break;
		}
	}
	return str;
}
