/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



#define VIDEO_RAM_SIZE 0x400

unsigned char *junglek_videoram2,*junglek_videoram3;
unsigned char *junglek_characterram;
unsigned char *junglek_scrollx1,*junglek_scrollx2,*junglek_scrollx3;
unsigned char *junglek_scrolly1,*junglek_scrolly2,*junglek_scrolly3;
unsigned char *junglek_gfxpointer,*junglek_paletteram;
unsigned char *junglek_colorbank,*junglek_video_enable;
static struct osd_bitmap *tmpbitmap2,*tmpbitmap3;
static const unsigned char *colors;
static int dirtypalette,dirtycolor;
static unsigned char dirtycharacter1[256],dirtycharacter2[256];
static unsigned char dirtysprite[64];



/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/
void junglek_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom)
{
	int i;


	colors = color_prom;	/* we'll need the colors later to dynamically remap the characters */

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		bit0 = (~color_prom[2*i+1] >> 6) & 0x01;
		bit1 = (~color_prom[2*i+1] >> 7) & 0x01;
		bit2 = (~color_prom[2*i] >> 0) & 0x01;
		palette[3*i] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (~color_prom[2*i+1] >> 3) & 0x01;
		bit1 = (~color_prom[2*i+1] >> 4) & 0x01;
		bit2 = (~color_prom[2*i+1] >> 5) & 0x01;
		palette[3*i + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (~color_prom[2*i+1] >> 0) & 0x01;
		bit1 = (~color_prom[2*i+1] >> 1) & 0x01;
		bit2 = (~color_prom[2*i+1] >> 2) & 0x01;
		palette[3*i + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}


	for (i = 0;i < Machine->drv->total_colors;i++)
		colortable[19*8 + i] = i;


	/* set up colors for the dip switch menu */
	for (i = 0;i < 8*3;i++)
		colortable[16*8 + i] = 0;
	colortable[16*8 + 7] = 167;	/* white */
	colortable[17*8 + 7] = 161;	/* yellow */
	colortable[18*8 + 7] = 129;	/* red */
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int junglek_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((tmpbitmap2 = osd_create_bitmap(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	if ((tmpbitmap3 = osd_create_bitmap(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		osd_free_bitmap(tmpbitmap2);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void junglek_vh_stop(void)
{
	osd_free_bitmap(tmpbitmap3);
	osd_free_bitmap(tmpbitmap2);
	generic_vh_stop();
}



int junglek_gfxrom_r(int offset)
{
	int offs;


	offs = junglek_gfxpointer[0]+junglek_gfxpointer[1]*256;

	junglek_gfxpointer[0]++;
	if (junglek_gfxpointer[0] == 0) junglek_gfxpointer[1]++;

	if (offs < 0x8000)
		return Machine->memory_region[2][offs];
	else return 0;
}



void junglek_videoram2_w(int offset,int data)
{
	if (junglek_videoram2[offset] != data)
	{
		dirtybuffer[offset] = 1;

		junglek_videoram2[offset] = data;
	}
}



void junglek_videoram3_w(int offset,int data)
{
	if (junglek_videoram3[offset] != data)
	{
		dirtybuffer[offset] = 1;

		junglek_videoram3[offset] = data;
	}
}



void junglek_paletteram_w(int offset,int data)
{
	if (junglek_paletteram[offset] != data)
	{
		dirtypalette = 1;

		junglek_paletteram[offset] = data;
	}
}



extern void junglek_colorbank_w(int offset,int data)
{
	if (junglek_colorbank[offset] != data)
	{
		dirtycolor = 1;

		junglek_colorbank[offset] = data;
	}
}



void junglek_characterram_w(int offset,int data)
{
	if (junglek_characterram[offset] != data)
	{
		if (offset < 0x1800)
		{
			dirtycharacter1[(offset / 8) & 0xff] = 1;
			dirtysprite[(offset / 32) & 0x3f] = 1;
		}
		else
			dirtycharacter2[(offset / 8) & 0xff] = 1;

		junglek_characterram[offset] = data;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void junglek_vh_screenrefresh(struct osd_bitmap *bitmap)
{
	int offs,i;
	extern struct GfxLayout junglek_charlayout,junglek_spritelayout;


	if (*junglek_video_enable == 0)
	{
		clearbitmap(bitmap);
		return;
	}


	/* decode modified characters */
	for (offs = 0;offs < 256;offs++)
	{
		if (dirtycharacter1[offs] == 1)
		{
			decodechar(Machine->gfx[1],offs,junglek_characterram,&junglek_charlayout);
			dirtycharacter1[offs] = 0;
		}
		if (dirtycharacter2[offs] == 1)
		{
			decodechar(Machine->gfx[3],offs,junglek_characterram + 0x1800,&junglek_charlayout);
			dirtycharacter2[offs] = 0;
		}
	}
	/* decode modified sprites */
	for (offs = 0;offs < 64;offs++)
	{
		if (dirtysprite[offs] == 1)
		{
			decodechar(Machine->gfx[2],offs,junglek_characterram,&junglek_spritelayout);
			dirtysprite[offs] = 0;
		}
	}


	/* if the palette has changed, rebuild the color lookup table */
	if (dirtypalette)
	{
		dirtypalette = 0;

		for (i = 0;i < 8*8;i++)
		{
			int col;


			offs = 0;
			while (offs < Machine->drv->total_colors)
			{
				if ((junglek_paletteram[2*i] & 1) == colors[2*offs] &&
						junglek_paletteram[2*i+1] == colors[2*offs+1])
					break;

				offs++;
			}

			col = Machine->gfx[4]->colortable[offs];
			/* avoid undesired transparency */
			if (col == 0 && i % 8 != 0) col = 1;
			Machine->gfx[1]->colortable[i] = col;
			if (i % 8 == 0) col = 0;	/* create also an alternate color code with transparent pen 0 */
			Machine->gfx[1]->colortable[i+8*8] = col;
		}

		/* redraw everything */
		for (offs = 0;offs < VIDEO_RAM_SIZE;offs++) dirtybuffer[offs] = 1;
	}


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = 0;offs < VIDEO_RAM_SIZE;offs++)
	{
		if (dirtycolor || dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = 8 * (offs % 32);
			sy = 8 * (offs / 32);

			drawgfx(tmpbitmap3,Machine->gfx[1],
					junglek_videoram3[offs],
					junglek_colorbank[1] & 0x0f,
					0,0,sx,sy,
					0,TRANSPARENCY_NONE,0);
			drawgfx(tmpbitmap2,Machine->gfx[1],
					junglek_videoram2[offs],
					((junglek_colorbank[0] >> 4) & 0x0f) + 8,	/* use transparent pen 0 */
					0,0,sx,sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	dirtycolor = 0;


	/* copy the first playfield */
	{
		int scrollx,scrolly[32];


		scrollx = *junglek_scrollx3;
		scrollx = -((scrollx & 0xf8) | ((scrollx-1) & 7)) - 10;
		for (i = 0;i < 32;i++)
//			scrolly[i] = -junglek_scrolly3[i];
			scrolly[i] = -junglek_scrolly3[i] + 16;

		copyscrollbitmap(bitmap,tmpbitmap3,1,&scrollx,32,scrolly,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
	}

	/* copy the second playfield */
	{
		int scrollx,scrolly[32];


		scrollx = *junglek_scrollx2;
		scrollx = -((scrollx & 0xf8) | ((scrollx+1) & 7)) - 10;
		for (i = 0;i < 32;i++)
//			scrolly[i] = -junglek_scrolly2[i];
			scrolly[i] = -junglek_scrolly2[i] + 16;

		copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,32,scrolly,&Machine->drv->visible_area,TRANSPARENCY_COLOR,Machine->background_pen);
	}


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = 31*4;offs >= 0;offs -= 4)
	{
		drawgfx(bitmap,Machine->gfx[(spriteram[offs + 3] & 0x40) ? 3 : 2],
				spriteram[offs + 3] & 0x3f,
				2 * ((junglek_colorbank[1] >> 4) & 0x0f) + ((spriteram[offs + 2] >> 2) & 1),
				spriteram[offs + 2] & 1,0,
				((spriteram[offs]+13)&0xff)-15,240-spriteram[offs + 1],
				&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = 0;offs < VIDEO_RAM_SIZE;offs++)
	{
		if (videoram[offs] < 160)	/* don't draw spaces */
		{
			int sx,sy;


			sx = offs % 32;
			sy = (8*(offs / 32) + junglek_scrolly1[sx]) & 0xff;
//			sx = (8*sx + *junglek_scrollx1) & 0xff;
			sx = 8*sx;

			drawgfx(bitmap,Machine->gfx[3],
					videoram[offs],
					junglek_colorbank[0] & 0x0f,
					0,0,sx,sy,
					&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
