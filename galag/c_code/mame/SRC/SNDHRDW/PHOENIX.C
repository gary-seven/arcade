#include "driver.h"

#define SALIMIT 1000
#define SAFREQ  1400
#define SBLIMIT 1000
#define SBFREQ  1400


/* I played around with these until they sounded ok */
static max_freq = 22050;
static inc_freq = 4500;
static dec_freq = 1750;

/* values needed by phoenix_sh_update */
static int sound_a_play = 0;
static int sound_a_adjust = -SALIMIT;
static int sound_a_sine[4] = { 0, 250, 0, -250 };
static int sound_a_sine_adjust = 0;
static int sound_a_vol = 0;
static int sound_a_freq = SAFREQ;

static int sound_b_play = 0;
static int sound_b_adjust = SBLIMIT;
static int sound_b_vol = 0;
static int sound_b_freq = SBFREQ;

static int noise_vol = 0;
static int noise_freq = 1000;

int noisemulate = 0;

int phoenix_sh_init(const char *gamename)
{
        if (Machine->samples != 0 && Machine->samples->sample[0] != 0)    /* We should check also that Samplename[0] = 0 */
          noisemulate = 0;
        else
          noisemulate = 1;

        return 0;
}


void phoenix_sound_control_a_w(int offset,int data)
{
        static int lastnoise;

        /* voice a */
        int freq = data & 0x0f;
        int vol = (data & 0x30) >> 4;

        /* noise */
        int noise = (data & 0xc0) >> 6;

        if (freq != sound_a_freq) sound_a_adjust = 0;
        sound_a_freq = freq;
        sound_a_vol = vol;

        if (freq != 0x0f)
        {
                osd_adjust_sample(0,max_freq-(dec_freq*freq),85*(3-vol));
                sound_a_play = 1;
        }
        else
        {
                osd_adjust_sample(0,SAFREQ,0);
                sound_a_play = 0;
        }

        if (noisemulate) {
           if (noise_freq != 1750*(4-noise))
           {
                   noise_freq = 1750*(4-noise);
                   noise_vol = 85*noise;
           }

           if (noise) osd_adjust_sample(2,noise_freq,noise_vol);
           else
           {
                   osd_adjust_sample(2,1000,0);
                   noise_vol = 0;
           }
         }
        else
         {
           switch (noise) {
             case 1 : if (lastnoise != noise)
                        osd_play_sample(2,Machine->samples->sample[0]->data,
                                          Machine->samples->sample[0]->length,
                                          Machine->samples->sample[0]->smpfreq,
                                          Machine->samples->sample[0]->volume,0);
                      break;
             case 2 : if (lastnoise != noise)
                        osd_play_sample(2,Machine->samples->sample[1]->data,
                                          Machine->samples->sample[1]->length,
                                          Machine->samples->sample[1]->smpfreq,
                                          Machine->samples->sample[1]->volume,0);
                      break;
           }
           lastnoise = noise;
         }
}



void phoenix_sound_control_b_w(int offset,int data)
{
        /* voice b */
        int freq = data & 0x0f;
        int vol = (data & 0x30) >> 4;

        /* melody - osd_play_midi anyone? */
        /* 0 - no tune, 1 - alarm beep?, 2 - even level tune, 3 - odd level tune */
        /*int tune = (data & 0xc0) >> 6;*/

        if (freq != sound_b_freq) sound_b_adjust = 0;
        sound_b_freq = freq;
        sound_b_vol = vol;

        if (freq != 0x0f)
        {
                osd_adjust_sample(1,(dec_freq*freq),85*(3-vol));
                sound_b_play = 1;
        }
        else
        {
                osd_adjust_sample(1,SBFREQ,0);
                sound_b_play = 0;
        }
}



int phoenix_sh_start(void)
{
        osd_play_sample(0,Machine->drv->samples,32,1000,0,1);
        osd_play_sample(1,Machine->drv->samples,32,1000,0,1);
        osd_play_sample(2,&Machine->drv->samples[32],128,1000,0,1);
	return 0;
}



void phoenix_sh_update(void)
{
        sound_a_adjust++;
        sound_b_adjust++;
        sound_a_sine_adjust++;

        if (sound_a_adjust == SALIMIT) sound_a_adjust = -SALIMIT;
        if (sound_b_adjust == SBLIMIT) sound_b_adjust = -SBLIMIT;
        if (sound_a_sine_adjust == 4) sound_a_sine_adjust = 0;

        if (sound_a_play)
                osd_adjust_sample(0,((inc_freq*sound_a_freq)+sound_a_adjust+sound_a_sine[sound_a_sine_adjust]),85*(3-sound_a_vol));
        if (sound_b_play)
                osd_adjust_sample(1,max_freq-((dec_freq*sound_b_freq)+sound_b_adjust),85*(3-sound_b_vol));

        if ((noise_vol) && (noisemulate))
        {
                osd_adjust_sample(2,noise_freq,noise_vol);
                noise_vol-=3;
        }

}
