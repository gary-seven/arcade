/***************************************************************************

Krull's memory map

Main processor (8088 minimum mode) memory map.
0000-0fff RAM
1000-1fff ROM
2000-2fff ROM
3000-37ff sprite programmation (64 sprites)
3800-3fff background ram (32x30 chars)
4000-4fff background object ram ?
5000-57ff palette ram (palette of 16 colors)
5800-5fff i/o ports (see below)
6000-ffff ROM

memory mapped ports:

read:
5800    Dip switch
5801    Inputs 10-17
5802    trackball input (optional)
5803    trackball input (optional)
5804    Inputs 40-47

write:
5800    watchdog timer clear
5801    trackball output (optional)
5802    Outputs 20-27
5803    Flipflop outputs:
		b7: F/B priority
		b6: horiz. flipflop
		b5: vert. flipflop
		b4: Output 33
		b3: coin meter
		b2: knocker
		b1: coin 1
		b0: coin lockout
5804    Outputs 40-47

interrupts:
INTR not connected
NMI connected to vertical blank

Sound processor (6502) memory map:
0000-0fff RIOT (6532)
1000-1fff amplitude DAC
2000-2fff SC01 voice chip
3000-3fff voice clock DAC
4000-4fff socket expansion
5000-5fff socket expansion
6000-6fff socket expansion
7000-7fff PROM
(repeated in 8000-ffff, A15 only used in socket expansion)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern int krull_vh_start(void);
extern void gottlieb_vh_init_basic_color_palette(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
extern void gottlieb_sh_w(int offset, int data);
extern void gottlieb_sh_update(void);
extern const char *gottlieb_sample_names[];
extern void gottlieb_output(int offset, int data);
extern int krull_IN1_r(int offset);
extern unsigned char *gottlieb_videoram;
extern unsigned char *gottlieb_paletteram;
extern unsigned char *gottlieb_spriteram;
extern unsigned char *gottlieb_characterram;
extern void gottlieb_videoram_w(int offset,int data);
extern void gottlieb_paletteram_w(int offset,int data);
extern void gottlieb_characterram_w(int offset, int data);
extern void gottlieb_vh_screenrefresh(struct osd_bitmap *bitmap);


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x2fff, MRA_ROM },
	{ 0x3000, 0x57ff, MRA_RAM },
	{ 0x5800, 0x5800, input_port_0_r },     /* DSW */
	{ 0x5801, 0x5801, krull_IN1_r },     /* buttons */
	{ 0x5802, 0x5802, input_port_2_r },     /* trackball: not used */
	{ 0x5803, 0x5803, input_port_3_r },     /* trackball: not used */
	{ 0x5804, 0x5804, input_port_4_r },     /* joystick */
	{ 0x6000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x2fff, MWA_ROM },
	{ 0x3000, 0x37ff, MWA_RAM, &gottlieb_spriteram },
	{ 0x3800, 0x3fff, gottlieb_videoram_w, &gottlieb_videoram },
	{ 0x4000, 0x4fff, gottlieb_characterram_w, &gottlieb_characterram },
	{ 0x5000, 0x57ff, gottlieb_paletteram_w, &gottlieb_paletteram },
	{ 0x5800, 0x5800, MWA_RAM },    /* watchdog timer clear */
	{ 0x5801, 0x5801, MWA_RAM },    /* trackball: not used */
	{ 0x5802, 0x5802, gottlieb_sh_w }, /* sound/speech command */
	{ 0x5803, 0x5803, gottlieb_output },       /* OUT1 */
	{ 0x5804, 0x5804, MWA_RAM },    /* OUT2 */
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct InputPort input_ports[] =
{
	{       /* DSW */
		0x00,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{       /* buttons */
		0x01,
		{ 0,                    /* diag mode */
		  OSD_KEY_F2,            /* select */
		  OSD_KEY_3, OSD_KEY_4, /* coin 1 & 2 */
		  0,0,                  /* not connected ? */
		  OSD_KEY_1,
		  OSD_KEY_2 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{       /* trackball: not used */
		0xff,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{       /* trackball: not used */
		0xff,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{       /* joysticks */
		0x00,
		{
		OSD_KEY_E,OSD_KEY_F,OSD_KEY_D,OSD_KEY_S,
		OSD_KEY_UP,OSD_KEY_RIGHT,OSD_KEY_DOWN,OSD_KEY_LEFT},
		{ OSD_JOY_UP, OSD_JOY_RIGHT, OSD_JOY_DOWN, OSD_JOY_LEFT,
					0, 0, 0, 0 }
	},
	{ -1 }  /* end of table */
};

static struct KEYSet keys[] =
{
        { 4, 4, "MOVE UP" },
        { 4, 7, "MOVE LEFT"  },
        { 4, 5, "MOVE RIGHT" },
        { 4, 6, "MOVE DOWN" },
        { 4, 0, "FIRE UP" },
        { 4, 3, "FIRE LEFT"  },
        { 4, 1, "FIRE RIGHT" },
        { 4, 2, "FIRE DOWN" },
        { -1 }
};


static struct DSW dsw[] =
{
	{ 0, 0x08, "LIVES PER GAME", { "3","5" } },
	{ 0, 0x01, "ATTRACT MODE SOUND", { "ON", "OFF" } },
	{ 0, 0x1C, "", {
		"1 PLAY FOR 1 COIN" , "1 PLAY FOR 2 COINS",
		"1 PLAY FOR 1 COIN" , "1 PLAY FOR 2 COINS",
		"2 PLAYS FOR 1 COIN", "FREE PLAY",
		"2 PLAYS FOR 1 COIN", "FREE PLAY"
		} },
	{ 0, 0x20, "HEXAGON", { "ROVING", "STATIONARY" } },
	{ 0, 0x02, "DIFFICULTY", { "NORMAL", "HARD" } },
	{ 0, 0xC0, "", {
		"LIFE AT 30000 THEN EVERY 50000",
		"LIFE AT 30000 THEN EVERY 30000",
		"LIFE AT 40000 THEN EVERY 50000",
		"LIFE AT 50000 THEN EVERY 75000"
		} },
	/*{ 1, 0x01, "TEST MODE", {"ON", "OFF"} },*/
	{ -1 }
};


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	{ 28, 24, 20, 16, 12, 8, 4, 0},
	32*8    /* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	256,    /* 256 sprites */
	4,      /* 4 bits per pixel */
	{ 0, 0x2000*8, 0x4000*8, 0x6000*8 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	{ 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout fakelayout =
{
	1,1,
	0,
	1,
	{ 0 },
	{ 0 },
	{ 0 },
	0
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 0, 0xa357, &charlayout, 16, 2 }, /* 1 "white" palette, 1 "yellow" palette */
	{ 0, 0x4000, &charlayout, 0, 1 }, /* 1 palette used by the game */
	{ 1, 0, &spritelayout, 0, 1 },
	{ 0, 0, &fakelayout, 3*16, 16 }, /* 256 colors */
	{ -1 } /* end of array */
};

static const struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_I86,
			5000000,        /* 5 Mhz */
			0,
			readmem,writemem,0,0,
			nmi_interrupt,1
		}
	},
	60,     /* frames / second */
	0,      /* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 30*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	256,256+3*16,        /* 256 for colormap, 1*16 for the game, 2*16 for the dsw menu. Silly, isn't it ? */
	gottlieb_vh_init_basic_color_palette,

	0,      /* init vh */
	krull_vh_start,
	generic_vh_stop,
	gottlieb_vh_screenrefresh,

	/* sound hardware */
	0,      /* samples */
	0,
	0,
	0,
	gottlieb_sh_update
};

ROM_START( krull_rom )
	ROM_REGION(0x10000)     /* 64k for code */
	ROM_LOAD( "ROM0.BIN",  0xe000, 0x2000 )
	ROM_LOAD( "ROM1.BIN",  0xc000, 0x2000 )
	ROM_LOAD( "ROM2.BIN",  0xa000, 0x2000 )
	ROM_LOAD( "ROM3.BIN",  0x8000, 0x2000 )
	ROM_LOAD( "ROM4.BIN",  0x6000, 0x2000 )
	ROM_LOAD( "RAM4.BIN",  0x2000, 0x1000 )
	ROM_LOAD( "RAM2.BIN",  0x1000, 0x1000 )

	ROM_REGION(0x8000)      /* temporary space for graphics */
	ROM_LOAD( "FG3.BIN",  0x0000, 0x2000 )       /* sprites */
	ROM_LOAD( "FG2.BIN",  0x2000, 0x2000 )       /* sprites */
	ROM_LOAD( "FG1.BIN",  0x4000, 0x2000 )       /* sprites */
	ROM_LOAD( "FG0.BIN",  0x6000, 0x2000 )       /* sprites */
ROM_END

static unsigned short krull_colors[256]={
	0x000, 0xfff, 0xff0,
	0x005, 0x006, 0x007, 0x008, 0x009, 0x00b, 0x00f,
	0x010, 0x017, 0x019, 0x01a, 0x020, 0x028, 0x02b, 0x030,
	0x039, 0x03b, 0x03c, 0x040, 0x04a, 0x04b, 0x04d, 0x05b,
	0x05e, 0x060, 0x06b, 0x06c, 0x06f, 0x070, 0x07b, 0x07d,
	0x07f, 0x080, 0x08b, 0x08e, 0x090, 0x09b, 0x0a0, 0x0af,
	0x0f0, 0x0ff, 0x100, 0x101, 0x104, 0x106, 0x110, 0x12a,
	0x150, 0x200, 0x202, 0x205, 0x207, 0x23b, 0x240, 0x260,
	0x290, 0x2a0, 0x2b0, 0x2c0, 0x2d0, 0x2e0, 0x300, 0x303,
	0x306, 0x308, 0x320, 0x340, 0x34c, 0x366, 0x370, 0x380,
	0x390, 0x3a0, 0x3b0, 0x400, 0x404, 0x407, 0x409, 0x433,
	0x440, 0x45d, 0x477, 0x480, 0x500, 0x505, 0x508, 0x50a,
	0x530, 0x533, 0x540, 0x549, 0x56e, 0x588, 0x600, 0x606,
	0x609, 0x60b, 0x61a, 0x633, 0x63a, 0x640, 0x64a, 0x65a,
	0x660, 0x66a, 0x67a, 0x67f, 0x68a, 0x699, 0x69a, 0x6aa,
	0x700, 0x707, 0x70a, 0x71c, 0x725, 0x730, 0x733, 0x740,
	0x750, 0x770, 0x7aa, 0x7f0, 0x800, 0x808, 0x80b, 0x82d,
	0x833, 0x880, 0x890, 0x8bb, 0x900, 0x904, 0x909, 0x90c,
	0x910, 0x914, 0x920, 0x924, 0x934, 0x93e, 0x944, 0x949,
	0x950, 0x980, 0x990, 0x9c3, 0x9cc, 0xa00, 0xa0a, 0xa0d,
	0xa10, 0xa15, 0xa20, 0xa25, 0xa30, 0xa35, 0xa40, 0xa45,
	0xa4f, 0xa50, 0xa55, 0xa60, 0xaa0, 0xac3, 0xadd, 0xb00,
	0xb30, 0xb32, 0xb40, 0xb41, 0xb52, 0xb60, 0xb61, 0xb80,
	0xbb0, 0xbbb, 0xbbf, 0xbc3, 0xbee, 0xc00, 0xc10, 0xc20,
	0xc30, 0xc40, 0xc55, 0xc56, 0xc80, 0xcb0, 0xcc0, 0xcc3,
	0xcff, 0xd00, 0xd10, 0xd20, 0xd30, 0xd40, 0xd50, 0xd60,
	0xd70, 0xd80, 0xd90, 0xdb0, 0xdc0, 0xdc3, 0xdd0, 0xe10,
	0xe20, 0xe61, 0xe80, 0xee0, 0xf00, 0xf07, 0xf10, 0xf20,
	0xf21, 0xf30, 0xf40, 0xf41, 0xf50, 0xf60, 0xf64, 0xf70,
	0xf72, 0xf73, 0xf80
};


struct GameDriver krull_driver =
{
	"krull",
	&machine_driver,

	krull_rom,
	0, 0,   /* rom decode and opcode decode functions */
	gottlieb_sample_names,

	input_ports, dsw, keys,

	(char *)krull_colors,
	0,0,    /* palette, colortable */

	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,    /* numbers */
		0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,       /* letters */
		0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23 },
	0,1,      /* white & yellow for dsw menu */
	8*11,8*20,1, /* paused message displayed at X,Y and color */

	0,0     /* hi-score load and save */
};
