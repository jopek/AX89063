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


 Copyright (C) 2011, Alexander Bluem <bluem [at] gmit-gmbh [dot] de>,
                                     <alex [at] binarchy [dot] net>
 Copyright (C) 2010, Kai Falkenberg <kai [at] layer0 [dot] de>

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

#ifndef __AX89063_H__
#define __AX89063_H__

#include "lcd.h"

#define AX89063_DEFAULT_DEVICE "/dev/ttyS1"
#define AX89063_SPEED B9600
#define AX89063_WIDTH 16
#define AX89063_HEIGHT 2
#define AX89063_CELLWIDTH 5
#define AX89063_CELLHEIGHT 7
#define AX89063_HWFRAMEBUFLEN 80

MODULE_EXPORT int  ax89063_init(Driver *drvthis);
MODULE_EXPORT void ax89063_close(Driver *drvthis);
MODULE_EXPORT int  ax89063_width(Driver *drvthis);
MODULE_EXPORT int  ax89063_height(Driver *drvthis);
MODULE_EXPORT int  ax89063_cellwidth(Driver *drvthis);
MODULE_EXPORT int  ax89063_cellheight(Driver *drvthis);
MODULE_EXPORT void ax89063_clear(Driver *drvthis);
MODULE_EXPORT void ax89063_flush(Driver *drvthis);
MODULE_EXPORT void ax89063_string(Driver *drvthis, int x, int y, const char string[]);
MODULE_EXPORT void ax89063_chr(Driver *drvthis, int x, int y, char c);

MODULE_EXPORT const char *ax89063_get_key(Driver *drvthis);

#endif //__AX89063_H__
