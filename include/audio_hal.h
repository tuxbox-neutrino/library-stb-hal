/*
 * (C) 2010-2013 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __audio_hal__
#define __audio_hal__

#include <stdint.h>
#include <cs_types.h>

typedef enum
{
  AUDIO_SYNC_WITH_PTS,
  AUDIO_NO_SYNC,
  AUDIO_SYNC_AUDIO_MASTER
} AUDIO_SYNC_MODE;

typedef enum {
	HDMI_ENCODED_OFF,
	HDMI_ENCODED_AUTO,
	HDMI_ENCODED_FORCED
} HDMI_ENCODED_MODE;

typedef enum
{
   AUDIO_FMT_AUTO = 0,
   AUDIO_FMT_MPEG,
   AUDIO_FMT_MP3,
   AUDIO_FMT_DOLBY_DIGITAL,
   AUDIO_FMT_BASIC = AUDIO_FMT_DOLBY_DIGITAL,
   AUDIO_FMT_AAC,
   AUDIO_FMT_AAC_PLUS,
   AUDIO_FMT_DD_PLUS,
   AUDIO_FMT_DTS,
   AUDIO_FMT_AVS,
   AUDIO_FMT_MLP,
   AUDIO_FMT_WMA,
   AUDIO_FMT_MPG1, // TD only. For Movieplayer / cPlayback
   AUDIO_FMT_ADVANCED = AUDIO_FMT_MLP
} AUDIO_FORMAT;

class cAudio
{
	public:
		/* construct & destruct */
		cAudio(void *, void *, void *);
		~cAudio(void);

		void *GetHandle() { return NULL; };
		/* shut up */
		int mute(void);
		int unmute(void);
		int SetMute(bool enable);

		/* volume, min = 0, max = 255 */
		int setVolume(unsigned int left, unsigned int right);
		int getVolume(void) { return volume;}
		bool getMuteStatus(void) { return muted; };

		/* start and stop audio */
		int Start(void);
		int Stop(void);
		bool Pause(bool Pcm = true);
		void SetStreamType(AUDIO_FORMAT type);
		void SetSyncMode(AVSYNC_TYPE Mode);

		/* select channels */
		int setChannel(int channel);
		int PrepareClipPlay(int uNoOfChannels, int uSampleRate, int uBitsPerSample, int bLittleEndian);
		int WriteClip(unsigned char * buffer, int size);
		int StopClip();
		void getAudioInfo(int &type, int &layer, int& freq, int &bitrate, int &mode);
		void SetSRS(int iq_enable, int nmgr_enable, int iq_mode, int iq_level);
		bool IsHdmiDDSupported();
		void SetHdmiDD(bool enable);
		void SetSpdifDD(bool enable);
		void ScheduleMute(bool On);
		void EnableAnalogOut(bool enable);
	private:
		bool muted;
		int volume;
		void *pdata;
};

#endif
