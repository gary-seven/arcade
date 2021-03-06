/*******************************************************************************
 **  galag: precise re-implementation of a popular space shoot-em-up
 **  gg1-5.s( gg1-5.3f)
 **
 **  sprite movement control
 *******************************************************************************/
/*
 ** header file includes
 */
#include "galag.h"

/*
 ** defines and typedefs
 */

/*
 ** extern declarations of variables defined in other files
 */

/*
 ** non-static external definitions this file or others
 */
#ifdef HELP_ME_DEBUG
uint16 dbg_step_cnt;
#endif


// use only even indices to keep consistent with z80 code:
// sprite-object states, and index to motion control pool
sprt_mctl_obj_t sprt_mctl_objs[0x40 * 2];
// rocket-hit notification to f_1DB3 from c_076A, 1-byte per object
uint8 sprt_hit_notif[0x30 * 2];

uint8 ds3_92A0_frame_cts[3];
uint8 cpu1_task_en[8];
uint8 b_bugs_flying_nbr;
uint8 bmbr_cont_flag; // b_92AA ... bomber continuous mode
r16_t bomb_hrates[0x08 * 2];

/*
 ** static external definitions in this file
 */
// variables
// this wastes some memory but it its the only way i can think of right
// now to preserve the 16-bit pointers that are embedded in the data
static uint8 flv_data[0x1000];

static uint8 mctl_actv_cnt;


// function prototypes
static uint8 hitd_fghtr_notif(uint8);
static void hitd_fghtr_hit(uint8, uint8, uint8);
static uint8 hitd_det_fghtr(uint8, uint8, uint8, uint8, uint8);
static void hitd_det_rckt(uint8, uint8, uint8);
static uint8 hitd_dspchr_rckt(uint8, uint8, uint8);
static uint8 hitd_dspchr_fghtr(uint8, uint8, uint8);
static uint8 hitd_dspchr(uint8, uint8, uint8);
static void rckt_man(uint8);
static void mctl_fltpn_dspchr(uint8);
static void mctl_hpos_ck(uint8);
static void mctl_rotn_incr(uint8);
static void mctl_coord_incr(uint8, uint8);
static void mctl_posn_set(uint8);
static uint16 mctl_rotn_hp(uint16, uint8);
static uint16 mctl_mul8(uint8, uint8);
static uint16 mctl_div_16_8(uint16, uint8);


/*
 if ld new address, hl+=2 ... loads new ptr
 not ld new address, hl+=3 ... skips 16-bit ptr and skips control byte
x=_flite_path_init
.=_next_superloop ($0B87,$0B8B,$0B8C)
/=_flite_path_init ($0BFF)
        $FF  case_0E49   make_object_inactive
/       $FE  case_0B16   HL+=9 ... alien breaks formation to attack ship (level 3+)
x       $FD  case_0B46   inc HL x2 ... alien returns to base from sortie
/       $FC  case_0B4E   inc HL x2 (not ptr) ... yellow alien loopback from dive, or boss left position and start dive
x       $FB  case_0AA0   HL+=1 ... element of convoy formation hits turning point and heads to home
/.      $FA  case_0BD1   inc HL x2, x3, yellow alien passed under bottom of screen and turns for home
.       $F9  case_0B5F   HL+=1 ... yellow alien passed under bottom of screen and turns for home
.       $F8  case_0B87   HL+=1 ... red alien transit to top
x       $F7  case_0B98   inc HL x2, x3, load 16-bit address ... attack convoy
.       $F6  case_0BA8   HL+=1 ... one red alien left in "free flight mode"
x       $F5  case_0942   HL+=1 ... ?
x       $F4  case_0A53   HL+=1 ... capture boss diving
/       $F3  case_0A01   HL+=9 ... diving alien left formation and fired
x       $F2  case_097B   inc HL x3 ... special 3 ship squadron (yellow alien split)
.       $F1  case_0968   HL+=1 ... diving attacks stop and aliens go home
.       $F0  case_0955   inc HL x2, x3: load 16-bit address ... attack convoy
.       $EF  case_094E   inc HL x2, x3: load 16-bit address ... one red alien left in "free flight mode"
*/

/*
 * I think this means word-to-bytes
 */
#define W2B( _arg_ )   (_arg_ & 0x00FF), (_arg_ >> 8)

/*
  flight-data, in groups of 3-byte sets:
   [0]: b0A (lo-nibble), b0B (hi-nibble) ... x and y displacements
   [1]: b0C ... rotation increment
   [2]: b0D ... duration of flight-step (frame-counts)
 */

static const uint8 flv_d_001d[] =
{
    0x23,0x06,0x16,0x23,0x00,0x19,0xF7,
    W2B(_flv_i_004b),
    0x23,0xF0,0x02,0xF0,
    W2B(_flv_i_005e),
    0x23,0xF0,0x24,0xFB,0x23,0x00,0xFF,0xFF,
};

// Create explicit array, this one is not contiguous with previous! Note the
// naming convention changed on this one - only referenced by init copy to RAM.
static const uint8 flv_p_004b[] =
{
    0x23,0xF0,0x26,0x23,0x14,0x13,0xFE,
    0x0D,0x0B,0x0A,0x08,0x06,0x04,0x03,0x01,0x23,0xFF,
    0xFF,0xFF,
//_flv_i_005e
    0x44,0xE4,0x18,0xFB,0x44,0x00,0xFF,0xFF,
    0xC9 // junk ?
};

static const uint8 flv_d_0067[] =
{
    0x23,0x08,0x08,0x23,0x03,0x1B,0x23,0x08,0x0F,0x23,0x16,0x15,0xF7,
    W2B(_flv_i_0084),
    0x23,0x16,0x03,0xF0,
    W2B(_flv_i_0097),
    0x23,0x16,0x19,0xFB,0x23,0x00,0xFF,0xFF,
//_flv_i_0084
    0x23,0x16,0x01,0xFE,
    0x0D,0x0C,0x0A,0x08,0x06,0x04,0x03,0x01,0x23,0xFC,
    0x30,0x23,0x00,0xFF,
    0xFF,
//_flv_i_0097
    0x44,0x27,0x0E,0xFB,0x44,0x00,0xFF,0xFF
};

static const uint8 flv_d_009f[] =
{
    0x33,0x06,0x18,0x23,0x00,0x18,0xF7,
    W2B(_flv_i_00b6),
    0x23,0xF0,0x08,0xF0,
    W2B(_flv_i_00cc),
    0x23,0xF0,0x20,0xFB,0x23,0x00,0xFF,0xFF,
// p_flv_00b6:
    0x23,0xF0,0x20,0x23,0x10,0x0D,0xFE,
    0x1A,0x18,0x15,0x10,0x0C,0x08,0x05,0x03,0x23,0xFE,
    0x30,0x23,0x00,0xFF,
    0xFF,
// p_flv_00cc:
    0x33,0xE0,0x10,0xFB,0x44,0x00,0xFF,0xFF
};

static const uint8 flv_d_00d4[] =
{
    0x23,0x03,0x18,0x33,0x04,0x10,0x23,0x08,0x0A,0x44,0x16,0x12,0xF7,
    W2B(_flv_i_0160),
    0x44,0x16,0x03,0xF0,
    W2B(_flv_i_0173),// stg 13
    0x44,0x16,0x1D,0xFB,0x23,0x00,0xFF,0xFF
};

// db_flv_00f1: this one or db_flv_0411 for boss launcher
static const uint8 flv_d_00f1[] =
{
    0x12,0x18,0x17,0x12,0x00,0x80,0xff
};


/*=============================================================================
;; Assignment of sprite-objects in formation:
;;
;;                  00 02 04 06 08 0A 0C 0E 10 12
;;
;;     14                    00 04 06 02          ; captured fighters (00, 02, 04 used as icons on push-start-btn screen)
;;     16                    30 34 36 32          ; bosses
;;     18              40 48 50 58 5A 52 4A 42    ; drones, escorts
;;     1A              44 4C 54 5C 5E 56 4E 46
;;     1C           08 10 18 20 28 2A 22 1A 12 0A
;;     1E           0C 14 1C 24 2C 2E 26 1E 16 0E
;;
;;     organization of row and column pixel position LUTs
;;
;;      |<-------------- COLUMNS --------------------->|<---------- ROWS ---------->|
;;
;;      00   02   04   06   08   0A   0C   0E   10   12   14   16   18   1A   1C   1E
;;
;;---------------------------------------------------------------------------*/
// The table assigns each "managed" sprite-object to it's formation position.
// IDs $38 - $3E are reserved for Special Bonus enemies .
// Each row and column ordinate, pre-multiplied by two, provides the
// byte-offset to lookup the actual pixel location.
const uint8 sprt_fmtn_hpos_ord_lut[] =
{
    0x14, 0x06, 0x14, 0x0C, 0x14, 0x08, 0x14, 0x0A, 0x1C, 0x00, 0x1C, 0x12, 0x1E, 0x00, 0x1E, 0x12,
    0x1C, 0x02, 0x1C, 0x10, 0x1E, 0x02, 0x1E, 0x10, 0x1C, 0x04, 0x1C, 0x0E, 0x1E, 0x04, 0x1E, 0x0E,
    0x1C, 0x06, 0x1C, 0x0C, 0x1E, 0x06, 0x1E, 0x0C, 0x1C, 0x08, 0x1C, 0x0A, 0x1E, 0x08, 0x1E, 0x0A,
    0x16, 0x06, 0x16, 0x0C, 0x16, 0x08, 0x16, 0x0A, 0x18, 0x00, 0x18, 0x12, 0x1A, 0x00, 0x1A, 0x12,
    0x18, 0x02, 0x18, 0x10, 0x1A, 0x02, 0x1A, 0x10, 0x18, 0x04, 0x18, 0x0E, 0x1A, 0x04, 0x1A, 0x0E,
    0x18, 0x06, 0x18, 0x0C, 0x1A, 0x06, 0x1A, 0x0C, 0x18, 0x08, 0x18, 0x0A, 0x1A, 0x08, 0x1A, 0x0A
};

// Create explicit array, this one is not contiguous with previous! Note the
// naming convention changed on this one - only referenced by init copy to RAM.
//static const uint8 flv_p_0160[] =

const uint8 flv_d_017b[] =
{
    0x23,0x06,0x18,0x23,0x00,0x18,0xF7,
    W2B( _flv_i_0192),
    0x44,0xF0,0x08,0xF0,
    W2B( _flv_i_01a8),
    0x44,0xF0,0x20,0xFB,0x23,0x00,0xFF,0xFF,
//p_flv_0192:
    0x44,0xF0,0x26,0x23,0x10,0x0B,0xFE,
    0x22,0x20,0x1E,0x1B,0x18,0x15,0x12,0x10,0x23,0xFE,
    0x30,0x23,0x00,0xFF,
    0xFF,
//p_flv_01a8:,
    0x66,0xE0,0x10,0xFB,0x44,0x00,0xFF,0xFF,
};

const uint8 flv_d_01b0[] =
{
    0x23,0x03,0x20,0x23,0x08,0x0F,0x23,0x16,0x12,0xF7,
    W2B(_flv_i_01ca),
    0x23,0x16,0x03,0xF0,
    W2B(_flv_i_01e0),
    0x23,0x16,0x1D,0xFB,0x23,0x00,0xFF,0xFF,
// p_flv_01ca:
    0x23,0x16,0x01,0xFE,
    0x0D,0x0C,0x0B,0x09,0x07,0x05,0x03,0x02,0x23,0x02,0x20,0x23,0xFC,
    0x12,0x23,0x00,0xFF,
    0xFF,
// p_flv_01e0:
    0x44,0x20,0x14,0xFB,0x44,0x00,0xFF,0xFF
};

static const uint8 flv_d_01E8[] =   // 6: challenge stage convoy
{
    0x23,0x00,0x10,0x23,0x01,0x40,0x22,0x0C,0x37,0x23,0x00,0xFF,0xFF
};

static const uint8 flv_d_01F5[] =   // 7: challenge stage convoy
{
    0x23,0x02,0x3A,0x23,0x10,0x09,0x23,0x00,0x18,0x23,0x20,0x10,
    0x23,0x00,0x18,0x23,0x20,0x0D,0x23,0x00,0xFF,0xFF
};

static const uint8 flv_d_020b[] =
{
    0x23,0x00,0x10,0x23,0x01,0x30,0x00,0x40,0x08,0x23,0xFF,0x30,0x23,0x00,0xFF,0xFF,
};

static const uint8 flv_d_021b[] =
{
    0x23,0x00,0x30,0x23,0x05,0x80,0x23,0x05,0x4C,0x23,0x04,0x01,0x23,0x00,0x50,0xFF,
};

static const uint8 flv_d_022b[] =
{
    0x23,0x00,0x28,0x23,0x06,0x1D,0x23,0x00,0x11,0x00,0x40,0x08,0x23,0x00,0x11,
    0x23,0xFA,0x1D,0x23,0x00,0x50,0xFF,
};

static const uint8 flv_d_0241[] =
{
    0x23,0x00,0x21,0x00,0x20,0x10,0x23,0xF8,0x20,0x23,0xFF,0x20,0x23,0xF8,0x1B,
    0x23,0xE8,0x0B,0x23,0x00,0x21,0x00,0x20,0x08,0x23,0x00,0x42,0xFF,
};

static const uint8 flv_d_025d[] =
{
    0x23,0x00,0x08,0x00,0x20,0x08,0x23,0xF0,0x20,0x23,0x10,0x20,0x23,0xF0,0x40,
    0x23,0x10,0x20,0x23,0xF0,0x20,0x00,0x20,0x08,0x23,0x00,0x30,0xFF,
};

static const uint8 flv_d_0279[] =
{
    0x23,0x10,0x0C,0x23,0x00,0x20,0x23,0xE8,0x10,
    0x23,0xF4,0x10,0x23,0xE8,0x10,0x23,0xF4,0x32,0x23,0xE8,0x10,0x23,0xF4,0x32,
    0x23,0xE8,0x10,0x23,0xF4,0x10,0x23,0xE8,0x0E,0x23,0x02,0x30,0xFF,
};

static const uint8 flv_d_029e[] =
{
    0x23,0xF1,0x08,0x23,0x00,0x10,0x23,0x05,0x3C,0x23,0x07,0x42,0x23,0x0A,0x40,
    0x23,0x10,0x2D,0x23,0x20,0x19,0x00,0xFC,0x14,0x23,0x02,0x4A,0xFF,
};

static const uint8 flv_d_02ba[] =
{
    0x23,0x04,0x20,0x23,0x00,0x16,0x23,0xF0,0x30,0x23,0x00,0x12,0x23,0x10,0x30,
    0x23,0x00,0x12,0x23,0x10,0x30,0x23,0x00,0x16,0x23,0x04,0x20,0x23,0x00,0x10,0xFF,
};

static const uint8 flv_d_02d9[] =
{
    0x23,0x00,0x15,0x00,0x20,0x08,0x23,0x00,0x11,
    0x00,0xE0,0x08,0x23,0x00,0x18,0x00,0x20,0x08,0x23,0x00,0x13,
    0x00,0xE0,0x08,0x23,0x00,0x1F,0x00,0x20,0x08,0x23,0x00,0x30,0xFF,
};

static const uint8 flv_d_02fb[] =
{
    0x23,0x02,0x0E,0x23,0x00,0x34,
    0x23,0x12,0x19,0x23,0x00,0x20,0x23,0xE0,0x0E,0x23,0x00,0x12,0x23,0x20,0x0E,
    0x23,0x00,0x0C,0x23,0xE0,0x0E,0x23,0x1B,0x08,0x23,0x00,0x10,0xFF,
};

static const uint8 flv_d_031d[] =
{
    0x23,0x00,0x0D,0x00,0xC0,0x04,0x23,0x00,0x21,0x00,0x40,0x06,0x23,0x00,0x51,
    0x00,0xC0,0x06,0x23,0x00,0x73,0xFF,
};

static const uint8 flv_d_0333[] =
{
    0x23,0x08,0x20,0x23,0x00,0x16,0x23,0xE0,0x0C,0x23,0x02,0x0B,
    0x23,0x11,0x0C,0x23,0x02,0x0B,0x23,0xE0,0x0C,0x23,0x00,0x16,0x23,0x08,0x20,0xFF,
};

static const uint8 flv_d_atk_yllw[] =
{
    0x12,0x18,0x1E,
//};
//static const uint8 _flv_i_0352[] = {
    0x12,0x00,0x34,0x12,0xFB,0x26,
//};
//static const uint8 _flv_i_0358[] = {
    0x12,0x00,0x02,0xFC,
    0x2E,0x12,0xFA,0x3C,0xFA,
    W2B(_flv_i_039e),
//};
//static const uint8 _flv_i_0363[] = {
    0x12,0xF8,0x10,0x12,0xFA,0x5C,0x12,0x00,0x23,
//};
//static const uint8 _flv_i_036c[] = {
    0xF8,0xF9,0xEF,
    W2B(_flv_i_037c),
    0xF6,0xAB,
    0x12,0x01,0x28,0x12,0x0A,0x18,0xFD,
    W2B(_flv_i_0352),
//};
//static const uint8 _flv_i_037c[] = {
    0xF6,0xB0,
    0x23,0x08,0x1E,0x23,0x00,0x19,0x23,0xF8,0x16,0x23,0x00,0x02,0xFC,
    0x30,0x23,0xF7,0x26,0xFA,
    W2B(_flv_i_039e),
    0x23,0xF0,0x0A,0x23,0xF5,0x31,0x23,0x00,0x10,0xFD,
    W2B(_flv_i_036c), // oops shot captured fighter
//};
//static const uint8 _flv_i_039e[] = {
    0x12,0xF8,0x10,0x12,0x00,0x40,
    0xFB,0x12,0x00,0xFF,0xFF
};

static const uint8 flv_d_atk_red[] =
{
    0x12,0x18,0x1D,
//};
//static const uint8 _flv_i_03ac[] = {
    0x12,0x00,0x28,0x12,0xFA,0x02,0xF3,
    0x3F,0x3B,0x36,0x32,0x28,0x26,0x24,0x22,
    0x12,0x04,0x30,0x12,0xFC,0x30,0x12,0x00,0x18,0xF8,0xF9,0xFA,
    W2B(_flv_i_040c),
    0xEF,
    W2B(_flv_i_03d7),
//};
//static const uint8 _flv_i_03cc[] = {
    0xF6,0xB0,
    0x12,0x01,0x28,0x12,0x0A,0x15,0xFD,
    W2B(_flv_i_03ac),
//};
//static const uint8 _flv_i_03d7[] = {
    0xF6,0xC0,
    0x23,0x08,0x10,0x23,0x00,0x23,0x23,0xF8,0x0F,0x23,0x00,0x48,0xF8,0xF9,0xFA,
    W2B(_flv_i_040c),
    0xF6,0xB0,
    0x23,0x08,0x20,0x23,0x00,0x08,0x23,0xF8,0x02,0xF3,
    0x34,0x31,0x2D,0x29,0x22,0x26,0x1F,0x18,
    0x23,0x08,0x18,0x23,0xF8,0x18,0x23,0x00,0x10,0xF8,0xF9,0xFD,
    W2B(_flv_i_03cc),
//};
//static const uint8 _flv_i_040c[] = {
    0xFB,0x12,0x00,0xFF,0xFF,
};
//flv_d_0411: this one or flv_d_00f1
static const uint8 flv_d_0411[] = {
    0x12,0x18,0x14,
//};
//static const uint8 _flv_i_0414[] = {
    0x12,0x03,0x2A,0x12,0x10,0x40,0x12,0x01,0x20,0x12,0xFE,0x71,
//};
//static const uint8 _flv_i_0420[] = {
    0xF9,0xF1,0xFA,
    W2B(_flv_i_040c),
//};
//static const uint8 _flv_i_0425[] = {
    0xEF,
    W2B(_flv_i_0430),
    0xF6,0xAB,
    0x12,0x02,0x20,0xFD,
    W2B(_flv_i_0414),
//};
//static const uint8 _flv_i_0430[] = {
    0xF6,0xB0,
    0x23,0x04,0x1A,0x23,0x03,0x1D,0x23,0x1A,0x25,0x23,0x03,0x10,0x23,0xFD,0x48,0xFD,
    W2B(_flv_i_0420),
};
// db_fltv_rogefgter:

static const uint8 flv_d_0454[] = { // capture mode boss
    0x12,0x18,0x14,0xf4,
    0x12,0x00,0x04,0xfc,
    0x48,0x00,0xfc,0xff,
    0x23,0x00,0x30,0xf8,0xf9,0xfa,
    W2B(_flv_i_040c),
    0xfd,
    W2B(_flv_i_0425)
};


/************************************/
uint8 flv_get_data(uint16 phl)
{
    if (phl >= sizeof(flv_data))
    {
        return 0; // oops, whatya want me to do about it
    }
    return flv_data[phl];
}
// this one is separate to allow breakpoint prior to selection of new command token
uint8 flv_get_data_uber(uint16 phl)
{
    return  flv_get_data(phl);
}

// return the next two bytes as a 16-bit address/offset with which to reload the data pointer
uint16 flv_0B46_set_ptr(uint16 u16hl)
{
    r16_t de;

    u16hl += 1;
    de.pair.b0 = flv_get_data(u16hl);
    u16hl += 1;
    de.pair.b1 = flv_get_data(u16hl);

    return de.word;
}

#define FLV_MCPY( _DTBLE_, _ADDR_ ) \
{ \
  int i = 0; \
  for ( i = 0; i < sizeof(_DTBLE_); i++) \
  { \
    flv_data[_ADDR_ + i] = _DTBLE_[i]; \
  } \
}


/*=============================================================================
;; cpu1_init()
;;  Description:
;;    irq_acknowledge_enable_cpu1 has no effect in C code
;; IN:
;;
;; OUT:
;;
;; PRESERVES:
;;
;;---------------------------------------------------------------------------*/
void cpu1_init(void)
{
    uint16 BC;

    // Following ROM test, CPU1 loops until CPU0 clears the flag,
    // then enables the Vblank interrupt, and the background task just loops after that.
    /*
    l_0596:
           ld   a,(de)                                ; wait for master to acknowledge/resume (0)
           and  a
           jr   nz,l_0596

           im   1

           xor  a
           ld   (b_89E0),a                            ; 0
    */
    // set task-enable defaults (ref. d_05B7)
    // cpu1_task_en[0]: this one initialized (to 7) in cpu0 following RAM test
    cpu1_task_en[1] = 0x01;
    cpu1_task_en[2] = 0x01; // 2
    cpu1_task_en[3] = 0x00; // 3
    cpu1_task_en[4] = 0x01; // 4
    cpu1_task_en[5] = 0x01; // 5
    cpu1_task_en[6] = 0x00; // 6
    cpu1_task_en[7] = 0x0A; // 7: don't see why this is not 1

/*
    for (BC = 0; BC < 7; BC++)
    {
        cpu1_task_en[1 + BC ] = db_cpu1_task_en_ini[BC];
    }
*/
    /*
               ld   a,#1
               ld   (0x6821),a                            ; cpu #1 irq acknowledge/enable
               ei
     */

    irq_acknowledge_enable_cpu1 = 1; // sfr_6821 ... enable cpu1_rst38 (cpu1 reset)


    // shouldn't be here
    flv_init_data();
}


/*=============================================================================
;; forward declarations for cpu1 tasks
;;-----------------------------------------------------------------------------*/
void f_05BE(void);
void f_05BF(void);
void f_08D3(void);
void f_05BE(void);
void f_06F5(void);
void f_05EE(void);
void f_05BE(void);
void f_0ECA(void);


/*=============================================================================
; Function pointers for periodic tasks on this CPU (ds_cpu1_task_enbl)
; The following bytes are copied from cpu1_task_en_ini to ds_cpu1_task_enbl[1]
;   0x01,0x01,0x00,0x01,0x01,0x00,0x0A
;;-----------------------------------------------------------------------------*/
void (* const d_cpu1_task_table[])(void) =
{
    f_05BE, // null-task (this is the only slot with a "null" task that is enabled.
    f_05BF, // [1]
    f_08D3, // [2]
    f_05BE, // null-task
    f_06F5, // [4]
    f_05EE, // [5] ... hit-detection: change to f_05BE for invincibility
    f_05BE, // null-task
    f_0ECA  // [7] ... ?
};

/*=============================================================================
;; jp_0513_rst38()
;;  Description:
;;    RST $0038 handler.
;;    The first part uses vsync signal to develop clock references.
;;      ds3_92A0_frame_cts[0]: 60Hz (base rate)
;;      ds3_92A0_frame_cts[1]:  2Hz (div 32)
;;      ds3_92A0_frame_cts[2]:  4Hz (div 16)
;;    The counters are not reset in the course of the game operation.
;;
;;    For efficiency, bit masking is used instead of division so the real base
;;    period is 64 which is close enough to 60Hz.
;;
;;    Note: frame_cts[2] is used as the baserate to 4 Game Timers in
;;    CPU0:f_1DD2 (rescaled to develop a 2Hz clock)
;; IN:
;;  ...
;; OUT:
;;  ...
;;-----------------------------------------------------------------------------*/
void cpu1_rst38(void)
{
    uint8 A, C;

    /*
           xor  a
           ld   (_sfr_6821),a                         ; 0 ...CPU-sub1 IRQ acknowledge/enable
           ld   a,(_sfr_dsw5)                         ; DSWA: freeze video
           and  #0x02                                 ; freeze_ctrl_dsw (6j)
           jp   z,l_0575                              ; if paused, goto 0575 // done
     */
    // the DI/EI pair doesn't actually do anything in C code
    irq_acknowledge_enable_cpu1 = 0; // sfr_6821 ... disable cpu1_rst38 (rst38)

    // frame_cntr++
    ds3_92A0_frame_cts[0]++;

    if ((ds3_92A0_frame_cts[0] & 0x1F) == 1) // if ( cnt % 20 == 1 )
    {
        // l_0536
        ds3_92A0_frame_cts[2]++; // update 4Hz only
    }
    else if ((ds3_92A0_frame_cts[0] & 0x1F) == 0)
    {
        // OR forces H to be ODD when A==0...
        // once the OR is done, H should henceforth be odd when A==0
        ds3_92A0_frame_cts[2] |= 1;

        // update both 4Hz and 2Hz
        ds3_92A0_frame_cts[1]++; // t[1] = L++
        // l_0536:
        ds3_92A0_frame_cts[2]++; // t[2] = H++
    }

    bmbr_cont_flag = (b_bugs_actv_nbr < ds_new_stage_parms[0x07] &&
                   0 != task_actv_tbl_0[0x15]); // fire btn inp


    // find the first ready task.. may run more than one.
    // In general, any non-zero value in cpu1_task_en[] enables that task.
    // The enable value is added to the offset (C) and may be other than 1, but
    // only the task at [0] is ever associated with an enable value >0 ... i.e.
    // the value 7 causes the scheduler to run only task[0] (empty task)
    // followed by task[7] (mystery task!) during a the short time following the
    // self-test while the checkerboard screen is shown.
    C = 0;
    while (C < 8)
    {
        A = cpu1_task_en[C];
        if (A)
        {
            d_cpu1_task_table[C]();
            C += A;
        }
        else
        {
            C += 1;
        }
    }

    irq_acknowledge_enable_cpu1 = 1; // sfr_6821 ... enable cpu1_rst38 (rst38)
}


/*=============================================================================
;; f_05BE()
;;  Description:
;;   null task
;; IN:
;;  ...
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
void f_05BE(void)
{
    return;
}

/*=============================================================================
;; f_05BF()
;;  Description:
;;   works in conjunction with f_0828 of main CPU to update sprite RAM
;; IN:
;;  ...
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
void f_05BF(void)
{

}

/*=============================================================================
;; f_05EE()
;;  Description:
;;    Detect and handle ship collision with aliens and bombs
;; IN:
;;  ...
;; OUT:
;;  ...
;;----------------------------------------------------------------------------*/
void f_05EE(void)
{
    uint8 tmpSx;
    uint8 hit_notif = 0;

    if ( 0 != task_actv_tbl_0[0x14]) // f_1F85 (input and ship movement)
    {

        // if two_ship ... ... jr   z,l_0613
        // {
        //   if (ds_sprite_posn + 0x60)
        //   {
        //     hitd_fghtr_hit(SPR_IDX_SHIP + 0, 1); // pass flag to inhibit stage restart


        // l_0613: else ... !two_ship
        if (0 == mrw_sprite.cclr[SPR_IDX_SHIP + 0].b0) return; // ret  z

        tmpSx = mrw_sprite.posn[SPR_IDX_SHIP + 0].b0; // stash fighter sX because it will be 0'd by hitd_fghtr_notif
        hit_notif = hitd_fghtr_notif(SPR_IDX_SHIP + 0); // ship_collisn_detectn_runner

        if (0 == hit_notif) return; // ret  z

        //if (!two_ship) jr   z,l_0639_not_two_


        // l_0639_not_two_:
        task_actv_tbl_0[0x14] = 0; // f_1F85 (input and fighter movement)
        task_actv_tbl_0[0x15] = 0; // f_1F04 (fire button input)
        cpu1_task_en[0x05] = 0;    // f_05EE (this task, fighter hit-detection) 
        //ld   (ds_99B9_star_ctrl + 0x00),a  ; 0 ... 1 when fighter on screen
        //ld   (ds_9200_glbls + 0x17),a  // 0 ... no_restart_stg

        hitd_fghtr_hit(tmpSx, SPR_IDX_SHIP + 0, 0); // not docked fighters, pass flag to allow stage restart
    }
}

/*=============================================================================
;; hitd_fghtr_hit()
;;  Description:
;;   handle a collision detected on fighter
;; IN:
;;   oldSx: previous value of fighterX (z80 read directly from sprite SFRs)
;;   HL == &sprite_posn_base[0x60]  ... ship2 position (if call hitd_fghtr_hit)
;;   HL == &sprite_posn_sfr[0x60] ... (if jp  l_064F)

;;   E == object index of fighter1 or fighter2
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
static void hitd_fghtr_hit(uint8 oldSx, uint8 fghtr_obj_offs, uint8 no_restart_stg)
{
    mrw_sprite.posn[fghtr_obj_offs].b0 = oldSx - 8; // x<7:0> (read directly from SFRs because j_07C2 has already 0'd it)
    mrw_sprite.posn[fghtr_obj_offs].b1 -= 8; // y<7:0>
    mrw_sprite.cclr[fghtr_obj_offs].b1 = 0x0B; // color
    mrw_sprite.cclr[fghtr_obj_offs].b0 = 0x20; // explosion tile

    sprt_mctl_objs[fghtr_obj_offs].state = ROGUE_FGHTR;
    sprt_mctl_objs[fghtr_obj_offs].mctl_idx = 0x0F; // explosion counter to obj-state dispatcher

    mrw_sprite.ctrl[fghtr_obj_offs].b0 = 0x08 | 0x04;
// ds_plyr_actv._b_2ship = 0

    // sound-fx count/enable registers "bang" sound (not in Attract Mode)
    b_9AA0[0x19] = glbls9200.game_state - 1; // dec  a

    if (0 == no_restart_stg) // stage must restart if not docked fighters
    {
        glbls9200.restart_stage = 1;
        // ret
    }
    // else ... ret  nz
}

/*=============================================================================
;; hitd_fghtr_notif()
;;  Description:
;;   hit notification for fighter
;; IN:
;;  L == sprite_posn_base[] ... offset (FIGHTER1 or FIGHTER2)
;; OUT:
;;  E == preserved offset passed as argument in L
;;---------------------------------------------------------------------------*/
static uint8 hitd_fghtr_notif(uint8 fghtr_obj_offs)
{
    r16_t tmp16;
    uint8 hit_notif1, hit_notif2; // use two separate variables to avoid the global ship_collsn_detectd_
    uint8 ixl, ixh, l, b;

    if (ROGUE_FGHTR == sprt_mctl_objs[fghtr_obj_offs].state)
    {
        return 0; // ret  z
    }

    ixl = mrw_sprite.posn[fghtr_obj_offs].b0; // x

    tmp16.pair.b0 = mrw_sprite.posn[fghtr_obj_offs].b1; // y<7:0>
    tmp16.pair.b1 = mrw_sprite.ctrl[fghtr_obj_offs].b1; // y<8>
    tmp16.word >>= 1; // y<8:1> in pair.b0

    ixh = tmp16.pair.b0;

    if (0 != task_actv_tbl_0[0x08]) // f_2916 ...supervises attack waves
    {
        // only transients can do collision in attack wave
        l = 0x38;
        b = 0x04;
        // jr   l_06AC_
    }
    else
    {
        // not attack wave, set parameters to check all
        l = 0x00;
        b = 0x30;
    }
    // l_06AC_
    hit_notif1 = hitd_det_fghtr(fghtr_obj_offs, ixl, ixh, l, b);

    l = 0x68; // bombs
    b = 0x08;

    hit_notif2 = hitd_det_fghtr(fghtr_obj_offs, ixl, ixh, l, b);

    return hit_notif1 | hit_notif2;
}

/*=============================================================================
;; hitd_det_fghtr
;;  Description:
;;   hit detection for fighter
;; IN:
;;  L==starting object/index of alien or bomb
;;  B==repeat count ($08 or $30)
;;  ixl == fighter x<7:0>
;;  ixh == fighter y<15:8> of fixed-point
;; OUT:
;;  b8_ship_collsn_detectd_status
;;---------------------------------------------------------------------------*/
static uint8 hitd_det_fghtr(uint8 fghtr_idx, uint8 fx, uint8 fy, uint8 start_offset, uint8 number)
{
    uint8 coffs = start_offset;
    uint8 b = number;
    uint8 hit_notif = 0;

    while(b > 0)
    {
        if (0x80 != sprt_hit_notif[coffs]
        && INACTIVE != sprt_mctl_objs[coffs].state)
        {
            if (0x04 != (sprt_mctl_objs[coffs].state & 0xFE))
            {
                if (0 != mrw_sprite.posn[coffs].b0)
                {
                    // test for dX +/- 7 using offset and overvflow technique
                    r16_t tmpA;
                    tmpA.word = mrw_sprite.posn[coffs].b0;
                    tmpA.pair.b0 = tmpA.pair.b0 - fx - 7;
                    tmpA.word += 13; // add  a,#13

                    if (tmpA.pair.b1 > 0)
                    {
                        tmpA.pair.b1 = mrw_sprite.ctrl[coffs].b1;
                        tmpA.pair.b0 = mrw_sprite.posn[coffs].b1;
                        tmpA.word >>= 1; // sprite.y<8:1> in b0

                        tmpA.pair.b0 = tmpA.pair.b0 - fy - 4;
                        tmpA.word += 7;

                        if (tmpA.pair.b1 > 0)
                        {
                            hit_notif = 1;
                            // we're hit!
                            // AF==1 if moving alien
                            // nz if fighter hit
                            hitd_dspchr_fghtr(1, fghtr_idx, coffs); // jp   j_07C2
                            return hit_notif ;
                        }
                    }
                }
            }
        }
        // l_06F0
        coffs += 2;
        b -= 1; // djnz while_06B7
    } // while

    return hit_notif;
}

/*=============================================================================
;; f_06F5()
;;  Description:
;;    rocket motion and hit-detection manager
;; IN:
;;  ...
;; OUT:
;;  ...
;;----------------------------------------------------------------------------*/
void f_06F5(void)
{
    rckt_man(0);
    rckt_man(1);
}

/*=============================================================================
;; rckt_man()
;;  Description:
;;    rocket motion and hit-detection manager
;; IN:
;;   DE == pointer to rocket "attribute", e.g. &b_92A0_4[0], &b_92A0_4[1]
;;         Value is E0 if the ship is oriented normally, not rotated.
;;         bit7=orientation, bit6=flipY, bit5=flipX, 1:2=displacement
;;
;; OUT:
;;  ...
;;----------------------------------------------------------------------------*/
static void rckt_man(uint8 de)
{
    uint8 AF, A;
    r16_t HL;

    // index for rocket 0 or 1,
    uint8 hl = SPR_IDX_RCKT + de * 2; // even indices

    if (0 != mrw_sprite.posn[hl].b0)
    {
        // if horizontal orientation, dY = A' ... adusted displacement in dY
        AF = b_92A4_rockt_attribute[de] & 0x07; // I thought it was only bits 1:2 ? ... bit7=orientation, bit6=flipY, bit5=flipX, 1:2=displacement

        A = 6; // maximum displacement in dX

        // if ( vertical orientation )
        if (b_92A4_rockt_attribute[de] & 0x80) // bit  7,b
        {
            //  ex   af,af' ... swap
            A = b_92A4_rockt_attribute[de] & 0x07; // adusted displacement in dX
            AF = 6; // maximum displacement in dY
        }

        // l_0713:
        // l_0719: add new sX increment
        if (b_92A4_rockt_attribute[de] & 0x40) // bit  6,b ... flipY
        {
            // non-flipped sprite is left facing ... negate X increment
            A = -A; // neg
        }
        mrw_sprite.posn[hl].b0 += A; // add  a,(hl)

        // one test for left/right limits ($F0) or < 0 ($FF)
        if (mrw_sprite.posn[hl].b0 < 240) // $F0
        {
            // ld   ixl,a ... rocket.sX passed to hitd_det_rckt

            // NOW onto sY

            HL.pair.b1 = mrw_sprite.ctrl[hl].b1 & 0x01; // rocket.sY<8>
            HL.pair.b0 = mrw_sprite.posn[hl].b1;

            if (0 != (b_92A4_rockt_attribute[de] & 0x20)) // bit  5,b ... flipX
            {
                // negate and add dY
                AF = -AF; // neg
            }
            // add dY ... adding into an int16 so remind C we're adding a signed int
            HL.word += (sint8)AF; // add  a,(hl)

            mrw_sprite.posn[hl].b1 = HL.pair.b0; // .sY<7:0>

            // explicit handling of posn.sy:8 i.e. sign, overflow/carry not needed in 16-bit math!
            mrw_sprite.ctrl[hl].b1 = (mrw_sprite.ctrl[hl].b1 & 0xFE) | (HL.pair.b1 & 0x01);

            // ld   ixh,a ... rocket.sy<8:1>, passed to hitd_det_rckt in IXH

            // z80 re-scales and drops bit-0, i.e. thresholds are $14 and $9C
            if (HL.word >= 40 && HL.word <= 312) // disable_rocket_wposn
            {
                // lower-byte of pointer to object/sprite in L is passed through to
                // j_07C2 (odd, i.e. offset to b1)
                //   ld   e,l
                if (0 != task_actv_tbl_0[0x1D]) // capturing boss destroyed, rescued ship spinning
                {
                    // ld   hl,#ds_sprite_posn + 0x08             ; skip first 4 objects...
                    // ld   b,#0x30 - 4
                    // jr   l_075C_call_hit_detection

                    hitd_det_rckt(hl, 0x08, 0x30 - 4);
                }
                else
                {
                    // jr   z,l_0757_call_hit_detection_all

                    // l_0757_call_hit_detection_all
                    // reset HL and count to check $30 objects
                    // hl,#ds_sprite_posn ... i.e. L == 0

                    // l_075C_call_hit_detection
                    // E=offset_to_rocket_sprite, hl=offset_to_object checked, b==count,
                    hitd_det_rckt(hl, 0x00, 0x30);
                }
                return;
            }
        }

        //l_0763_disable_rocket:
        mrw_sprite.posn[hl].b0 = 0; // x
        mrw_sprite.ctrl[hl].b0 = 0; // attribute bits

        // ret
    } // if (0 != mrw_sprite.posn[hl].b0)
}

/*=============================================================================
;; hitd_det_rckt()
;;  Description:
;;   rocket hit detection
;; IN:
;;  E == pointer/index to rocket object, passed through to
;;       hitd_dspchr_rckt (odd, i.e. offset to b1)
;;  HL == pointer to starting object in sprite.posn[] to test ... 0, or
;;        +8 skips 4 objects... see explanation at l_0757.
;;  B == count ... $30, or ($30 - 4) as per explanation above.
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
static void hitd_det_rckt(uint8 E, uint8 hl, uint8 B)
{
    uint8 IXL, IXH;
    r16_t tmp16;

    // rocket.sy<8:1> (scale factor 2 in order to get it in 8-bits)
    tmp16.pair.b0 = mrw_sprite.posn[E].b1;
    tmp16.pair.b1 = mrw_sprite.ctrl[E].b1;
    IXH = tmp16.word >> 1; // rocket.sy<8:1> passed in IXH
    IXL = mrw_sprite.posn[E].b0;

    // l_076A_while_object:
    while (B-- > 0)
    {
        uint8 AF;

        // if (obj_status[L].state<7>  ||  hit_notif<7> )

        if (INACTIVE != (sprt_mctl_objs[hl].state |
                         sprt_hit_notif[hl])) // else  jr   c,l_07B4_next_object
        {
            // check if object status 04 (already exploding) or 05 (bonus bitmap)

            // cp   #4 ... else ... jr   z,l_07B4_next_object
            if (EXPLODING != sprt_mctl_objs[hl].state &&
                    SCORE_BITM != sprt_mctl_objs[hl].state)
            {
                r16_t tmpA;

                // test dX and dY for within +/- 6 pixels, using the addition
                // offset with "Cy" so only 1 test needed for ( d>-6 && d<+6 )

                // check Y coordinate ... set .sY<8:1>
                tmpA.pair.b1 = mrw_sprite.ctrl[hl].b1; // .sy<8>
                tmpA.pair.b0 = mrw_sprite.posn[hl].b1; // .sy<7:0>
                // tmpA.pair.b1 should now be 0 to properly detect carry-out
                // into <:8> from addition (below)
                tmpA.word >>= 1; // rrc  d ... rra

                // tolerance (3) and offset (6) for hit-check divided by 2 to
                // account for scaling
                // only 1-byte of result needed ( d>-3 && d<+3 )
                tmpA.pair.b0 -= IXH; // sub  ixh ...  -= rocket.sy<8:1>
                tmpA.pair.b0 -= 3;

                // tmpA.pair.b1 should be 0 (from right-shift, above) to
                // properly detect carry-out into <:8> from addition
                tmpA.word += 6; // carry out from 1-byte sets "Cy" in .b1<0>

                if (tmpA.pair.b1) // ... else ... jr   nc,l_07B4_next_object
                {
                    // check X coordinate

                    // ld   a,c ... dec  a ... and  #0xFE ... ex   af,af
                    AF = (sprt_mctl_objs[hl].state - 1) & 0xFE; // object status to j_07C2

                    if (1 /* ! ds_plyr_actv._b_2ship */) // ... else ... jr   nz,l_07A4
                    {
                        // only 1-byte of result needed ( d>-6 && d<+6 )
                        tmpA.word = mrw_sprite.posn[hl].b0;
                        tmpA.pair.b0 -= IXL; // sub  ixl ...  -= rocket.sy<8:1>
                        tmpA.pair.b0 -= 6; // sub  6
                        tmpA.pair.b1 = 0; // clear it so we can test for "Cy"
                        tmpA.word += 11; // carry out from 1-byte sets "Cy" in .b1<0>

                        if (tmpA.pair.b1) // jr   c,l_07B9_
                        {
                            // l_07B9_

                            if ( 1 != hitd_dspchr_rckt(AF, E, hl) )
                            {
                                return;
                            }
                            // else ... jr   l_07B4_next_object
                        }
                        // else ... jr   l_07B4_next_object
                    }
                    else //  l_07A4 ... twin fighter
                    {
                        tmpA.word = mrw_sprite.posn[hl].b0 - IXL; // sub  ixl ... sprite.sx -= rocket.sx
                        tmpA.word -= 20;
                        tmpA.pair.b1 = 0; // clear it so we can test for "Cy"
                        tmpA.word += 11;

                        if (!tmpA.pair.b1) // jr   c,l_07B9_pre_hdl_collsn
                        {
                            tmpA.pair.b1 = 0;
                            tmpA.word += 4;
                            if (!tmpA.pair.b1) // jr   c,l_07B4_next_object
                            {
                                tmpA.pair.b1 = 0;
                                tmpA.word += 11;

                                if (!tmpA.pair.b1) // jr   c,l_07B9_pre_hdl_collsn
                                {
                                    // l_07B4_next_object
                                    //hl += 2;
                                    break;
                                }
                                // else ... jr   c,l_07B9_
                            }
                            else
                            {
                                // jr   c,l_07B4_next_object

                                // l_07B4_next_object:

                                //       inc  l
                                //       inc  l
                                //       djnz l_076A_begin_object_check
                                hl += 2;
                                break;
                            }
                        }

                        if ( 1 != hitd_dspchr_rckt(AF, E, hl) )
                        {
                            return;
                        }

                        return; // j_07C2
                    }
                    // nothing else can go here!
                }
            } // if (0x04) ... else ... jr   z,l_07B4_next_object
        }

        // l_07B4_next_object:

        //       inc  l
        //       inc  l
        //       djnz l_076A_begin_object_check

        hl += 2;

    } // while (B-- > 0)

    //  j_07C2
}

/*=============================================================================
;;  Description:
;;   Detect collisions from the reference of the rocket ... update hit count
;;   and call common subroutine.
;; IN:
;;   AF == (sprt_mctl_objs[hl].state  - 1) & 0xFE
;;   L == offset/index of destroyed enemy/bomb
;;   E == offset/index of sprite[rocket.n]
;; OUT:
;;  ...
;; RETURN:
;;   1 on jp   l_07B4_next_object
;;   0
;;---------------------------------------------------------------------------*/
static uint8 hitd_dspchr_rckt(uint8 AF, uint8 E, uint8 HL)
{
    //l_07B9_
    plyr_actv.hit_ct += 1;

    // make the rocket available to rckt_sprite_init() again:
    //  z80 code "harmlessly" allowed the passed pointer to .b0 for fighter,
    //  but .b1 for rocket, so the correct variables are set here.
    mrw_sprite.posn[E].b1 = 0; // ld   (de),a ... sX<7:0>
    mrw_sprite.ctrl[E].b1 = 0; // ld   (de),a

    return hitd_dspchr(AF, E, HL);
}

/*=============================================================================
;;  Description:
;;   Detect collisions from the reference of the fighter ... update hit count
;;   and call common subroutine.
;; IN:
;;   AF == (sprt_mctl_objs[hl].state  - 1) & 0xFE
;;   L == offset/index of destroyed enemy/bomb
;;   E == offset/index of sprite[rocket.n]
;; OUT:
;;  ...
;; RETURN:
;;   1 on jp   l_07B4_next_object
;;   0
;;---------------------------------------------------------------------------*/
static uint8 hitd_dspchr_fghtr(uint8 AF, uint8 E, uint8 HL)
{
    // z80 code "harmlessly" allowed the passed pointer to .b0 for fighter, but
    // .b1 for rocket, so the correct variables are set here.
    //mrw_sprite.posn[E].b0 = 0; // ld   (de),a ... sX<7:0>
    //mrw_sprite.ctrl[E].b0 = 0; // ld   (de),a

    return hitd_dspchr(AF, E, HL);
}

/*=============================================================================
;; hitd_dspchr()
;;  Description:
;;   collisions are detected from the reference of the rocket or fighter - this
;;   function is common to both rocket and fighter hit detection, and
;;   dispatches the target appropriately.
;; IN:
;;   L == offset/index of destroyed enemy/bomb
;;   E == offset/index of rocket[n].b1 ... sprite.posn[RCKTn].y must
;;        be set to zero as required for correct handling in rckt_sprite_init
;;   E == offset/index of fighter[n].b0 ... sprite.ctrl[FGHTRn].b0 is set to 0 ... does it matter?
;;   A' == object status
;; OUT:
;;  ...
;; RETURN:
;;   1 on jp   l_07B4_next_object
;;   0
;;---------------------------------------------------------------------------*/
static uint8 hitd_dspchr(uint8 AF, uint8 E, uint8 HL)
{
    uint8 A;

    // for fighter, setting .y to 0 will screw up setting .y of explosion tile in hitd_fghtr_hit
    //mrw_sprite.posn[E].b1 = 0; // ld   (de),a ... sX<7:0>
    //mrw_sprite.ctrl[E].b1 = 0; // ld   (de),a

    if (0 == mrw_sprite.cclr[HL].b1) // and  a
    {
        // l_08CA_: color map 0 is "green" boss, don't delete from queue yet
        mrw_sprite.cclr[HL].b1 += 1; // color blue

        // sound-fx count/enable registers
        b_9AA0[0x04] = mrw_sprite.cclr[HL].b1; // hit_green_boss

        // jp   l_07B4_next_object
        return 1;
    }

    if (0x0B == mrw_sprite.cclr[HL].b1) // cp   #0x0B ... color map $B is for "bombs"
    {
        // noticed this stuff will also be cleared out by gctl_plyr_terminate
        // l_0815_ ... bomb colliding with fighter
        mrw_sprite.posn[HL].b0 = 0;
        sprt_mctl_objs[HL].state = INACTIVE;

        return 0; // not jp   l_07B4_next_object
    }

    // rocket or ship collided with bug
    // ex   af,af'
    // jr   nz,l_081E
    if (0 == AF)
    {
        // rocket hit stationary bug
        // ex   af,af'

        // l_07DB: set it up for elimination
        sprt_hit_notif[HL] = 0x81;

        // l_07DF: if capture boss
    }
    else
    {
        // l_081E: handle collision with moving alien
        A = sprt_mctl_objs[ HL ].mctl_idx;
        mctl_mpool[A].b13 = 0;

        b_bug_flyng_hits_p_round += 1;

        // non-challenge stage, ctr intialized to 0 at start of round.
        // if 0 == w_bug_flying_hit_cnt ... jr   nz,l_0849
        w_bug_flying_hit_cnt -= 1;

        if ( 0 == w_bug_flying_hit_cnt )
        {
            // splashed all elements of challenge stage convoy
            sprt_hit_notif[HL] = stg_chllg_rnd_attrib[1];
            ds_bug_collsn[0x0F] += stg_chllg_rnd_attrib[0];
            // jr   l_07DF

            // l_07DF:
        }
        else
        {
            // l_0849: handle special cases of moving objects

            // if (color map == captured fighter)  $07

            // jr   l_08B0

            // l_0852:

            // color map 1 ... blue boss hit once
            // else
            if ( 0x01 != mrw_sprite.cclr[HL].b1 ) // color map 1 ... blue boss hit once
            {
                // jp   nz,l_07DB

                // l_07DB: set it up for elimination
                sprt_hit_notif[HL] = 0x81;

                // l_07DF:
            }
            else // blue boss (moving)
            {
                // check for captured-fighter

                // jr   nz,l_0899

                //l_0899:
                // lone blue boss killed, or boss killed before pulling the beam all in
                ds4_game_tmrs[0x01] = 6;

                // A = ds_plyr_actv[_ds_array8 + ( hl & 0x07 }]

                // D = ds_plyr_actv[_ds_array8 + ( hl & 0x07 + 1 }]

                // l_08AA:
                ds_bug_collsn[ 0x0F ] += A; // add  a,(hl) etc.

                // jp here if shot the flying captured ship
                // l_08B0:
                sprt_hit_notif[HL] = 0xB5;

                // jp   l_07DF
            }
        }
    }

    // l_07DF:
    // if capture boss ...
    //   ld   a,(ds_plyr_actv +_b_cboss_obj)
    //   sub  l
    //   jr   nz,l_07EC
    // ... then ...
    //   ld   (ds_plyr_actv +_b_cboss_dive_start),a ; 0  ... shot the boss that was starting the capture beam
    //   inc  a
    //   ld   (ds_plyr_actv +_b_cboss_obj),a        ; 1  ... invalidate the capture boss object key

    //l_07EC: use the sprite color to get index to sound
    A = mrw_sprite.cclr[HL].b1 - 1; // ld   a,c ... dec  a

    if (0x07 != mrw_sprite.cclr[HL].b1) // jr   nz,l_07F5
    {
        // l_07F5:
        A &= 0x03; // and  #0x03
    }

    // l_07F8:
    b_9AA0[0x01 + A] = 1; // sound_fx_status ... add dec'd A back to index

    A = mrw_sprite.cclr[HL].b1; // ld   a,c

    if (0x07 == mrw_sprite.cclr[HL].b1) // jr   nz,l_0808
    {
        //   ld   hl,#ds_plyr_actv +_b_cboss_dive_start ; 0
        //   ld   (hl),#0
    }

    // l_0808:
    // ld   hl,#ds_bug_collsn + 0x00              ; missile/bug or ship/bug collision
    // rst  0x10                                  ; HL += A
    // inc  (hl)
    ds_bug_collsn[A] += 1; //  missile/bug or ship/bug collision

    // ex   af,af'
    // jr   z,l_0811
    if ( 0 != AF ) // un-stash parameter
    {
        // inc  (hl)
        ds_bug_collsn[A] += 1; //  missile/bug or ship/bug collision
    }

    // l_0811:
    //   pop  hl
    //   jp   l_07B4_next_object
    return 1;
}

/*=============================================================================
;; f_08D3()
;;  Description:
;;   Periodic task to supervise object motion.
;; IN:
;;  ...
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
void f_08D3(void)
{
    uint8 a; // index of element in motion-control pool

    // wait till next frame to report count from previous frame for some reason
    b_bugs_flying_nbr = mctl_actv_cnt;
    mctl_actv_cnt = 0;

    // l_08E4_ ... traverse the pool array
    for (a = 0; a < 12; a++) // a,#0x0C
    {
        // check for activated state
        if (0 != (mctl_mpool[a].b13 & 0x01)) // bit  0,0x13(ix)
        {
            mctl_actv_cnt += 1; // inc  (hl)

            if ((sprt_mctl_objs[ mctl_mpool[a].b10 ].state == PTRN_CTL ||
                    sprt_mctl_objs[ mctl_mpool[a].b10 ].state == SPAWNING ||
                    sprt_mctl_objs[ mctl_mpool[a].b10 ].state == HOMING))
            {
                // jr   z,mctl_fltpn_dspchr
                mctl_fltpn_dspchr(a);
            }
            else // jp   nz,case_0E49_make_object_inactive
            {
                // shot a non-flying capture boss

                // jp   next__pool_idx
            }
        }
        // next__pool_idx
    } // ret  z
}

/*=============================================================================
;; mctl_fltpn_dspchr()
;;  Description:
;;   flight plan state control, load and dispatch state-token
;; IN:
;;  ...
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
static void mctl_fltpn_dspchr(uint8 mpidx)
{
    mctl_mpool[mpidx].b0D -= 1; // dec  0x0D(ix)

    // check for expiration of this token
    // if token not expired, go directly to flite path handler
    if (0 == mctl_mpool[mpidx].b0D) // jp   nz,l_0C05_
    {
        uint16 pHLdata;
        uint8 mctld; // control or data token
        uint8 get_token; // avoids a goto for exitting do/while (jp 0x0BFF)

        // flight-path vector has expired... setup HL as pointer to next data token
        pHLdata = mctl_mpool[mpidx].p08.word;

        // j_090E_flite_path_init ...
        // do-block allows reading data token after state-selection
        do
        {
            r16_t pushDE, rBC, rHL, rDE;

            // get next token and check if ordinary data or state-selection
            mctld = flv_get_data_uber(pHLdata); // ld   a,(hl)

            get_token = 0;

            // cp   #0xEF
            if (mctld >= 0xEF) // jp   c,l_0BDC_flite_pth_load
            {
                // the flag forces repetition of the do-block so that the
                // next token will be retrieved before continuing to flight-path handler
                get_token = 1;
                // enclose remaining if/else within if > $EF to optimize and allow ctrl-shift-B over entire block
            }

            // cpl'd token indexes into jp-tbl for selection of next state,
            // but there is no benefit to the cpl here
            //
            switch (mctld) // d_0920_jp_tbl
            {

            case 0xFF: // _0E49 (00): inactive
            {
                // training mode, make previous diving boss disabled
                sprt_mctl_objs[ mctl_mpool[mpidx].b10 ].state = INACTIVE; // ld   (hl),#0x80
                mrw_sprite.posn[ mctl_mpool[mpidx].b10 ].b0 = 0; // ld   (hl),#0
                mctl_mpool[mpidx].b13 = 0; // ld   0x13(ix),#0x00

                return; // only control token that exits the do

                break; //jp   next__pool_idx
            }

            case 0xFE: // _0B16 (01): attack elements that break formation to attack ship (level 3+)
            {
                get_token = 0; // get out of do/while
                break; // jp   l_0BFF_flite_pth_skip_load
            }

            // returning to base: red or boss from top of screen, yellow
            // from bottom of loop-around.
            case 0xFD: // _0B46 (02):
            {
                pHLdata = flv_0B46_set_ptr(pHLdata);
                break; // jp   j_090E_flite_path_init
            }

            // yellow dive and starting loopback, or boss left position
            // and starting dive down
            case 0xFC: // _0B4E (03):
            {
                pHLdata += 1; // inc  hl
                mctl_mpool[mpidx].b06 = flv_get_data(pHLdata);
                pHLdata += 1; // inc  hl
                mctl_mpool[mpidx].b07 = 0; // X
                mctl_mpool[mpidx].b13 |= 0x20; // set  5,0x13(ix)

                get_token = 0; // get out of do/while
                break; // jp   l_0BFF_flite_pth_skip_load
            }

            case 0xFB: // _0AA0 (04): attack wave turn and head to home
            {
                uint8 B, C, D, E, L;

                L = mctl_mpool[mpidx].b10;

                // already 9 if executing attack sortie
                sprt_mctl_objs[ L ].state = HOMING;

                C = sprt_fmtn_hpos_ord_lut[ L + 0 ]; // row index
                L = sprt_fmtn_hpos_ord_lut[ L + 1 ]; // column index

                B = fmtn_hpos.offs[L]; // x offset
                E = fmtn_hpos_orig[L / 2]; // x-coord (z80 must read from RAM copy)

                L = C;
                C = fmtn_hpos.offs[L]; // y offset
                D = fmtn_hpos_orig[L / 2]; // y coord (z80 must read from RAM copy)

                pushDE.pair.b0 = E >> 1; // srl ... origin position x (set bits 15:8)
                pushDE.pair.b1 = D; // origin position y (already bits 15:8)

                mctl_mpool[mpidx].b11 = B; // step x coord, x offset
                mctl_mpool[mpidx].b12 = C; // step y coord, y offset

                if (glbls9200.flip_screen)
                {
                    // flipped ... negate the steps
                    B = -B;
                    C = -C;
                }

                // l_0ACD:
                // adjust x/y for offset of home-positions - think
                // of screen-pixels being in quadrant IV so x and y
                // y adjustments are opposite in sign (subtract x)

                // add y-offset to .b00/.b01 (9.7 fixed-point scaling)
                rDE.word = C << 7; // ld   d,c
                mctl_mpool[mpidx].cy.word += rDE.word;

                // sub x-offset from .b02/.b03 (9.7 fixed-point scaling)
                rBC.word = B << 8;
                rBC.word >>= 1; // sra  b ... rr  c
                rBC.pair.b1 |= (B & 0x80); // gets the sign extension of sra b
                mctl_mpool[mpidx].cx.word -= rBC.word; // sbc  hl,bc

                // update rotation angle for updated adjusted position
                rHL.word = mctl_rotn_hp(pushDE.word, mpidx); // preserves DE & BC

                mctl_mpool[mpidx].ang.word = rHL.word >> 1;

                mctl_mpool[mpidx].b06 = pushDE.pair.b1; // origin home position y (bits 15:8)
                mctl_mpool[mpidx].b07 = pushDE.pair.b0; // origin home position x (bits 15:8)

                // if set, flite path handler checks for home
                mctl_mpool[mpidx].b13 |= 0x40; // set  6,0x13(ix)

                pHLdata += 1; // inc  hl

                break; // jp   j_090E_flite_path_init
            }

            // homing, red transit to top, yellow from offscreen at bottom
            // or skip if in continuous bombing mode
            case 0xFA: // _0BD1 (05):
            {
                // ld   a,(b_92A0 + 0x0A) ; flag set when continuous bombing
                // ld   c,a
                // ld   a,(ds_cpu0_task_actv + 0x1D)          ; f_2000 (destroyed boss that captured ship)
                // dec  a
                // and  c

                // jr   l_0B9F

                // l_0B9F:
                if (0 == bmbr_cont_flag || 1 == task_actv_tbl_0[0x1D]) // jp   z,l_0B46
                {
                    // l_0B46:
                    pHLdata = flv_0B46_set_ptr(pHLdata);
                }
                else
                {
                    pHLdata += 3; // inc  hl (x3)
                }
                break; // jp   j_090E_flite_path_init
            }

            // red alien flew through bottom of screen to top, heading for home
            // yellow alien flew under bottom of screen and now turns for home
            case 0xF9: // _0B5F (06):
            {
                uint8 E, A;

                E = mctl_mpool[mpidx].b10;
                E = sprt_fmtn_hpos_ord_lut[ E + 1 ]; // column index

                if (0 == glbls9200.flip_screen)
                {
                    A = fmtn_hpos.spcoords[ E ].pair.b0; // relative offset
                }
                else
                {
                    A = 0xF0 - fmtn_hpos.spcoords[ E ].pair.b0 + 0x01;
                }

                //l_0B76
                mctl_mpool[mpidx].cx.pair.b1 =  A >> 1; // srl  a

                if (0 != bmbr_cont_flag) // jp   z,l_0B8B
                {
                    b_9AA0[0x13] = 1; // non-zero value, dive attack sound
                }

                //l_0B8B
                pHLdata += 1; // inc  hl

                // l_0B8C:
                mctl_mpool[mpidx].p08.word = pHLdata;
                mctl_mpool[mpidx].b0D += 1; // inc  0x0D(ix)

                return; // jp   next__pool_idx
                break;
            }

            // red alien flew through bottom of screen to top, heading for home
            // yellow alien flew under bottom of screen and now turns for home
            case 0xF8: // _0B87 (07):
            {
                mctl_mpool[mpidx].cy.pair.b1 = 0x0138 >> 1; // ld   0x01(ix),#$9C

                //l_0B8B
                pHLdata += 1; // inc  hl

                // l_0B8C:
                mctl_mpool[mpidx].p08.word = pHLdata;
                mctl_mpool[mpidx].b0D += 1; // inc  0x0D(ix)

                return; // jp   next__pool_idx
                break;
            }

            case 0xF7: // _0B98 (08): in an attack convoy ... changing direction
            {
                // "transient"? ($38, $3A, $3C, $3E)

                // ... coming from case_0BD1

                if (0x38 != (0x38 & mctl_mpool[mpidx].b10))
                {
                    pHLdata += 3; // 2 incs to skip address in table
                }
                else // jp   z,l_0B46 ... jp if this is a "transient" ($38, $3A, $3C, $3E)
                {
                    //l_0B46:
                    //       inc  hl
                    //       ld   e,(hl)
                    //       inc  hl
                    //       ld   d,(hl)
                    //       ex   de,hl
                    pHLdata = flv_0B46_set_ptr(pHLdata);
                }
                break; // jp   j_090E_flite_path_init
            }

            case 0xF6: // _0BA8 (09): continuous bombing mode
            {
                r16_t AC;

                pHLdata += 1; // inc  hl
                AC.word = flv_get_data(pHLdata); // ld   a,(hl)

                // reverses path index for opposite rotation
                if ( 0 != (mctl_mpool[mpidx].b13 & 0x80)) // bit  7,0x13(ix)
                {
                    AC.pair.b0 += 0x80; // add  a,#0x80
                    AC.pair.b0 = -AC.pair.b0 ; // neg
                }
                // l_0BB4:
                AC.word <<= 2;

                mctl_mpool[mpidx].ang.word = AC.word;
                mctl_mpool[mpidx].b0E = 0x1E; // bomb drop counter
                mctl_mpool[mpidx].b0F = b_92C0_0[0x08]; // bomb drop enable flags

                // jp   l_0B8B

                //l_0B8B
                pHLdata += 1; // inc  hl

                // l_0B8C:
                mctl_mpool[mpidx].p08.word = pHLdata;
                mctl_mpool[mpidx].b0D += 1; // inc  0x0D(ix)

                return; // jp   next__pool_idx
                break;
            }

            case 0xF5: // _0942 (0A): ?
            {
                break; //jp   j_090E_flite_path_init
            }

            case 0xF4: // _0A53 (0B): capture boss diving
            {
                break; //jp   j_090E_flite_path_init
            }

            // Red alien element has left formation - use deltaX to fighter
            // to select flight plan. Occurs when approximately mid-screen,
            // after initial jump from formation.
            case 0xF3: // _0A01 (0C)
            {
                r16_t tmpA;
                uint8 A;
                // setup horizontal limits for targetting - red alien is
                // somewhere around center-screen at the waypoint and can
                // target horizontally $60 pixels left or right (to the limits)
                if (mrw_sprite.posn[SPR_IDX_SHIP].b0 < 0x1E) // cp   #0x1E
                {
                    tmpA.word = 0x1E;
                }
                else if (mrw_sprite.posn[SPR_IDX_SHIP].b0 >= 0xD1) // cp   #0xD1
                {
                    tmpA.word = 0xD1;
                }
                else
                {
                    tmpA.word = mrw_sprite.posn[SPR_IDX_SHIP].b0;
                }

                // l_0A16:
                if ( 0 != glbls9200.flip_screen ) // bit  0,c
                {
                    tmpA.word = 0xF0 - tmpA.word + 0x01;
                }

                // l_0A1E: fighterX - alienX
                // first divide by 2 provides 9.7 scaling for sub
                tmpA.word >>= 1; // srl  a
                tmpA.word -= mctl_mpool[mpidx].cx.pair.b1; // sprite.x<7:1>

                // divide by 2 again ... b8 of 16-bit difference-result,
                // shifted into b7, will capture Cy from sub in previous step.
                tmpA.word >>= 1; // rra  a ... Cy into <7>

                // reverses path index for opposite rotation
                if ( 0 != (mctl_mpool[mpidx].b13 & 0x80)) // bit  7,0x13(ix)
                {
                    // negative (clockwise) rotation ... approach
                    // to waypoint is from right to left
                    tmpA.pair.b0 = -tmpA.pair.b0; // neg
                }

                // l_0A2C_:
                // offset to make positive index (working range -$18 to +$17)
                // provides indices to 8 available paths ranging from fighter
                // max-left (negative delta) to fighter max-right (positive delta)
                A = tmpA.word + (0x30 / 2); // 0x18;

                // test if offseted result still out of range negative
                if ((sint8)A < 0) // jp   p,l_0A32
                {
                    A = 0; // xor  a ... S is not set (overflow adding to negative sum)
                }

                //l_0A32:
                // enforce upper limit on offseted result
                if (A >= 0x30) A = 0x2F; // cp   #0x30 ... limit to 47 ... divide by 6 ... choose from 8

                //l_0A38:
                // divide pixels-per-step into scaled/offseted/bounded delta
                A = mctl_div_16_8(A, 6); // HL = HL / A

                A = flv_get_data(pHLdata + A + 1); // adjust for index range 1 thru 8
                mctl_mpool[mpidx].b0D = A; // expiration of this data-set
                pHLdata = pHLdata + 1 + 8;

                get_token = 0; // get out of do/while
                break; // jp   l_0BFF_flite_pth_skip_load
            }

            case 0xF2: // _097B (0D): special clone attacker
            {
                break; // jp   j_090E_flite_path_init
            }

            // boss fly through bottom of screen to top
            case 0xF1: // _0968 (0E):
            {
                uint8 E, A;

                E = mctl_mpool[mpidx].b10;

                E = sprt_fmtn_hpos_ord_lut[ E + 0 ]; // row index
                A = fmtn_hpos_orig[E / 2]; // x-coord (z80 must read from RAM copy)
                mctl_mpool[mpidx].cy.pair.b1 = A + 0x20;

                // jp   l_0B8B
                //l_0B8B
                pHLdata += 1; // inc  hl

                // l_0B8C:
                mctl_mpool[mpidx].p08.word = pHLdata;
                mctl_mpool[mpidx].b0D += 1; // inc  0x0D(ix)

                return; // jp   next__pool_idx
                break;
            }

            case 0xF0: // _0955
            case 0xEF: // _094E (10): continuous bombing mode
            {
                uint8 A; // this can be local with two cases mashed together

                A = ds_new_stage_parms[0x09]; // jumps the pointer on/after stage 8

                if (0xEF != mctld) // jp   l_0959
                {
                    // case_0955 (0F): attack wave:
                    A = ds_new_stage_parms[0x08]; // jumps the pointer on/after stage 8
                    A = 0; // this can be 0 for now
                }

                // l_0959:
                if (0 != A) // and  a
                {
                    // not until stage 8
                    // load a pointer from data tbl into .p08 (09)
                    pHLdata = flv_0B46_set_ptr(pHLdata); // reuse subroutine
                    // jp   l_0B8C
                }
                else // jr   z,l_0963
                {
                    // l_0963: skip loading new address
                    pHLdata += 2;
                    // jp   l_0B8B

                    //l_0B8B:
                    pHLdata += 1; // inc  hl
                }
                // l_0B8C:
                mctl_mpool[mpidx].p08.word = pHLdata;
                mctl_mpool[mpidx].b0D += 1; // inc  0x0D(ix)

                return; // jp   next__pool_idx
                break;
            }

            default:
                break;
            } // switch
        }
        while (0 != get_token); // break out if data-token (or jp 0BFF)

        // make sure we're not in a control-token (avoid using goto for "jp   l_0BFF_flite_pth_skip_load")
        if (mctld < 0xEF)
        {
            // l_0BDC_flite_pth_load
            mctl_mpool[mpidx].b0A = mctld & 0x0F;
            mctl_mpool[mpidx].b0B = (mctld >> 4) & 0x0F; // rlca * 4

            pHLdata += 1;

            if (0x80 & mctl_mpool[mpidx].b13) // bit  7,0x13(ix)
            {
                // negate rotation increment
                mctl_mpool[mpidx].b0C = -flv_get_data(pHLdata); // neg
            }
            else
            {
                //l_0BF7
                mctl_mpool[mpidx].b0C = flv_get_data(pHLdata);
            }
            pHLdata += 1;
            mctl_mpool[mpidx].b0D = flv_get_data(pHLdata);
            pHLdata += 1;
        } // if mctld

        // l_0BFF_flite_pth_skip_l
        mctl_mpool[mpidx].p08.word = pHLdata;

    } // if (0 == mctl_mpool[mpidx].b0D)

    // the rest of this would not be done in caller if return'd from cases above

    mctl_hpos_ck(mpidx); // l_0C05_ ... check home positions

    // l_0D03_ almost done ... update the sprite x/y positions
    mctl_posn_set(mpidx); // jp   z,l_0D03
}

/*=============================================================================
;; mctl_hpos_ck()
;;  Description:
;;    if homing flag set, check for proximity to home positions
;; IN:
;;
;; OUT:
;;
;; PRESERVES:
;;
;;---------------------------------------------------------------------------*/
static void mctl_hpos_ck(uint8 mpidx)
{
    // l_0C05_ ... bit-flag is set by case_0AA0
    if (0x40 & mctl_mpool[mpidx].b13) // bit  6 ... check if homing
    {
        // transitions to the next segment of the flight pattern
        if (mctl_mpool[mpidx].cy.pair.b1 == mctl_mpool[mpidx].b06
                ||
                (mctl_mpool[mpidx].cy.pair.b1 - mctl_mpool[mpidx].b06) == 1
                ||
                (mctl_mpool[mpidx].b06 - mctl_mpool[mpidx].cy.pair.b1) == 1)
        {
            if (mctl_mpool[mpidx].cx.pair.b1 == mctl_mpool[mpidx].b07
                    ||
                    (mctl_mpool[mpidx].cx.pair.b1 - mctl_mpool[mpidx].b07) == 1
                    ||
                    (mctl_mpool[mpidx].b07 - mctl_mpool[mpidx].cx.pair.b1) == 1)
            {
                uint8 A, L;

                // jp l_0E08 ... creature gets to home-spot
                mctl_mpool[mpidx].b13 &= ~0x01; // res  0,0x13(ix) ... mark the flying structure as inactive

                L = mctl_mpool[mpidx].b10;
                sprt_mctl_objs[L].state = HOME_RTN;

                A = (mrw_sprite.cclr[L].b1 + 1) & 0x07; // inc  a ... sprite color code

                if (A >= 5) // ... remaining bonus-bee returns to collective
                {
                }

                // l_0E3A
                // these could be off by one if not already equal
                mctl_mpool[mpidx].cy.word = mctl_mpool[mpidx].b06 << 8; // ld   0x01(ix),a
                mctl_mpool[mpidx].cx.word = mctl_mpool[mpidx].b07 << 8; // ld   0x03(ix),a

                return; // jp   l_0D03_flite_pth_posn_set
            }
        }
    }

    // else ...  jp l_0C2D

    // l_0C2D_
    mctl_rotn_incr(mpidx);
}

/*=============================================================================
;; mctl_rotn_incr()
;;  Description:
;;    Advance the rotation increment and select tile.
;;
;;      determine sprite ctrl
;;      Step-size in .b0C and 10-bit angle in .b04+.b05
;;                90          - angle in degrees
;;              1  | 0        - quadrant derived from 10-bit angle
;;           180 --+-- 0      - each tile rotation is 15 degrees (6 tiles per quadrant)
;;              2  | 3
;;                270
;;      b0: flipx - flip about the X axis, i.e. "up/down"
;;      b1: flipy - flip about the Y axis, i.e. "left/right"
;;       Quad  b05        U/D (b<0>)  LR = (b<0> ^ b<1>) + 1
;;       0     00  ->     0           0
;;       1     01  ->     0           1
;;       2     10  ->     1           1
;;       3     11  ->     1           0
;;
;; IN:
;;  mpidx - mctl pool index
;; OUT:
;;
;; PRESERVES:
;;
;;---------------------------------------------------------------------------*/
static void mctl_rotn_incr(uint8 mpidx)
{
    r16_t temp16;
    uint8 A, B, L;

    // l_0C2D_
    // red alien doesn't need special handling because it flies thru
    // screen and snaps to his home position column
    if (0x20 & mctl_mpool[mpidx].b13) // bit  5 ... check for yellow-alien or boss dive
    {
        if ((mctl_mpool[mpidx].cy.pair.b1 == mctl_mpool[mpidx].b06)
                ||
                (mctl_mpool[mpidx].b06 - mctl_mpool[mpidx].cy.pair.b1) == 1)
        {
            // set it up to expire on next step
            mctl_mpool[mpidx].b0D = 1;
            mctl_mpool[mpidx].b13 &= ~0x20; // res  5,0x13(ix)
        }
    }

    // l_0C46 ... below

    /*
     * determine_sprite_code
     */
    if (0x0100 & mctl_mpool[mpidx].ang.word) // bit  0,c
    {
        A = ~mctl_mpool[mpidx].ang.pair.b0; // cpl ... invert bits 7:0 in quadrant 1 and 3
    }
    else
    {
        A = mctl_mpool[mpidx].ang.pair.b0; // ld   a,e
    }

    // l_0C6D
    temp16.word = A + 42/2; // 0x15

    if (temp16.word & 0x0100)
    {
        // select vertical tile if within 21 degrees (half "step") of 90 or 270
        B = 6;
        // jr   l_0C81
    }
    else
    {
        A = temp16.pair.b0;

        // l_0C75 ... you just don't see this sort of thing anymore ...
        // divide by 42 ...42 counts per step of rotation (with 24 steps in the circle, 1 step is 42 degrees)
        // Here's the math: A * ( 1/2 + 1/4 ) * ( 1/32 )
        B = A >> 1; // srl  a ... etc
        A = (A >> 1) + (B >> 1); // srl  b
        A = A >> 5; // rlca x3
        B = A & 0x07;
    }

    // l_0C81
    L = mctl_mpool[mpidx].b10; // ld   l,0x10(ix)
    mrw_sprite.cclr[L].b0 = B | (mrw_sprite.cclr[L].b0 & 0xF8); // base code is multiple of 8

    /*
     * determine sprite ctrl
     */
    A = (mctl_mpool[mpidx].ang.pair.b1 >> 1) & 0x01; // rrc c
    mrw_sprite.ctrl[L].b0 = ((mctl_mpool[mpidx].ang.pair.b1 ^ A) + 1) << 1; // (b0 ^ b1) + 1
    mrw_sprite.ctrl[L].b0 = (mrw_sprite.ctrl[L].b0 | A) & 0x03; // and  #0x03 (double-x/double-y bits arrarently not used)


    // select displacement vector
    if (0x01 & ds3_92A0_frame_cts[0])
    {
        A = mctl_mpool[mpidx].b0A;
    }
    else
    {
        A = mctl_mpool[mpidx].b0B;
    }

    // l_0CA7
    if (A) // jp   z,l_0D03_
    {
        mctl_coord_incr(A, mpidx);
    }

    // l_0C46: now the rotation value for this slot can be updated
    mctl_mpool[mpidx].ang.word += (sint8) mctl_mpool[mpidx].b0C;
}


/*=============================================================================
;; mctl_coord_incr()
;;  Description:
      NOT a reused subroutine.
;;    Calculate next increment of X and Y coords from rotion angle.
;; IN:
;;  ds - displacement magnitude applied to x and y ordinates
;;  mpidx - mctl pool index
;; OUT:
;;
;; PRESERVES:
;;
;;---------------------------------------------------------------------------*/
static void mctl_coord_incr(uint8 ds, uint8 mpidx)
{
    r16_t * pv[2];
    r16_t pushDE, popHL, tmpAC;
    uint8 A, L;

    /*
      xor bit-7 with bit-8 ... test for orientation near 0 or 180
      i.e. < xx80 in quadrant 0 & 2, and >= xx80 in quadrant 1 & 3

      b9 b8  b7
     q 0  0   0  h
       0  0   1
     q 0  1   0
       0  1   1  h
     q 1  0   0  h
       1  0   1
     q 1  1   0
       1  1   1  h
    */
    // select displacement vector  ... jr   c,l_0CBF
    if (0x00 ==
            ((0 != (mctl_mpool[mpidx].ang.pair.b0 & 0x80)) ^
             (0 != (mctl_mpool[mpidx].ang.pair.b1 & 0x01)))) // jr   c,l_0CBF
    {
        // near horizontal orientation
        pv[0] = &mctl_mpool[mpidx].cx;
        pv[1] = &mctl_mpool[mpidx].cy;
    }
    else
    {
        // near vertical orientation
        pv[0] = &mctl_mpool[mpidx].cy;
        pv[1] = &mctl_mpool[mpidx].cx;
    }

    // integer from precision 9.7 format
    pushDE.word = (mctl_mpool[mpidx].ang.word & 0x03FF) << 1;

    // l_0CBF
    /*
      if  angle >135 && <304 then displacement is negated ... add 1, test bit-2
      b9 b8  b7  +1
     q 0  0   0  -> 001
       0  0   1  -> 010
     q 0  1   0  -> 011
       0  1   1  -> 100
     q 1  0   0  -> 101
       1  0   1  -> 110
     q 1  1   0  -> 111
       1  1   1  -> 000
    */
    // jr   z,l_0CC7
    if (0x04 & (pushDE.pair.b1 + 1)) // bit  2,d ... jr   z,l_0CC7
    {
        A = -ds; // neg
    }
    else
    {
        A = ds;
    }

    // l_0CC7
    tmpAC.word = A << 8; // ld   c,a ... from 0x0A(ix) or 0x0B(ix)
    tmpAC.word = (sint16) tmpAC.word >> 1; // sra  c ... sign extend and carry out of bit-0 of msb
    pv[0]->word += tmpAC.word; // adc  a,c

    // determine if minor ordinate is negative or positive
    if (0 == (0x0080 & mctl_mpool[mpidx].ang.word)) // jr   nc,l_0CE3
    {
        L = mctl_mpool[mpidx].ang.word & 0x007F;
    }
    else
    {
        L = ~mctl_mpool[mpidx].ang.word & 0x007F; // xor  #0x7F
    }

    // l_0CE3
    popHL.word = mctl_mul8(ds, L); // HL = L * A

    // calculate minor offset as a proportion to the ratio of the angle to 90
    // degress and adjust signage of offset to the quadrant and whether the
    // angle is closer to vertical or horizontal
    /*
             . 90          - angle in degrees
             1  | 0        - quadrant derived from 10-bit angle
          180 --+-- 0      - each tile rotation is 15 degrees (6 tiles per quadrant)
           . 2  | 3 .
             . 270
      b9 b8  b7
     q 0  0   0  -> 010 -> 001
       0  0   1  -> 011 -> 010
     q 0  1   0  -> 000 -> 111  x  .
       0  1   1  -> 001 -> 000
     q 1  0   0  -> 110 -> 101  x  .
       1  0   1  -> 111 -> 110  x  .
     q 1  1   0  -> 100 -> 011
       1  1   1  -> 101 -> 100  x  .
    */
    A = (pushDE.pair.b1 ^ 0x02) - 1; // xor  #0x02 ... msb of adjusted angle

    if (A & 0x04) // bit  2,a ... jr   z,l_0CFA
    {
        //and  a ...  whuuuuuut????

        pv[1]->word -= popHL.word; // sbc  hl,bc (negate the word)
    }
    else
    {
        // l_0CFA ... *HL += *DE
        pv[1]->word += popHL.word; // adc  a,(hl)
    }
}

/*=============================================================================
;; mctl_posn_set()
;;  Description:
;;    Updates sprite coordinates for specified object which is basically
;;    extracting the integer portion from fixed point 9.7 precision format
;;    as stored in the mctl pool coordinate registers.
;;    Also determines if bomb drop is activated.
;; IN:
;;   mpidx: index of mctl pool slot
;; OUT:
;;
;; PRESERVES:
;;
;;---------------------------------------------------------------------------*/
static void mctl_posn_set(uint8 mpidx)
{
    r16_t r16;
    uint8 Cy, A, L;

    L = mctl_mpool[mpidx].b10; // object index

    // extract x-coord and adjust for homing if needed
    r16.word = mctl_mpool[mpidx].cx.word << 1;
    A = r16.pair.b1; // recover integer portion from fixed point 9.7

    if (0 != glbls9200.flip_screen) // bit  0,c
    {
        A = 0xF0 - A + 0x02; // add  a,#0x0D ... cpl
    }
    // l_0D1A
    if (0 != (0x40 & mctl_mpool[mpidx].b13)) // bit  6,0x13(ix)
    {
        // heading home (formation x offset)
        A += mctl_mpool[mpidx].b11; // add  a,0x11(ix)
    }
    // l_0D23
    mrw_sprite.posn[L].b0 = A; // sX


    // extract y-coord and adjust for homing if needed
    r16.word = mctl_mpool[mpidx].cy.word >> 7; // extract 9-bit integer

    // for some reason pool y-coord tracked in flipped format
    if (0 == glbls9200.flip_screen) // bit  0,c
    {
        // reference calculation: (9C << 1) + 9E == 01D6 ... ~01D6 == FE29
        //                                 29 + 9E == C7 ... ~C7 == FF38
        // screen "virtual size" 0138+28=0160 so flipped y would be (160-y)
        // can "sorta" be done with 8-bit add by rolling the subtracted term
        // into an 8-bit magic number and handling bit-8 explicitly i.e.
        //   (0 - 0160 - 2) = FE9E ... so the following would work:
        //r16.word = r16.word - 0x0160 - 0x02;
        //r16.word = ~r16.word; // un-negate i.e. 1s comp but lazy omit +1
        // note mask of resultant bit-8 in assignment to sprite.ctrl.b1 below
        r16.word = 0x0160 - r16.word + 0x01; // how bout just use 16-bits!
    }
    // l_0D38 ... rr, rla, rl

    if (0 != (0x40 & mctl_mpool[mpidx].b13)) // bit  6,0x13(ix)
    {
        // heading home (formation y offset)
        r16.word += mctl_mpool[mpidx].b12; // add  a,0x12(ix)
    }
    // l_0D50
    mrw_sprite.posn[L].b1 = r16.pair.b0; // <0:7>
    mrw_sprite.ctrl[L].b1 = (mrw_sprite.ctrl[L].b1 & ~0x01) | (r16.pair.b1 & 0x01); // sprite[n].posn.sy<8>


    // Once the timer in $0E is reached, then check conditions to enable bomb drop.
    // If bomb is disabled for any reason, the timer is restarted.
    mctl_mpool[mpidx].b0E -= 1; // dec  0x0E(ix)

    if (0 == mctl_mpool[mpidx].b0E) // jp   nz,next__pool_idx
    {
        Cy = mctl_mpool[mpidx].b0F & 0x01;
        mctl_mpool[mpidx].b0F >>= 1; // srl  0x0F(ix)

        if (Cy
                &&
                mctl_mpool[mpidx].cy.pair.b1 >= (152 / 2) // cp   #0x4C ... 152>>1
                &&
                0 != task_actv_tbl_0[0x15] // fire button input
                &&
                0 == ds4_game_tmrs[1])
        {
            uint8 hl;
            // l_0D82: check for bomb available, 8 positions
            for (hl = 0; hl < 8; hl++)
            {
                // jr   z,l_0D8D_ ... bombs are rendered inactive at l_0815
                if (INACTIVE == sprt_mctl_objs[SPR_IDX_BOMB0 + hl * 2].state)
                {
                    r16_t bc16, hl16, a16, b16;
                    uint8 a, b;

                    // l_0D8D_
                    sprt_mctl_objs[SPR_IDX_BOMB0 + hl * 2].state = BOMB;

                    mrw_sprite.posn[SPR_IDX_BOMB0 + hl * 2].b0 = mrw_sprite.posn[L].b0; // x
                    mrw_sprite.posn[SPR_IDX_BOMB0 + hl * 2].b1 = mrw_sprite.posn[L].b1; // y<7:0>
                    mrw_sprite.ctrl[SPR_IDX_BOMB0 + hl * 2].b1 = mrw_sprite.ctrl[L].b1; // y<8>

                    bc16.pair.b1 = mrw_sprite.ctrl[L].b1; // y<8>
                    bc16.pair.b0 = mrw_sprite.posn[L].b1; // y<7:0>
                    b = bc16.word >> 1; // y<8:1>

                    hl16.pair.b1 = mrw_sprite.posn[SPR_IDX_SHIP].b0 -
                                   mrw_sprite.posn[L].b0 ; // dX;

                    if (mrw_sprite.posn[L].b0 > mrw_sprite.posn[SPR_IDX_SHIP].b0)
                    {
                        // result in 8-bits but parameter passed in hl to div16_8()
                        hl16.pair.b1 = -hl16.pair.b1; // neg ... ld   h,a
                    }

                    // l_0DB1:
                    a16.word = 298 >> 1; // 0x95 ... 354-56
                    if (glbls9200.flip_screen)  a16.word = 56 >> 1; // 0x1C ... 354-298

                    // l_0DBC:
                    a16.word -= b; // sub  b

                    if (0 != a16.pair.b1) // jr   nc,l_0DC1
                    {
                        a16.pair.b0 = -a16.pair.b0; // neg
                    }

                    // l_0DC1: determine x-rate of bomb ... dX/dY
// off by 1 ... 6569/56=12d
                    hl16.word = mctl_div_16_8(hl16.word, a16.pair.b0); // HL = HL / A
                    bc16.word = hl16.word;
                    hl16.word >>= 2;
                    hl16.word += bc16.word; // add  hl,bc
                    hl16.word >>= 2;

                    a = 0x60; // 96
                    if (0 == hl16.pair.b1) // jr   nz,l_0DE0
                    {
                        if (hl16.pair.b0 < 0x60) // jr   c,l_0DE2
                        {
                            //l_0DE0:
                            a = hl16.pair.b0; // ld   a,l
                        }
                    }
                    //l_0DE2:
                    // pop  af ... restore Cy from dX above, make b 16-bit to rr the Cy into b<7>
                    b16.word = mrw_sprite.posn[SPR_IDX_SHIP].b0 - mrw_sprite.posn[L].b0; // dX;
                    b16.pair.b0 = a; // ld   b,a
                    b16.word >>= 1; // rr   b

                    a = (SPR_IDX_BOMB0 + hl * 2 + 0x08) & 0x0F; // can get rid of IDX_BOMB0 and  & $0F but doesn't hurt anything
                    bomb_hrates[a].pair.b0 = b16.pair.b0;
                    bomb_hrates[a].pair.b1 = 0;

                    break; // end for
                }
                // else ... jr   l_0DF5_next_superloop_and_
            }
        }

        // l_0DF5_next_superloop_and_reload_0E
        mctl_mpool[mpidx].b0E = b_92E2_stg_parm[0]; // bomb drop counter
    }

    // next__pool_idx:

    // jp   for__pool_idx
}

/*=============================================================================
;; mctl_rotn_hp()
;;  Description:
;;    Calculate rotation angle to approach home position.
;; IN:
;;  D - object Y coord, 9.7 fixed-point upper byte (bits <8:1>)
;;  E - object X coord, 9.7 fixed-point upper byte (bits <8:1>)
;;  H,L - 10 bit rotation angle
;; OUT:
;;  HL
;; PRESERVES:
;;  BC, DE
;;---------------------------------------------------------------------------*/
static uint16 mctl_rotn_hp(uint16 _DE_, uint8 mctl_que_idx)
{
    r16_t rDE, rHL;
    uint8 A, B, C, D, E, L, Cy, pushCy;

    rDE.word = _DE_;
    E = rDE.pair.b0; // cX
    D = rDE.pair.b1; // cY

    A = E - mctl_mpool[mctl_que_idx].cx.pair.b1; // sub  l
    B = 0;

    if (mctl_mpool[mctl_que_idx].cx.pair.b1 > E) // jr   nc,l_0E67
    {
        B = 1; // set  0,b
        A = -A; // neg
    }

    // l_0E67:
    C = A;

    A = D - mctl_mpool[mctl_que_idx].cy.pair.b1; // sub  h

    if (mctl_mpool[mctl_que_idx].cy.pair.b1 > D) // jr   nc,l_0E76
    {
        B ^= 1; // xor  #0x01
        B |= 2; // or   #0x02

        A = -A; // neg
    }

    // l_0E76:
    Cy = (C > A); // cp   c
    pushCy = Cy; // push af
    Cy ^= B; // rla ... rra
    Cy ^= 1; // ccf
    B <<= 1; // rl   b ...
    B |= (Cy & 0x01); // ... rl   b

    // pop  af

    if (pushCy)
    {
        D = C;
        C = A;
        A = D;
    }

    // l_0E84:
    rHL.word = C << 8; // ld   h,c

    rHL.word = mctl_div_16_8(rHL.word, A); // HL = HL / A

    L = rHL.pair.b0;

    A = rHL.pair.b1; // ld   a,h
    A ^= B; // xor  b
    A &= 0x01; // and  #0x01

    // jr   z,l_0E93
    if (A)
    {
        L = ~rHL.pair.b0; // cpl
    }

    // l_0E93
    rHL.pair.b0 = L;
    rHL.pair.b1 = B; // ld   h,b

    return rHL.word;
}

/*=============================================================================
;; mctl_mul8()
;;  Description:
;;    calculate 16-bit product of 2 8-bit integers
;;    HL = HL * A
;; IN:
;;  A - displacement magnitude
;;  HL (only L is significant)
;; OUT:
;;  H
;;  L
;; PRESERVES:
;;  DE
;;---------------------------------------------------------------------------*/
static uint16 mctl_mul8(uint8 A, uint8 L)
{
    r16_t HL, DE;

    DE.word = L; // ex   de,hl
    HL.word = 0;

    do
    {
        if (A & 0x01) // jr   nc,l_0EA1
            HL.word += DE.word;

        // l_0EA1
        DE.word <<= 1; // sla  e ... rl   d
        A >>= 1; // srl  a
    }
    while (0 != A);

    return HL.word;
}

/*=============================================================================
;; mctl_div_16_8()
;;  Description:
;;   HL = HL / A  ... the hard way
;; IN:
;;  A, HL
;; OUT:
;;  HL
;; PRESERVES:
;;  BC
;;---------------------------------------------------------------------------*/
static uint16 mctl_div_16_8(uint16 HL, uint8 A)
{
    uint32 Cy16; // carry out from adc hl
    r16_t rA, rHL;
    uint8 B, C;
    uint8 Cy;

    rHL.word = HL;
    C = A;
    rA.word = 0; // xor  a ... clears Cy
    Cy = 0;
    B = 0; // 0x11;

    // l_0EAF:
    while (B < 17)
    {
        rA.word = rA.pair.b0 + rA.pair.b0 + Cy; // adc  a,a

        // jr   c,l_0EBD
        if (rA.word < 0x0100)
        {
            Cy = 1;
            // cp   c
            // jr   c,l_0EB6
            if (C <= rA.pair.b0)
            {
                Cy = 0;
                rA.pair.b0 -= C; // sub  c
            }
            // l_0EB6:
            Cy ^= 1; // ccf
        }
        else
        {
            // l_0EBD:
            rA.pair.b0 -= C; // sub  c
            Cy = 1; // scf
            // jp   l_0EB7
        }

        // l_0EB7:
        Cy16 = rHL.word + rHL.word + Cy; // adc  hl,hl
        rHL.word = Cy16;
        Cy = (0 != (Cy16 & 0x00010000)); // overflow out of 16-bits

        B += 1; // djnz l_0EAF
    }
    // pop  bc
    return rHL.word;
}

/*=============================================================================
;; f_0ECA()
;;  Description:
;;   Reads dsw3 which is doesn't seem to have any function (MAME list as unused).
;;   If the switch were active (0) then the section of code would be reading
;;   from code space locations beyond the $1000. Also odd is the conditional
;;   rst  $00.
;; IN:
;;  ...
;; OUT:
;;  ...
;;---------------------------------------------------------------------------*/
void f_0ECA(void)
{

}

/*---------------------------------------------------------------------------*/

// additional challenge stage data (see db_2A3C)
static const uint8 flv_d_0fda[] =
{
    0x23,0x00,0x1B,0x23,0xF0,0x40,0x23,0x00,0x09,0x23,0x05,0x11,
    0x23,0x00,0x10,0x23,0x10,0x40,0x23,0x04,0x30,0xFF
};
static const uint8 flv_d_0ff0[] =
{
    0x23,0x02,0x35,0x23,0x08,0x10,
    0x23,0x10,0x3C,0x23,0x00,0xFF,0xFF
};

/*---------------------------------------------------------------------------*/

// offsets from gg1-5 map file - can use t_flv_offs enum in place of hard
// values for what it's worth
void flv_init_data(void)
{
    FLV_MCPY( flv_d_001d      , 0x001D) // stg 1
    FLV_MCPY( flv_d_00f1      , 0x00F1)
    FLV_MCPY( flv_p_004b      , 0x004B) // this one is a "jump" but is not contigous with previous so must be copied explicitly
    FLV_MCPY( flv_d_0067      , 0x0067) // stg 1
    FLV_MCPY( flv_d_009f      , 0x009F) // stg 2
//  FLV_MCPY(_flv_i_00b6      , 0x00B6)
//  FLV_MCPY(_flv_i_00cc      , 0x00CC)
    FLV_MCPY( flv_d_00d4      , 0x00D4) // stg 2
//  FLV_MCPY(_flv_i_0160      , 0x0160)
//  FLV_MCPY(_flv_i_0173      , 0x0173)
    FLV_MCPY( flv_d_017b      , 0x017B)
//  FLV_MCPY(_flv_i_0192      , 0x0192)
//  FLV_MCPY(_flv_i_01a8      , 0x01A8)
    FLV_MCPY( flv_d_01b0      , 0x01B0)
//  FLV_MCPY(_flv_i_01ca      , 0x01CA)
//  FLV_MCPY(_flv_i_01e0      , 0x01E0)
    FLV_MCPY( flv_d_01E8      , 0x01E8) // chllg stg (3)
    FLV_MCPY( flv_d_01F5      , 0x01F5) // chllg stg (3)
    FLV_MCPY( flv_d_020b      , 0x020B)
    FLV_MCPY( flv_d_021b      , 0x021B)
    FLV_MCPY( flv_d_022b      , 0x022B)
    FLV_MCPY( flv_d_0241      , 0x0241)
    FLV_MCPY( flv_d_025d      , 0x025D)
    FLV_MCPY( flv_d_0279      , 0x0279)
    FLV_MCPY( flv_d_029e      , 0x029E)
    FLV_MCPY( flv_d_02ba      , 0x02BA)
FLV_MCPY( flv_d_02d9      , 0x02D9)
FLV_MCPY( flv_d_02fb      , 0x02FB)
FLV_MCPY( flv_d_031d      , 0x031D)
FLV_MCPY( flv_d_0333      , 0x0333)
    FLV_MCPY( flv_d_atk_yllw  , 0x034F)
//  FLV_MCPY(_flv_i_0352      , 0x0352)
//  FLV_MCPY(_flv_i_0358      , 0x0358)
//  FLV_MCPY(_flv_i_0363      , 0x0363)
//  FLV_MCPY(_flv_i_036c      , 0x036C)
//  FLV_MCPY(_flv_i_037c      , 0x037C)
//  FLV_MCPY(_flv_i_039e      , 0x039E)
    FLV_MCPY( flv_d_atk_red   , 0x03A9)
//  FLV_MCPY(_flv_i_03ac      , 0x03AC)
//  FLV_MCPY(_flv_i_03cc      , 0x03CC)
//  FLV_MCPY(_flv_i_03d7      , 0x03D7)
//  FLV_MCPY(_flv_i_040c      , 0x040C)
  FLV_MCPY( flv_d_0411      , 0x0411)
//  FLV_MCPY(_flv_i_0414      , 0x0414)
//  FLV_MCPY(_flv_i_0420      , 0x0420)
//  FLV_MCPY(_flv_i_0425      , 0x0425)
//  FLV_MCPY(_flv_i_0430      , 0x0430)
//  FLV_MCPY( flv_d_cboss      , 0x046B)
//  FLV_MCPY(_flv_i_0499      , 0x0499)
//  FLV_MCPY( flv_d_04c6      , 0x04C6)
//  FLV_MCPY(_flv_i_04c6      , 0x04C6)
//  FLV_MCPY(_flv_i_04cf      , 0x04CF)
//  FLV_MCPY( flv_d_04cf      , 0x04CF)
//  FLV_MCPY( flv_d_04d8      , 0x04D8)
//  FLV_MCPY(_flv_i_04d8      , 0x04D8)
//  FLV_MCPY( flv_d_0502      , 0x0502)
//  FLV_MCPY(_flv_i_0502      , 0x0502)
    FLV_MCPY( flv_d_0fda      , 0x0FDA)
    FLV_MCPY( flv_d_0ff0      , 0x0FF0)
}
