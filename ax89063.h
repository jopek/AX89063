#ifndef __AX89063_H__
#define __AX89063_H__
#define __AX89063_H__

#include "lcd.h"


// Use hardcoded interface for now
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
