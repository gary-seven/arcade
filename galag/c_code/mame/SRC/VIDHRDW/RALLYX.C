/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



#define VIDEO_RAM_SIZE 0x400

unsigned char *rallyx_videoram1;
unsigned char *rallyx_colorram1;
unsigned char *rallyx_videoram2;
unsigned char *rallyx_colorram2;
unsigned char *rallyx_scrollx;
unsigned char *rallyx_scrolly;
static unsigned char dirtybuffer1[VIDEO_RAM_SIZE];	/* keep track of modified portions of the screen */
											/* to speed up video refresh */
static unsigned char dirtybuffer2[VIDEO_RAM_SIZE];	/* keep track of modified portions of the screen */
											/* to speed up video refresh */
static struct osd_bitmap *tmpbitmap1;



static struct rectangle visiblearea =
{
	0*8, 28*8-1,
	0*8, 28*8-1
};

static struct rectangle radarvisiblearea =
{
	28*8, 36*8-1,
	0*8, 28*8-1
};



int rallyx_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((tmpbitmap1 = osd_create_bitmap(32*8,32*8)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void rallyx_vh_stop(void)
{
	osd_free_bitmap(tmpbitmap1);
	generic_vh_stop();
}



void rallyx_videoram1_w(int offset,int data)
{
	if (rallyx_videoram1[offset] != data)
	{
		dirtybuffer1[offset] = 1;

		rallyx_videoram1[offset] = data;
	}
}



void rallyx_colorram1_w(int offset,int data)
{
	if (rallyx_colorram1[offset] != data)
	{
		dirtybuffer1[offset] = 1;

		rallyx_colorram1[offset] = data;
	}
}



void rallyx_videoram2_w(int offset,int data)
{
	if (rallyx_videoram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		rallyx_videoram2[offset] = data;
	}
}



void rallyx_colorram2_w(int offset,int data)
{
	if (rallyx_colorram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		rallyx_colorram2[offset] = data;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void rallyx_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs,sx,sy;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = 0;offs < VIDEO_RAM_SIZE;offs++)
	{
		if (dirtybuffer2[offs])
		{
			dirtybuffer2[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap1,Machine->gfx[0],
					rallyx_videoram2[offs],
					rallyx_colorram2[offs] & 0x1f,
					!(rallyx_colorram2[offs] & 0x40),rallyx_colorram2[offs] & 0x80,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* update radar */
	for (sy = 0;sy < 32;sy++)
	{
		for (sx = 0;sx < 8;sx++)
		{
			offs = sy * 32 + sx;

			if (dirtybuffer1[offs])
			{
				dirtybuffer1[offs] = 0;

				drawgfx(tmpbitmap,Machine->gfx[0],
						rallyx_videoram1[offs],
						rallyx_colorram1[offs] & 0x1f,
						!(rallyx_colorram1[offs] & 0x40),rallyx_colorram1[offs] & 0x80,
						8 * ((sx ^ 4) + 28),8*(sy-2),
						&radarvisiblearea,TRANSPARENCY_NONE,0);
			}
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrollx,scrolly;


		scrollx = -(*rallyx_scrollx - 3);
		scrolly = -(*rallyx_scrolly + 16);

		copyscrollbitmap(bitmap,tmpbitmap1,1,&scrollx,1,&scrolly,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
	}


	/* radar */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&radarvisiblearea,TRANSPARENCY_NONE,0);


	/* draw the sprites */
	for (offs = 0;offs < 6*2;offs += 2)
	{
		drawgfx(bitmap,Machine->gfx[1],
				spriteram[offs] >> 2,spriteram_2[offs + 1],
				spriteram[offs] & 1,spriteram[offs] & 2,
				spriteram[offs + 1] - 1,224 - spriteram_2[offs],
				&visiblearea,TRANSPARENCY_PEN,0);
	}
}
