/***************************************************************************

  vidhrdw/generic.c

  Some general purpose functions used by many video drivers.

***************************************************************************/

#include "driver.h"
#include "generic.h"


unsigned char *videoram;
unsigned char *colorram;
unsigned char *spriteram;	/* not used in this module... */
unsigned char *spriteram_2;	/* ... */
unsigned char *spriteram_3;	/* ... here just for convenience */
unsigned char *dirtybuffer;
struct osd_bitmap *tmpbitmap;



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int generic_vh_start(void)
{
	int len;


	len = (Machine->drv->screen_width/8) * (Machine->drv->screen_height/8);
	/* round the length to the next 0x400 boundary. This is necessary to */
	/* allocate a buffer large enough for Pac Man and other games using */
	/* 224x288 and 288x224 video modes, but still a 32x32 video memory */
	/* layout, and for Popeye, using a 512x480 screen but a 64x64 memory. */
	len = (len + 0x3ff) & 0xfffffc00;

	if ((dirtybuffer = (unsigned char *)malloc(len)) == 0)
		return 1;
	memset(dirtybuffer,0,len);

	if ((tmpbitmap = osd_create_bitmap(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void generic_vh_stop(void)
{
	free(dirtybuffer);
	osd_free_bitmap(tmpbitmap);
}



int videoram_r(int offset)
{
	return videoram[offset];
}



int colorram_r(int offset)
{
	return colorram[offset];
}



void videoram_w(int offset,int data)
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset] = 1;

		videoram[offset] = data;
	}
}



void colorram_w(int offset,int data)
{
	if (colorram[offset] != data)
	{
		dirtybuffer[offset] = 1;

		colorram[offset] = data;
	}
}
