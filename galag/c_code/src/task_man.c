/*******************************************************************************
 **  galag: precise re-implementation of a popular space shoot-em-up
 **  task_man.s (gg1-1.3p)
 **
 **  The "task manager" is triggered by the v-blank interrupt (RST 38)
 **  thus the base execution rate is 60Hz. Some tasks will implement
 **  their own sub-rates (e.g. 1 Hz, 4 Hz etc) by checking a global timer.
 **
 **  ds_cpu0_task_activ ($20 bytes) is indexed by order of the
 **  function pointers in d_cpu0_task_table. Periodic tasks can be prioritized,
 **  enabled and disabled by changing the appropriate index in the table.
 **  The task enable table is accessed globally allowing one task to enable or
 **  disable another task. At startup, actv_task_tbl ($20 bytes) is loaded with
 **  a default configuration from ROM.
 **
 **  In ds_cpu0_task_activ the following values are used:
 **   $00 - will skip first entry ($0873) but continue with second
 **   $01
 **   $1f - execute first then skip to last? (but it sets to $00 again?)
 **   $20 - will execute $0873 (empty task) then immediately exit scheduler
 **
 *******************************************************************************/
/*
 ** header file includes
 */
#include <string.h> // memset
#include "galag.h"
#include "task_man.h"

/*
 ** defines and typedefs
 */

/*
 ** extern declarations of variables defined in other files
 */

/*
 ** non-static external definitions this file or others
 */
t_struct_plyr_state plyr_state_actv;
t_struct_plyr_state plyr_state_susp;
uint8 task_actv_tbl_0[32]; // active plyr task tbl cpu0
uint8 task_resv_tbl_0[32]; // suspended plyr task tbl cpu0
uint8 ds4_game_tmrs[4];
uint16 w_bug_flying_hit_cnt;
// object-collision notification to f_1DB3 from cpu1:c_076A
uint8 b_9200_obj_collsn_notif[0x60]; // only even-bytes used (uint16?)


/*
 ** static external definitions in this file
 */

// variables
static uint8 d_str20000[];
static uint8 d_strScore[];

// function prototypes
static void c_01C5_new_stg_game_or_demo();


/**********************************************************************
;; d_cpu0_task_table
;;  Description:
;;   jump table for functions called by task manager.
;;   32 entries
 **********************************************************************/
void (* const d_cpu0_task_table[]) (void) =
{
    f_0827,
    f_0828,
    f_17B2,
    f_1700,
    f_1A80,
    f_0857,
    f_0827,
    f_0827,

    f_2916,
    f_1DE6,
    f_2A90,
    f_1DB3,
    f_23DD,
    f_1EA4,
    f_1D32,
    f_0935,

    f_1B65,
    f_19B2,
    f_1D76,
    f_0827,
    f_1F85,
    f_1F04,
    f_0827,
    f_1DD2,

    f_2222,
    f_21CB,
    f_0827,
    f_0827,
    f_20F2,
    f_2000,
    f_0827,
    f_0977
};

// helper macro for table size
#define SZ_TASK_TBL  sizeof(d_cpu0_task_table) / sizeof(void *)

/*=============================================================================*/
// string "1UP    HIGH SCORE"  (reversed)
static const uint8 d_TxtScore[] =
{
    0x0E, 0x1B, 0x18, 0x0C, 0x1C, 0x24, 0x11, 0x10, 0x12, 0x11, 0x24, 0x24, 0x24, 0x24, 0x19, 0x1E, 0x01
};

/*=============================================================================
;; c_textout_1uphighscore_onetime()
;;  Description:
;;   display score text top of screen (1 time only after boot)
;; IN:
;;  ...
;; OUT:
;;  ...
-----------------------------------------------------------------------------*/
void c_textout_1uphighscore_onetime(void)
{
    int bc;

    bc = 6;//sizeof (d_str20000 - 1); //  6

    while (bc > 0)
    {
        m_tile_ram [ 0x03E0 + 0x0D + bc -1 ] = d_str20000[ bc - 1 ];
        bc--;
    }

    bc = 17;//sizeof (d_TxtScore) - 1; // 17

    while (bc > 0)
    {
        m_tile_ram [ 0x03C0 + 0x0B + bc -1 ] = d_TxtScore[ bc - 1 ];
        bc--;
    }
}


/*=============================================================================
;; Home positions of objects in the cylon fleet. Replicated in gg1-5.s
;; Refer to diagram:
;;
;; object[] {
;;  location.row    ...index to row pixel LUTs
;;  location.column ...index to col pixel LUTs
;; }
;;                  00 02 04 06 08 0A 0C 0E 10 12
;;
;;     14                    00 04 06 02            ; captured vipers
;;     16                    30 34 36 32            ; base stars
;;     18              40 48 50 58 5A 52 4A 42      ; raiders
;;     1A              44 4C 54 5C 5E 56 4E 46
;;     1C           08 10 18 20 28 2A 22 1A 12 0A
;;     1E           0C 14 1C 24 2C 2E 26 1E 16 0E
;;
;;  organization of row and column pixel position LUTs (ds_home_posn_org etc.):
;;
;;      |<-------------- COLUMNS --------------------->|<---------- ROWS ---------->|
;;
;;      00   02   04   06   08   0A   0C   0E   10   12   14   16   18   1A   1C   1E
;;
;;----------------------------------------------------------------------------*/
// db_obj_home_posn_rc

/*=============================================================================
;; c_sctrl_playfld_clr()
;;  Description:
;;    Clears playfield tileram (not the score and credit texts at top & bottom).
;;
;;    Tile RAM layout (color RAM is same, starting at 8400):
;;     Tile rows  0-1:   $8300 - 803F
;;     Playfield area:   $8040 - 83BF
;;     Tile rows 34-35:  $83C0 - 83FF
;;
;;     2 bytes at each end of tile rows 0,1,34,35 are not visible.
;;
;;     2 bytes |                                     | 2 bytes (not visible)
;;    ----------------------------------------------------
;;    .3DF     .3DD                              .3C2  .3C0     <- Row 0
;;    .3FF     .3FD                              .3E2  .3E0     <- Row 1
;;             .3A0-------------------------.060 .040
;;               |                             |   |
;;             .3BF-------------------------.07F .05F
;;    .01F     .01D                              .002  .000     <- Row 34
;;    .03F     .03D                              .022  .020     <- Row 35
;;
;; IN:
;;  ...
;; OUT:
;;  ...
;;-----------------------------------------------------------------------------*/
void c_sctrl_playfld_clr(void)
{
    uint16 BC;
    uint8 *HL;

    HL = m_tile_ram;
    for (BC = 0; BC < 0x0380; BC++)
    {
        HL[ 0x0040 + BC ] = 0x24;
    }

    HL = m_color_ram;
    for (BC = 0; BC < 0x0380; BC++)
    {
        HL[ 0x0040 + BC ] = 0;
    }

    // HL==87bf
    // Set the color (red) of the topmost row: let the pointer in HL wrap
    // around to the top row fom where it left off from the loop above.
    memset(m_color_ram + 0x03BF, 4, 0x20);

    // HL==87df
    // Set color of 2nd row from top, again retaining pointer value from.
    // previous loop. Why $4E? I don't know but it ends up white.
    memset(m_color_ram + 0x03DF, 0x4E, 0x20);
}

/*=============================================================================
;; c_new_stg_game_only()
;;  Description:
;;   clears a stage (on two-player game, runs at the first turn of each player)
;;   Increments stage_ctr (and dedicated challenge stage %4 indicator)
;; IN:
;;  ...
;; OUT:
;;  ...
;;-----------------------------------------------------------------------------*/
void c_new_stg_game_only(void)
{
    int usres;
    uint8 Cy;

    plyr_state_actv.stage_ctr += 1;

    // determine stage count modulus ... gives 0 for challenge stage
    plyr_state_actv.not_chllng_stg = (plyr_state_actv.stage_ctr + 1) & 0x03; // 0 if challenge stage

    if (0 != plyr_state_actv.not_chllng_stg)
    {
        uint16 HL;
        HL = j_string_out_pe(1, -1, 6); // string_out_pe "STAGE "

        // Print "X" of STAGE X. ...HL == $81B0
        c_text_out_i_to_d(plyr_state_actv.stage_ctr, HL);

        // l_01AC: ; start value for wave_bonus_ctr (decremented by cpu-b when bug destroyed)
        w_bug_flying_hit_cnt = 0; // irrelevant if !challenge stage
    }
    else
    {
        // l_01A2_set_challeng_stg:
        j_string_out_pe(1, -1, 7); // "CHALLENGING STAGE"

        b_9AA0[0x0D] = 1 ; // sound-fx count/enable registers, start challenge stage

        // l_01AC: ; start value for wave_bonus_ctr (decremented by cpu-b when bug destroyed)
        w_bug_flying_hit_cnt = 8; // 8 for challenge stage (else 0 i.e. don't care)
    }

    // set the timer to synchronize finish of c_new_level_tokens
    ds4_game_tmrs[2] = 3;

    glbls9200.flying_bug_attck_condtn = 3; // 3 (begin round ... use 3 for optimization, but merely needs to be !0)

    /*
      Set Cy to inhibit sound clicks for level tokens at challenge stage.
      Argument "A" (loaded from plyr_actv.b_not_chllg_stg) not used here
      to pass to c_build_token_1 (sets b_9AA0[0x15] sound count/enable)
     */
    Cy = (0 == plyr_state_actv.not_chllng_stg); // 1211

    //  and  a ... if A != 0, clear Cy
    //  ex   af,af' ... Cy' == 1 if inhibit sound clicks
    c_new_level_tokens(Cy); // A' == 0 if challenge stg, else non-zero (stage_ct + 1)

    // l_01BF:
    while (ds4_game_tmrs[2])
    {
        if (0 != (usres = _updatescreen(1))) // 1 == blocking wait for vblank
        {
            /* goto getout; */ // 1=blocking
        }
    }

    c_01C5_new_stg_game_or_demo();
}

/*=============================================================================
;; c_01C5_new_stg_game_or_demo()
;;  Description:
;;   Continue c_new_stg_game_only, or called in the demo to allow skipping
;;   of the "STAGE X" text.
;;   If Rack Advance set, continues looping back through c_new_stg_game_only
;;   If Rack Advance not set, it does a normal return.
;; IN:
;;  ...
;; OUT:
;;  ...
;;-----------------------------------------------------------------------------*/
static void c_01C5_new_stg_game_or_demo()
{
    uint8 *pHL;
    uint8 B;

    ds4_game_tmrs[2] = 120;

    c_2896(); // Initializes each creature by position
    c_25A2(); // mob setup

    ds4_game_tmrs[0] = 2;

    c_12C3(0); //  A==0 ... set MOB coordinates, new stage

    for (B = 0; B < 0x60; B += 2)
    {
        b_9200_obj_collsn_notif[B] = 0;
    }


    task_actv_tbl_0[0x09] = 0; // f_1DE6 ... collective bug movement
    task_actv_tbl_0[0x10] = 0; // f_1B65 ... manage flying-bug-attack
    task_actv_tbl_0[0x04] = 0; // f_1A80 ... bonus-bee manager

    b_bug_flyng_hits_p_round = 0;

    plyr_state_actv.captur_boss_enable = 0; // disable demo boss capture overide
    plyr_state_actv.bonus_bee_launch_tmr = 0;
    plyr_state_actv.b_atk_wv_enbl = 0;
    plyr_state_actv.b_attkwv_ctr = 0;
    //b8_99B0_X3attackcfg_ct = 0;
    plyr_state_actv.nest_lr_flag = 0;

    plyr_state_actv.bonus_bee_obj_offs = 1;
    plyr_state_susp.bonus_bee_obj_offs = 1;
    plyr_state_susp.captur_boss_obj_offs = 1;

    task_actv_tbl_0[0x0B] = 1; // f_1DB3 ... Update enemy status
    task_actv_tbl_0[0x08] = 1; // f_2916 ... Launches the attack formations
    task_actv_tbl_0[0x0A] = 1; // f_2A90 ... left/right movement of collective while attack waves coming

    c_2C00_new_stg_setup();

    pHL = plyr_state_susp.pbm;
    B = 0;
    while (B++ < 4)
    {
        *pHL++ = 0x01;
        *pHL++ = 0xB5;
    }

    // if ( !RackAdvance )
    return;
}


/*=============================================================================
;; jp_Task_man()
;;  Description:
;;   handler for rst $38
;;   Updates star control registers.
;;   Executes the Scheduler.
;;   Sets IO chip for control input.
;;   The task enable table is composed of 1-byte entries corresponding to each
;;   of $20 tasks. Each cycle starts at task[0] and advances an index for each
;;   entry in the table. The increment value is actually obtained from the
;;   task_enable table entry itself, which is normally 1, but other values are
;;   also used, such as $20. The "while" logic exits at >$20, so this is used
;;   to exit the task loop without iterating through all $20 entries. Tthe
;;   possible enable values are:
;;     $00 - disables task
;;     $01 - enables task_man
;;     $0A -
;;     $1F -   1F + 0A = $29     (where else could $0A be used?)
;;     $20 - exit current task man step after the currently executed task.
;; IN:
;;  ...
;; OUT:
;;  ...
-----------------------------------------------------------------------------*/
void cpu0_rst38(void)
{
    uint8 C = 0;

    while (C < SZ_TASK_TBL) // 32
    {
        // loop until non-zero: assumes we will find a valid entry in the table!
        while (0 == task_actv_tbl_0[C])
        {
            C++;
        }

        d_cpu0_task_table[C]();

        C += task_actv_tbl_0[C];
    }
}


/*=============================================================================*/
//  "20000" (reversed)
static uint8 d_str20000[] =
{
    0x00, 0x00, 0x00, 0x00, 0x02, 0x24
};
//  "SCORE" (reversed)
static uint8 d_strScore[] =
{
    0x17, 0x0A, 0x16, 0x0C, 0x18
};
/*-----------------------------------------------------------------------------*/

