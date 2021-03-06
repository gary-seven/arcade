/***************************************************************************

Amidar memory map (preliminary)

0000-3fff ROM (Amidar US version and Turtles: 0000-4fff)
8000-87ff RAM
9000-93ff Video RAM
9800-983f Screen attributes
9840-985f sprites


read:
a800      watchdog reset
b000      IN0
b010      IN1
b020      IN2
b820      DSW2 (not Turtles)

*
 * IN0 (all bits are inverted)
 * bit 7 : COIN 1
 * bit 6 : COIN 2
 * bit 5 : LEFT player 1
 * bit 4 : RIGHT player 1
 * bit 3 : FIRE player 1
 * bit 2 : CREDIT
 * bit 1 : ?
 * bit 0 : UP player 2 (TABLE only)
 *
*
 * IN1 (all bits are inverted)
 * bit 7 : START 1
 * bit 6 : START 2
 * bit 5 : LEFT player 2 (TABLE only)
 * bit 4 : RIGHT player 2 (TABLE only)
 * bit 3 : FIRE player 2 (TABLE only)
 * bit 2 : ?
 * bit 1 : ?
 * bit 0 : ?
*
 * IN2 (all bits are inverted)
 * bit 7 : ?
 * bit 6 : DOWN player 1
 * bit 5 : ?
 * bit 4 : UP player 1
 * bit 3 : COCKTAIL or UPRIGHT cabinet (0 = UPRIGHT)
 * bit 2 : bonus  0 = 30000 1 = 50000 (amidar)
 * bit 1 : ? (amidar)
 * bit 2 :\ coins per play
 * bit 1 :/ (turtles)
 * bit 0 : DOWN player 2 (TABLE only)
 *
*
 * DSW2 (all bits are inverted)
 * bit 7 :\
 * bit 6 :|  right coin slot
 * bit 5 :|
 * bit 4 :/  all 0 = free play
 * bit 3 :\
 * bit 2 :|  left coin slot
 * bit 1 :|
 * bit 0 :/
 *

write:
a008      interrupt enable
b800      To AY-3-8910 port A (commands for the audio CPU)
b810      bit 3 = interrupt trigger on audio CPU


SOUND BOARD:
0000-1fff ROM
8000-83ff RAM

I/0 ports:
read:
20      8910 #2  read
80      8910 #1  read

write
10      8910 #2  control
20      8910 #2  write
40      8910 #1  control
80      8910 #1  write

interrupts:
interrupt mode 1 triggered by the main CPU

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/generic.h"
#include "sndhrdw/8910intf.h"



extern unsigned char *amidar_attributesram;
extern void amidar_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
extern void amidar_attributes_w(int offset,int data);
extern void amidar_vh_screenrefresh(struct osd_bitmap *bitmap);

extern int amidar_sh_interrupt(void);
extern int amidar_sh_start(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9800, 0x985f, MRA_RAM },
	{ 0x0000, 0x4fff, MRA_ROM },
	{ 0xb000, 0xb000, input_port_0_r },	/* IN0 */
	{ 0xb010, 0xb010, input_port_1_r },	/* IN1 */
	{ 0xb020, 0xb020, input_port_2_r },	/* IN2 */
	{ 0xb820, 0xb820, input_port_3_r },	/* DSW2 */
	{ 0xa800, 0xa800, MRA_NOP },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x93ff, videoram_w, &videoram },
	{ 0x9800, 0x983f, amidar_attributes_w, &amidar_attributesram },
	{ 0x9840, 0x985f, MWA_RAM, &spriteram },
	{ 0x9860, 0x987f, MWA_NOP },
	{ 0xa008, 0xa008, interrupt_enable_w },
	{ 0xb800, 0xb800, sound_command_w },
	{ 0x0000, 0x4fff, MWA_ROM },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0x0000, 0x1fff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x8000, 0x83ff, MWA_RAM },
	{ 0x0000, 0x1fff, MWA_ROM },
	{ -1 }	/* end of table */
};



static struct IOReadPort sound_readport[] =
{
	{ 0x80, 0x80, AY8910_read_port_0_r },
	{ 0x20, 0x20, AY8910_read_port_1_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x40, 0x40, AY8910_control_port_0_w },
	{ 0x80, 0x80, AY8910_write_port_0_w },
	{ 0x10, 0x10, AY8910_control_port_1_w },
	{ 0x20, 0x20, AY8910_write_port_1_w },
	{ -1 }	/* end of table */
};



static struct InputPort input_ports[] =
{
	{	/* IN0 */
		0xff,
		{ 0, 0, OSD_KEY_3, OSD_KEY_CONTROL, OSD_KEY_RIGHT, OSD_KEY_LEFT, OSD_KEY_4, OSD_KEY_5 },
		{ 0, 0, 0, OSD_JOY_FIRE, OSD_JOY_RIGHT, OSD_JOY_LEFT, 0, 0 }
	},
	{	/* IN1 */
		0xff,
		{ 0, 0, 0, 0, 0, 0, OSD_KEY_2, OSD_KEY_1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* IN2 */
		0xf1,
		{ 0, 0, 0, 0, OSD_KEY_UP, 0, OSD_KEY_DOWN, 0 },
		{ 0, 0, 0, 0, OSD_JOY_UP, 0, OSD_JOY_DOWN, 0 }
	},
	{	/* DSW2 */
		0xff,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{ -1 }	/* end of table */
};


static struct KEYSet keys[] =
{
        { 2, 4, "MOVE UP" },
        { 0, 5, "MOVE LEFT"  },
        { 0, 4, "MOVE RIGHT" },
        { 2, 6, "MOVE DOWN" },
        { 0, 3, "BOMB" },
        { -1 }
};



static struct DSW amidar_dsw[] =
{
	{ 2, 0x04, "BONUS", { "30000", "50000" } },
	{ 2, 0x02, "SW2", { "OFF", "ON" } },
	{ 2, 0x20, "SW6", { "OFF", "ON" } },
	{ 2, 0x80, "SW8", { "OFF", "ON" } },
	{ -1 }
};



static struct DSW turtles_dsw[] =
{
	{ 2, 0x20, "SW6", { "OFF", "ON" } },
	{ 2, 0x80, "SW8", { "OFF", "ON" } },
	{ -1 }
};



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 64*16*16 },	/* the two bitplanes are separated */
	{ 23*8, 22*8, 21*8, 20*8, 19*8, 18*8, 17*8, 16*8,
			7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	32*8	/* every sprite takes 32 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,     0, 8 },
	{ 1, 0x0000, &spritelayout,   0, 8 },
	{ -1 } /* end of array */
};



static unsigned char amidar_color_prom[] =
{
	/* palette */
	0x00,0x07,0xC0,0xB6,0x00,0x38,0xC5,0x67,0x00,0x30,0x07,0x3F,0x00,0x07,0x30,0x3F,
	0x00,0x3F,0x30,0x07,0x00,0x38,0x67,0x3F,0x00,0xFF,0x07,0xDF,0x00,0xF8,0x07,0xFF
};



static unsigned char turtles_color_prom[] =
{
	/* palette */
	0x00,0xC0,0x57,0xFF,0x00,0x66,0xF2,0xFE,0x00,0x2D,0x12,0xBF,0x00,0x2F,0x7D,0xB8,
	0x00,0x72,0xD2,0x06,0x00,0x94,0xFF,0xE8,0x00,0x54,0x2F,0xF6,0x00,0x24,0xBF,0xC6
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz */
			0,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,	/* 2 Mhz?????? */
			2,	/* memory region #2 */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			amidar_sh_interrupt,1
		}
	},
	60,
	0,

	/* video hardware */
	32*8, 32*8, { 2*8, 30*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	32,32,
	amidar_vh_convert_color_prom,

	0,
	generic_vh_start,
	generic_vh_stop,
	amidar_vh_screenrefresh,

	/* sound hardware */
	0,
	0,
	amidar_sh_start,
	AY8910_sh_stop,
	AY8910_sh_update
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( amidar_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "amidarus.2c", 0x0000, 0x1000 )
	ROM_LOAD( "amidarus.2e", 0x1000, 0x1000 )
	ROM_LOAD( "amidarus.2f", 0x2000, 0x1000 )
	ROM_LOAD( "amidarus.2h", 0x3000, 0x1000 )
	ROM_LOAD( "amidarus.2j", 0x4000, 0x1000 )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "amidarus.5f", 0x0000, 0x0800 )
	ROM_LOAD( "amidarus.5h", 0x0800, 0x0800 )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "amidarus.5c", 0x0000, 0x1000 )
	ROM_LOAD( "amidarus.5d", 0x1000, 0x1000 )
ROM_END

ROM_START( amidarjp_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "amidar.2c", 0x0000, 0x1000 )
	ROM_LOAD( "amidar.2e", 0x1000, 0x1000 )
	ROM_LOAD( "amidar.2f", 0x2000, 0x1000 )
	ROM_LOAD( "amidar.2h", 0x3000, 0x1000 )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "amidar.5f", 0x0000, 0x0800 )
	ROM_LOAD( "amidar.5h", 0x0800, 0x0800 )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "amidar.5c", 0x0000, 0x1000 )
	ROM_LOAD( "amidar.5d", 0x1000, 0x1000 )
ROM_END

ROM_START( turtles_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "turt_vid.2c", 0x0000, 0x1000 )
	ROM_LOAD( "turt_vid.2e", 0x1000, 0x1000 )
	ROM_LOAD( "turt_vid.2f", 0x2000, 0x1000 )
	ROM_LOAD( "turt_vid.2h", 0x3000, 0x1000 )
	ROM_LOAD( "turt_vid.2j", 0x4000, 0x1000 )

	ROM_REGION(0x1000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "turt_vid.5f", 0x0000, 0x0800 )
	ROM_LOAD( "turt_vid.5h", 0x0800, 0x0800 )

	ROM_REGION(0x10000)	/* 64k for the audio CPU */
	ROM_LOAD( "turt_snd.5c", 0x0000, 0x1000 )
	ROM_LOAD( "turt_snd.5d", 0x1000, 0x1000 )
ROM_END



struct GameDriver amidar_driver =
{
	"amidar",
	&machine_driver,

	amidar_rom,
	0, 0,
	0,

	input_ports, amidar_dsw, keys,

	amidar_color_prom, 0, 0,
	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,	/* numbers */
		0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,	/* letters */
		0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a },
	0x06, 0x04,
	8*13, 8*16, 0x00,

	0, 0
};

struct GameDriver amidarjp_driver =
{
	"amidarjp",
	&machine_driver,

	amidarjp_rom,
	0, 0,
	0,

	input_ports, amidar_dsw, keys,

	amidar_color_prom, 0, 0,
	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,	/* numbers */
		0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,	/* letters */
		0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a },
	0x06, 0x04,
	8*13, 8*16, 0x00,

	0, 0
};

struct GameDriver turtles_driver =
{
	"turtles",
	&machine_driver,

	turtles_rom,
	0, 0,
	0,

	input_ports, turtles_dsw, keys,

	turtles_color_prom, 0, 0,
	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,	/* numbers */
		0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,	/* letters */
		0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a },
	0x06, 0x02,
	8*13, 8*16, 0x04,

	0, 0
};
