/** \file server/drivers/ax89063.c
 * LCDd \c ax89063 driver for AXIOMTEK 89063 LCDs..
 */

/*  This is the LCDproc driver for AXIOMTEK 89063 displays
 as found in the following 1U AXIOMTEK rack servers:
 NA-510
 NA-710
 NA-814
 NA-820
 NA-821
 NA-822
 ... and probably some more ;)

 The (rather useless) datasheet is available at
 <http://eservice.axiomtek.com.tw/attach_files/K0509-0016/AX89063%20LCM%20Control%20Spec.pdf>
 (backup at <http://bitdump.msquadrat.de/2010/ax89063/AX89063%20LCM%20Control%20Spec.pdf>).

 The protocol is simple: The AX89063 is a 16x2 display with a 40x2 character
 framebuffer. Only the first 16 characters of each line are displayed. A
 single 0x0d followed by the contents of the framebuffer trigger an update:
 "\x0d0123456789abcdef************************0123456789abcdef************************"

 The four buttons are encoded as (clockwise starting from top left): ULRD

 Copyright (C) 2011, Alexander Bluem <bluem [at] gmit-gmbh [dot] de>,
                                     <alex [at] binarchy [dot] net>
 Copyright (C) 2010, Kai Falkenberg <kai [at] layer0 [dot] de>
 Copyright (C) 2010, Malte S. Stretz <http://msquadrat.de>

 The code is derived from the ms6931 and bayrad driver

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 */

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
	char *framebuf;
	char *framebuf_hw;
	int framebuf_clr;
} PrivateData;

MODULE_EXPORT char * api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 1;
MODULE_EXPORT int supports_multiple = 0;
MODULE_EXPORT char *symbol_prefix = "ax89063_";


/**
 * Reset the framebuffer with spaces if clear() was called before. The
 * clearing of the screen is postponed to avoid hammering of the display
 * which makes the keypad unresponsible.
 * \param drvthis  Pointer to driver structure.
 */
static inline void ax89063_clear_if_needed(PrivateData *p) {
	memset(p->framebuf, ' ', p->width * p->height);
	p->framebuf_clr = 0;
}


/**
 * Initialize the driver.
 * \param drvthis  Pointer to driver structure.
 * \retval 0       Success.
 * \retval <0      Error.
 */
MODULE_EXPORT int ax89063_init(Driver *drvthis) {
	PrivateData *p;
	struct termios portset;

	p = (PrivateData *) calloc(1, sizeof(PrivateData));
	if ((p == NULL) || (drvthis->store_private_ptr(drvthis, p)))
		return -1;

	/* initialize private data */
	p->fd = -1;
	p->framebuf = NULL;
	p->framebuf_hw = NULL;
	p->framebuf_clr = 0;
	p->speed = AX89063_SPEED;
	p->width = AX89063_WIDTH;
	p->height = AX89063_HEIGHT;

	/* Read config file */
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

	/* Get and set serial device parameters */
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
	/* Initialize the framebuffer */
	ax89063_clear(drvthis);

	/* Make sure the frame buffer is there... */
	p->framebuf_hw = (char *) malloc(AX89063_HWFRAMEBUFLEN + 1);
	if (p->framebuf_hw == NULL) {
		report(RPT_ERR, "%s: unable to create LCD framebuffer", drvthis->name);
		return -1;
	}
	/* Encode the start byte */
	p->framebuf_hw[0] = (char) 0x0d;
	/* Initialize the framebuffer */
	memset(p->framebuf_hw + 1, '*', AX89063_HWFRAMEBUFLEN);

	return 0;
}

/**
 * Flush data on screen to the LCD.
 * \param drvthis  Pointer to driver structure.
 */
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

	ax89063_clear_if_needed(p);

	//p->width * p->height
	for (y = 0; y < p->height; y++)
		for (x = 0; x < p->width; x++)
			p->framebuf_hw[y * (AX89063_HWFRAMEBUFLEN / 2) + x + 1] = *(str++);

	if ((ret = select(FD_SETSIZE, NULL, &fdset, NULL, &selectTimeout)) < 0) {
		report(RPT_ERR, "%s: get_key: select() failed (%s)", drvthis->name,
				strerror(errno));
		return;
	}

	// select() timed out - unlikely
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

	// Flush all 81 chars at once
	result = write(p->fd, p->framebuf_hw, AX89063_HWFRAMEBUFLEN + 1);
}

/**
 * Close the driver (do necessary clean-up).
 * \param drvthis  Pointer to driver structure.
 */
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
	drvthis->store_private_ptr(drvthis, NULL);
}

/**
 * Return the display width in characters.
 * \param drvthis  Pointer to driver structure.
 * \return         Number of characters the display is wide.
 */
MODULE_EXPORT int ax89063_width(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->width;
}

/**
 * Return the display height in characters.
 * \param drvthis  Pointer to driver structure.
 * \return         Number of characters the display is high.
 */
MODULE_EXPORT int ax89063_height(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	return p->height;
}

/**
 * Return the width of a character in pixels.
 * \param drvthis  Pointer to driver structure.
 * \return         Number of pixel columns a character cell is wide.
 */
MODULE_EXPORT int ax89063_cellwidth(Driver *drvthis) {
	return AX89063_CELLWIDTH;
}

/**
 * Return the height of a character in pixels.
 * \param drvthis  Pointer to driver structure.
 * \return         Number of pixel lines a character cell is high.
 */
MODULE_EXPORT int ax89063_cellheight(Driver *drvthis) {
	return AX89063_CELLHEIGHT;
}

/**
 * Clear the screen. clear() actually clears the buffer by
 * filling it with spaces, 0x20.
 * \param drvthis  Pointer to driver structure.
 */
MODULE_EXPORT void ax89063_clear(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	p->framebuf_clr = 1;
}

/**
 * Print a string on the screen at position (x,y).
 * The upper-left corner is (1,1), the lower-right corner is (p->width, p->height).
 * \param drvthis  Pointer to driver structure.
 * \param x        Horizontal character position (column).
 * \param y        Vertical character position (row).
 * \param string   String that gets written.
 */
MODULE_EXPORT void ax89063_string(Driver *drvthis, int x, int y,
		const char string[]) {
	int i = 0;
	PrivateData *p = drvthis->private_data;

	/* Convert 1-based coords to 0-based... */
	x--;
	y--;

	if ((y < 0) || (y >= p->height))
		return;

	ax89063_clear_if_needed(p);
	for (i = 0; (string[i] != '\0') && (x < p->width); i++, x++) {
		/* Check for buffer overflows... */
		if (x >= 0)
			p->framebuf[(y * p->width) + x] = string[i];
	}
}

/**
 * Print a character on the screen at position (x,y).
 * The upper-left corner is (1,1), the lower-right corner is (p->width, p->height).
 * \param drvthis  Pointer to driver structure.
 * \param x        Horizontal character position (column).
 * \param y        Vertical character position (row).
 * \param c        Character that gets written.
 */
MODULE_EXPORT void ax89063_chr(Driver *drvthis, int x, int y, char c) {
	PrivateData *p = drvthis->private_data;
	y--;
	x--;

	ax89063_clear_if_needed(p);
	if ((x >= 0) && (y >= 0) && (x < p->width) && (y < p->height))
		p->framebuf[(y * p->width) + x] = c;
}

/**
 * Get key from the device.
 * \param drvthis  Pointer to driver structure.
 * \return         String representation of the key;
 *                 \c NULL if nothing available / unmapped key.
 */
MODULE_EXPORT const char *ax89063_get_key(Driver *drvthis) {
	PrivateData *p = drvthis->private_data;
	char key;
	char *str = NULL;
	int ret = 0;

	/* the timeout time is set high, as the display is unresponsive when
	 * just written to */
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
