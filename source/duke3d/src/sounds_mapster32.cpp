//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

// Stripped sounds.c for use in Mapster32, breaks all ties to game & music

#include "compat.h"
#include "baselayer.h"

#include "fx_man.h"
#include "osd.h"

#include "cache1d.h"
#include "build.h"  // vec3_t
#include "editor.h"

#include "macros.h"
#include "common.h"

#ifdef _WIN32
#include "winlayer.h"
#endif

#include "sounds_mapster32.h"
#include "common.h"
#include "common_game.h"

#include "vfs.h"

#define LOUDESTVOLUME 150
#define MUSICANDSFX 5

static char SM32_havesound = 0;

static char SoundToggle = 1;
int32_t NumVoices = 32;
int32_t MixRate = 44100;

int32_t g_numEnvSoundsPlaying;

void S_Callback(intptr_t);

int16_t g_definedsndnum[MAXSOUNDS];
int16_t g_sndnum[MAXSOUNDS];
int32_t g_numsounds = 0;

/*
===================
=
= SoundStartup
=
===================
*/


int32_t S_SoundStartup(void)
{
    int32_t status;
    void *initdata = 0;

    // TODO: read config
    int32_t FXVolume=220, /*NumVoices=32,*/ NumChannels=2;

#ifdef _WIN32
    initdata = (void *) win_gethwnd(); // used for DirectSound
#endif

    initprintf("Initializing sound... ");

    status = FX_Init(NumVoices, NumChannels, MixRate, initdata);
    if (status != FX_Ok)
    {
        initprintf("Sound startup error: %s\n", FX_ErrorString(status));
        return -2;
    }

    FX_SetVolume(FXVolume);
#ifdef ASS_REVERSESTEREO
    FX_SetReverseStereo(0);
#endif
    FX_SetCallBack(S_Callback);

    SM32_havesound = 1;

    return 0;
}

/*
===================
=
= SoundShutdown
=
===================
*/

void S_SoundShutdown(void)
{
    if (!SM32_havesound)
        return;

    int status = FX_Shutdown();
    if (status != FX_Ok)
        initprintf("Sound shutdown error: %s\n", FX_ErrorString(status));
}

int32_t S_LoadSound(uint32_t num)
{
    if (!SM32_havesound) return 0;
    if (num >= MAXSOUNDS || SoundToggle == 0) return 0;

    if (g_sounds[num].filename == NULL)
    {
        OSD_Printf(OSD_ERROR "Sound (#%d) not defined!\n",num);
        return 0;
    }

    buildvfs_kfd fp = S_OpenAudio(g_sounds[num].filename, 0, 0);
    if (fp == buildvfs_kfd_invalid)
    {
        OSD_Printf(OSDTEXT_RED "Sound %s(#%d) not found!\n",g_sounds[num].filename,num);
        return 0;
    }

    int32_t l = kfilelength(fp);
    g_sounds[num].soundsiz = l;

    g_sounds[num].lock = CACHE1D_LOCKED;

    g_cache.allocateBlock((intptr_t *)&g_sounds[num].ptr,l,(char *)&g_sounds[num].lock);
    kread(fp, g_sounds[num].ptr , l);
    kclose(fp);
    return 1;
}

int32_t S_DefineSound(int sndidx, const char * filename, const char * definedname, int minpitch, int maxpitch, int priority, int type, int distance, float volume)
{
    if ((unsigned)sndidx >= MAXSOUNDS)
        return -1;

    auto & snd = g_sounds[sndidx];
    int duplicate = 0;

    if (snd.filename)
    {
        duplicate = 1;
        Xfree(snd.filename);
    }
    snd.filename = Xstrdup(filename);
    // Hopefully noone does memcpy(..., g_sounds[].filename, BMAX_PATH)

    if (snd.definedname)
    {
        duplicate = 1;
        Xfree(snd.definedname);
    }
    if (duplicate)
        initprintf("warning: duplicate sound #%d, overwriting\n", sndidx);

    if (!duplicate)
    {
        if ((unsigned)g_numsounds >= MAXSOUNDS)
            return -2;
        g_sndnum[g_numsounds] = g_definedsndnum[g_numsounds] = sndidx;
        g_numsounds++;
    }

    snd.definedname = Xstrdup(definedname);  // we want to keep it for display purposes
    snd.ps = clamp(minpitch, INT16_MIN, INT16_MAX);
    snd.pe = clamp(maxpitch, INT16_MIN, INT16_MAX);
    snd.pr = priority & 0xFF;
    snd.m = type;
    snd.vo = clamp(distance, INT16_MIN, INT16_MAX);
    snd.volume = volume * fix16_one;

    return 0;
}

int32_t S_PlaySound3D(int32_t const num, int32_t i, const vec3_t *pos)
{
    int32_t sndist, cx, cy, cz, j/*,k*/;
    int32_t pitche,pitchs,cs;
    int32_t voice, sndang, ca, pitch;

    //    if(num != 358) return 0;

    if (num >= MAXSOUNDS ||
            !SM32_havesound ||
            SoundToggle == 0)
        return -1;

    auto & snd = g_sounds[num];

    if (snd.num > 3 ||
//        ((snd.m & SF_ADULT) && ud.lockout) ||
            FX_VoiceAvailable(snd.pr) == 0)
        return -1;

    if (snd.m & SF_DTAG)
    {
        S_PlaySound(num);
        return 0;
    }

    if (snd.m & SF_TALK)
    {
        for (j=0; j<MAXSOUNDS; j++)
        {
            auto const & jsnd = g_sounds[j];
//            for (k=0; k<jsnd.num; k++)
            if ((jsnd.num > 0) && (jsnd.m & SF_TALK))
                return -1;
        }
    }

    cx = pos->x;
    cy = pos->y;
    cz = pos->z;
    cs = cursectnum;
    ca = ang;

    sndist = FindDistance3D((cx-pos->x),(cy-pos->y),(cz-pos->z));

    if (i >= 0 && (snd.m & SF_GLOBAL) == 0 && PN(i) == MUSICANDSFX && SLT(i) < 999 && (sector[SECT(i)].lotag&0xff) < 9)
        sndist = divscale14(sndist,(SHT(i)+1));

    pitchs = snd.ps;
    pitche = snd.pe;
    cx = klabs(pitche-pitchs);

    if (cx)
    {
        if (pitchs < pitche)
            pitch = pitchs + (wrand()%cx);
        else
            pitch = pitche + (wrand()%cx);
    }
    else
        pitch = pitchs;

    sndist += snd.vo;
    if (sndist < 0) sndist = 0;
    if (cs > -1 && sndist && PN(i) != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,SX(i),SY(i),SZ(i)-(24<<8),SECT(i)))
        sndist += sndist>>5;
#if 0
        switch (num)
        {
        case PIPEBOMB_EXPLODE:
        case LASERTRIP_EXPLODE:
        case RPG_EXPLODE:
            if (sndist > (6144))
                sndist = 6144;
            if (g_player[screenpeek].ps->cursectnum > -1 && sector[g_player[screenpeek].ps->cursectnum].lotag == 2)
                pitch -= 1024;
            break;
        default:
#endif
    if (cursectnum > -1 && sector[cursectnum].lotag == 2 && (snd.m & SF_TALK) == 0)
        pitch = -768;
    if (sndist > 31444 && PN(i) != MUSICANDSFX)
        return -1;
//        break;
//    }

    if (snd.num > 0 && PN(i) != MUSICANDSFX)
    {
        if (snd.SoundOwner[0].ow == i) S_StopSound(num);
        else if (snd.num > 1) S_StopSound(num);
//        else if (A_CheckEnemySprite(&sprite[i]) && sprite[i].extra <= 0) S_StopSound(num);
    }

    sndang = 2048 + ca - getangle(cx-pos->x,cy-pos->y);
    sndang &= 2047;

    if (snd.ptr == 0)
    {
        if (S_LoadSound(num) == 0) return 0;
    }
    else
    {
        if (snd.lock < CACHE1D_LOCKED)
            snd.lock = CACHE1D_LOCKED;
        else
            snd.lock++;
    }

    if (snd.m & SF_GLOBAL) sndist = 0;

    if (sndist < ((255-LOUDESTVOLUME)<<6))
        sndist = ((255-LOUDESTVOLUME)<<6);

    if (snd.m & SF_LOOP)
    {
#if 0
        // prevent multiple looping sounds of same type from playing at the same time
        if (snd.num > 0)
            return -1;
#endif
        voice = FX_Play(snd.ptr, snd.soundsiz, 0, -1,
                        pitch, sndist>>6, sndist>>6, 0, snd.pr, snd.volume, num);
    }
    else
    {
        voice = FX_Play3D(snd.ptr, snd.soundsiz, FX_ONESHOT,
                          pitch, sndang>>4, sndist>>6, snd.pr, snd.volume, num);
    }

    if (voice >= FX_Ok)
    {
        snd.SoundOwner[snd.num].ow = i;
        snd.SoundOwner[snd.num].voice = voice;
        snd.num++;
    }
    else
        snd.lock--;

    return voice;
}

void S_PlaySound(int32_t const num)
{
    int32_t pitch,pitche,pitchs,cx;
    int32_t voice;

    if (!SM32_havesound) return;
    if (SoundToggle==0) return;
    if ((unsigned)num >= MAXSOUNDS || !g_sounds[num].filename)
    {
        OSD_Printf("WARNING: invalid sound #%d\n",num);
        return;
    }

    auto & snd = g_sounds[num];

//    if ((snd.m & SF_ADULT) && ud.lockout) return;
    if (FX_VoiceAvailable(snd.pr) == 0) return;

    pitchs = snd.ps;
    pitche = snd.pe;
    cx = klabs(pitche-pitchs);

    if (cx)
    {
        if (pitchs < pitche)
            pitch = pitchs + (wrand()%cx);
        else
            pitch = pitche + (wrand()%cx);
    }
    else
        pitch = pitchs;

    if (snd.ptr == 0)
    {
        if (S_LoadSound(num) == 0) return;
    }
    else
    {
        if (snd.lock < CACHE1D_LOCKED)
            snd.lock = CACHE1D_LOCKED;
        else
            snd.lock++;
    }

    if (snd.m & SF_LOOP)
    {
        voice = FX_Play(snd.ptr, snd.soundsiz, 0, -1,
                        pitch,LOUDESTVOLUME,LOUDESTVOLUME,LOUDESTVOLUME,
                        snd.soundsiz, snd.volume, num);
    }
    else
    {
        voice = FX_Play3D(snd.ptr, snd.soundsiz, FX_ONESHOT,
                          pitch,0,255-LOUDESTVOLUME,snd.pr, snd.volume, num);
    }

    if (voice >= FX_Ok)
    {
        snd.SoundOwner[snd.num].ow = -1;
        snd.SoundOwner[snd.num].voice = voice;
        snd.num++;
        return;
    }
    snd.lock--;
}

int32_t A_PlaySound(uint32_t num, int32_t i)
{
    if (num >= MAXSOUNDS) return -1;
    if (i < 0)
    {
        S_PlaySound(num);
        return 0;
    }

    return S_PlaySound3D(num,i, &sprite[i].xyz);
}

void S_StopSound(int32_t num)
{
    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0)
        {
            FX_StopSound(g_sounds[num].SoundOwner[g_sounds[num].num-1].voice);
            S_Callback(num);
        }
}

void S_StopEnvSound(int32_t num,int32_t i)
{
    int32_t j, k;

    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0)
        {
            k = g_sounds[num].num;
            for (j=0; j<k; j++)
                if (g_sounds[num].SoundOwner[j].ow == i)
                {
                    FX_StopSound(g_sounds[num].SoundOwner[j].voice);
                    break;
                }
        }
}

// Do not remove this or make it inline.
void S_StopAllSounds(void)
{
    FX_StopAllSounds();
}

void S_Update(void)
{
    int32_t sndist, sx, sy, sz, cx, cy, cz;
    int32_t sndang,ca,j,k,i,cs;

    g_numEnvSoundsPlaying = 0;

    cx = pos.x;
    cy = pos.y;
    cz = pos.z;
    cs = cursectnum;
    ca = ang;

    for (j=0; j<MAXSOUNDS; j++)
        for (k=0; k<g_sounds[j].num; k++)
        {
            i = g_sounds[j].SoundOwner[k].ow;

            sx = sprite[i].x;
            sy = sprite[i].y;
            sz = sprite[i].z;

            sndang = 2048 + ca - getangle(cx-sx,cy-sy);
            sndang &= 2047;
            sndist = FindDistance3D((cx-sx),(cy-sy),(cz-sz));
            if (i >= 0 && (g_sounds[j].m & SF_GLOBAL) == 0 && PN(i) == MUSICANDSFX && SLT(i) < 999 && (sector[SECT(i)].lotag&0xff) < 9)
                sndist = divscale14(sndist,(SHT(i)+1));

            sndist += g_sounds[j].vo;
            if (sndist < 0) sndist = 0;

            if (cs > -1 && sndist && PN(i) != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,sx,sy,sz-(24<<8),SECT(i)))
                sndist += sndist>>5;

            if (PN(i) == MUSICANDSFX && SLT(i) < 999)
                g_numEnvSoundsPlaying++;
            /*
                        switch (j)
                        {
                        case PIPEBOMB_EXPLODE:
                        case LASERTRIP_EXPLODE:
                        case RPG_EXPLODE:
                            if (sndist > (6144)) sndist = (6144);
                            break;
                        default:
            */
            if (sndist > 31444 && PN(i) != MUSICANDSFX)
            {
                S_StopSound(j);
                continue;
            }
//            }

            if (g_sounds[j].ptr == 0 && S_LoadSound(j) == 0) continue;
            if (g_sounds[j].m & SF_GLOBAL) sndist = 0;

            if (sndist < ((255-LOUDESTVOLUME)<<6))
                sndist = ((255-LOUDESTVOLUME)<<6);

            FX_Pan3D(g_sounds[j].SoundOwner[k].voice,sndang>>4,sndist>>6);
        }
}

void S_Callback(intptr_t num)
{
    int32_t& sndcnt = g_sounds[num].num;
    if (sndcnt > 0)
    {
        // if it's a global sound, simply decrement by one
        if (g_sounds[num].m & SF_GLOBAL)
            sndcnt--;
        else
        {
            // cleanup all actor sounds in one go
            extern uint8_t g_ambiencePlaying[(MAXSPRITES+7)>>3];
            for (int idx = 0; idx < sndcnt;)
            {
                int32_t spriteNum = g_sounds[num].SoundOwner[idx].ow;
                if (spriteNum < 0 || spriteNum >= MAXSPRITES || !bitmap_test(g_ambiencePlaying, spriteNum))
                {
                    if (idx != --sndcnt)
                    {
                        g_sounds[num].SoundOwner[idx].voice = g_sounds[num].SoundOwner[sndcnt].voice;
                        g_sounds[num].SoundOwner[idx].ow = g_sounds[num].SoundOwner[sndcnt].ow;
                    }
                    g_sounds[num].SoundOwner[sndcnt].ow = -1;
                }
                else idx++;
            }
        }
    }

    // sanity check, restore k to 0 if something went wrong
    if (sndcnt < 0)
    {
        LOG_F(WARNING, "Negative number of active sounds %d for soundnum %ld detected! This shouldn't be happening!", sndcnt, num);
        sndcnt = 0;
    }

    // only release sound lock if none are playing
    if (sndcnt == 0)
        g_sounds[num].lock--;
}

void S_ClearSoundLocks(void)
{
    int32_t i;

    for (i=0; i<MAXSOUNDS; i++)
        if (g_sounds[i].lock >= CACHE1D_LOCKED)
            g_sounds[i].lock = CACHE1D_UNLOCKED;
}

int32_t A_CheckSoundPlaying(int32_t i, int32_t num)
{
    UNREFERENCED_PARAMETER(i);
    if (num < 0) num=0;	// FIXME
    return (g_sounds[num].num > 0);
}

int32_t S_CheckSoundPlaying(int32_t i, int32_t num)
{
    if (i == -1)
    {
        if (g_sounds[num].lock >= CACHE1D_LOCKED)
            return 1;
        return 0;
    }
    return g_sounds[num].num;
}

int32_t S_SoundsPlaying(int32_t i)
{
    int32_t j = MAXSOUNDS-1;
    for (; j>=0; j--)
        if (g_sounds[j].SoundOwner[0].ow == i)
            break;

    return j;
}

int32_t S_InvalidSound(int32_t num)
{
    return (unsigned) num >= MAXSOUNDS;
}

int32_t S_SoundFlags(int32_t num)
{
    return g_sounds[num].m;
}
