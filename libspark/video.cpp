/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 * (C) 2010-2011 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Suite 500 Boston, MA 02110-1335 USA
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#include <linux/dvb/video.h>
#include "video_lib.h"
#define VIDEO_DEVICE "/dev/dvb/adapter0/video0"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)

#define fop(cmd, args...) ({				\
	int _r;						\
	if (fd >= 0) { 					\
		if ((_r = ::cmd(fd, args)) < 0)		\
			lt_info(#cmd"(fd, "#args")\n");	\
		else					\
			lt_debug(#cmd"(fd, "#args")\n");\
	}						\
	else { _r = fd; } 				\
	_r;						\
})

cVideo * videoDecoder = NULL;
int system_rev = 0;

#define VIDEO_STREAMTYPE_MPEG2 0
#define VIDEO_STREAMTYPE_MPEG4_H264 1
#define VIDEO_STREAMTYPE_VC1 3
#define VIDEO_STREAMTYPE_MPEG4_Part2 4
#define VIDEO_STREAMTYPE_VC1_SM 5
#define VIDEO_STREAMTYPE_MPEG1 6


static int proc_put(const char *path, const char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_WRONLY);
	if (pfd < 0)
		return pfd;
	ret = write(pfd, value, len);
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

static int proc_get(const char *path, char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_RDONLY);
	if (pfd < 0)
		return pfd;
	ret = read(pfd, value, len);
	value[len-1] = '\0'; /* make sure string is terminated */
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

static unsigned int proc_get_hex(const char *path)
{
	unsigned int n, ret = 0;
	char buf[16];
	n = proc_get(path, buf, 16);
	if (n > 0)
		sscanf(buf, "%x", &ret);
	return ret;
}

cVideo::cVideo(int, void *, void *)
{
	lt_debug("%s\n", __FUNCTION__);

	openDevice();
	//croppingMode = VID_DISPMODE_NORM;
	//outputformat = VID_OUTFMT_RGBC_SVIDEO;
	scartvoltage = -1;
	video_standby = 0;
}

cVideo::~cVideo(void)
{
	/* disable DACs and SCART voltage */
	Standby(true);
	closeDevice();
}

void cVideo::openDevice(void)
{
	if ((fd = open(VIDEO_DEVICE, O_RDWR)) < 0)
		lt_info("%s cannot open %s: %m\n", __FUNCTION__, VIDEO_DEVICE);
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	playstate = VIDEO_STOPPED;
}

void cVideo::closeDevice(void)
{
	if (fd >= 0)
		close(fd);
	fd = -1;
	playstate = VIDEO_STOPPED;
}

int cVideo::setAspectRatio(int aspect, int mode)
{
	static const char *a[] = { "n/a", "4:3", "14:9", "16:9" };
	static const char *m[] = { "panscan", "letterbox", "bestfit", "nonlinear", "(unset)" };
	int n;
	lt_debug("%s: a:%d m:%d  %s\n", __func__, aspect, mode, m[(mode < 0||mode > 3) ? 4 : mode]);

	if (aspect > 3 || aspect == 0)
		lt_info("%s: invalid aspect: %d\n", __func__, aspect);
	else if (aspect > 0) /* -1 == don't set */
	{
		lt_debug("%s: /proc/stb/video/aspect -> %s\n", __func__, a[aspect]);
		n = proc_put("/proc/stb/video/aspect", a[aspect], strlen(a[aspect]));
		if (n < 0)
			lt_info("%s: proc_put /proc/stb/video/aspect (%m)\n", __func__);
	}

	if (mode == -1)
		return 0;

	lt_debug("%s: /proc/stb/video/policy -> %s\n", __func__, m[mode]);
	n = proc_put("/proc/stb/video/policy", m[mode], strlen(m[mode]));
	if (n < 0)
		return 1;
	return 0;
}

int cVideo::getAspectRatio(void)
{
	int ratio = 0; /* proc: 0 = 4:3, 1 = 16:9 */
	ratio = proc_get_hex("/proc/stb/vmpeg/0/aspect");
	if (ratio >= 0)
		return ratio * 2 + 1; /* return: 1 = 4:3, 3 = 16:9 */
	return ratio;
}

int cVideo::setCroppingMode(int /*vidDispMode_t format*/)
{
	return 0;
#if 0
	croppingMode = format;
	const char *format_string[] = { "norm", "letterbox", "unknown", "mode_1_2", "mode_1_4", "mode_2x", "scale", "disexp" };
	const char *f;
	if (format >= VID_DISPMODE_NORM && format <= VID_DISPMODE_DISEXP)
		f = format_string[format];
	else
		f = "ILLEGAL format!";
	lt_debug("%s(%d) => %s\n", __FUNCTION__, format, f);
	return fop(ioctl, MPEG_VID_SET_DISPMODE, format);
#endif
}

int cVideo::Start(void * /*PcrChannel*/, unsigned short /*PcrPid*/, unsigned short /*VideoPid*/, void * /*hChannel*/)
{
	lt_debug("%s playstate=%d\n", __FUNCTION__, playstate);
#if 0
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
#endif
	playstate = VIDEO_PLAYING;
	fop(ioctl, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	return fop(ioctl, VIDEO_PLAY);
}

int cVideo::Stop(bool blank)
{
	lt_debug("%s(%d)\n", __FUNCTION__, blank);
	playstate = blank ? VIDEO_STOPPED : VIDEO_FREEZED;
	return fop(ioctl, VIDEO_STOP, blank ? 1 : 0);
}

int cVideo::setBlank(int)
{
	return Stop(1);
}

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	lt_info("%s(%d, %d)\n", __func__, video_system, remember);
	return 0;
#if 0
	if (video_system > VID_DISPFMT_SECAM || video_system < 0)
		video_system = VID_DISPFMT_PAL;
        return fop(ioctl, MPEG_VID_SET_DISPFMT, video_system);
#endif
}

int cVideo::getPlayState(void)
{
	return playstate;
}

void cVideo::SetVideoMode(analog_mode_t mode)
{
	lt_debug("%s(%d)\n", __FUNCTION__, mode);
#if 0
	switch(mode)
	{
		case ANALOG_SD_YPRPB_SCART:
			outputformat = VID_OUTFMT_YBR_SVIDEO;
			break;
		case ANALOG_SD_RGB_SCART:
			outputformat = VID_OUTFMT_RGBC_SVIDEO;
			break;
		default:
			lt_info("%s unknown mode %d\n", __FUNCTION__, mode);
			return;
	}
	fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
#endif
}

void cVideo::ShowPicture(const char * fname)
{
	return;
#if 0
	lt_debug("%s(%s)\n", __FUNCTION__, fname);
	char destname[512];
	char cmd[512];
	char *p;
	void *data;
	int mfd;
	struct stat st;
	strcpy(destname, "/var/cache");
	mkdir(destname, 0755);
	/* the cache filename is (example for /share/tuxbox/neutrino/icons/radiomode.jpg):
	   /var/cache/share.tuxbox.neutrino.icons.radiomode.jpg.m2v
	   build that filename first...
	   TODO: this could cause name clashes, use a hashing function instead... */
	strcat(destname, fname);
	p = &destname[strlen("/var/cache/")];
	while ((p = strchr(p, '/')) != NULL)
		*p = '.';
	strcat(destname, ".m2v");
	/* ...then check if it exists already...
	   TODO: check if the cache file is older than the jpeg file... */
	if (access(destname, R_OK))
	{
		/* it does not exist, so call ffmpeg to create it... */
		sprintf(cmd, "ffmpeg -y -f mjpeg -i '%s' -s 704x576 '%s' </dev/null",
							fname, destname);
		system(cmd); /* TODO: use libavcodec to directly convert it */
	}
	/* the mutex is a workaround: setBlank is apparently called from
	   a differnt thread and takes slightly longer, so that the decoder
	   was blanked immediately after displaying the image, which is not
	   what we want. the mutex ensures proper ordering. */
	pthread_mutex_lock(&stillp_mutex);
	mfd = open(destname, O_RDONLY);
	if (mfd < 0)
	{
		lt_info("%s cannot open %s: %m", __FUNCTION__, destname);
		goto out;
	}
	if (fstat(mfd, &st) != -1 && st.st_size > 0)
	{
		data = malloc(st.st_size);
		if (! data)
			lt_info("%s malloc failed (%m)\n", __FUNCTION__);
		else if (read(mfd, data, st.st_size) != st.st_size)
			lt_info("%s short read (%m)\n", __FUNCTION__);
		else
		{
			BUFINFO buf;
			buf.ulLen = st.st_size;
			buf.ulStartAdrOff = (int)data;
			Stop(false);
			fop(ioctl, MPEG_VID_STILLP_WRITE, &buf);
		}
		free(data);
	}
	close(mfd);
 out:
	pthread_mutex_unlock(&stillp_mutex);
	return;
#endif
#if 0
	/* DirectFB based picviewer: works, but is slow and the infobar
	   draws in the same plane */
	int width;
	int height;
	if (!fname)
		return;

	IDirectFBImageProvider *provider;
	DFBResult err = dfb->CreateImageProvider(dfb, fname, &provider);
	if (err)
	{
		fprintf(stderr, "cVideo::ShowPicture: CreateImageProvider error!\n");
		return;
	}

	DFBSurfaceDescription desc;
	provider->GetSurfaceDescription (provider, &desc);
	width = desc.width;
	height = desc.height;
	provider->RenderTo(provider, dfbdest, NULL);
	provider->Release(provider);
#endif
}

void cVideo::StopPicture()
{
#if 0
	lt_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_SELECT_SOURCE, VID_SOURCE_DEMUX);
#endif
}

void cVideo::Standby(unsigned int bOn)
{
#if 0
	lt_debug("%s(%d)\n", __FUNCTION__, bOn);
	if (bOn)
	{
		setBlank(1);
		fop(ioctl, MPEG_VID_SET_OUTFMT, VID_OUTFMT_DISABLE_DACS);
	} else
		fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
	routeVideo(bOn);
	video_standby = bOn;
#endif
}

int cVideo::getBlank(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return 0;
}

/* this function is regularly called, checks if video parameters
   changed and triggers appropriate actions */
void cVideo::VideoParamWatchdog(void)
{
#if 0
	static unsigned int _v_info = (unsigned int) -1;
	unsigned int v_info;
	if (fd == -1)
		return;
	ioctl(fd, MPEG_VID_GET_V_INFO_RAW, &v_info);
	if (_v_info != v_info)
	{
		lt_debug("%s params changed. old: %08x new: %08x\n", __FUNCTION__, _v_info, v_info);
		setAspectRatio(-1, -1);
	}
	_v_info = v_info;
#endif
}

void cVideo::Pig(int x, int y, int w, int h, int osd_w, int osd_h)
{
	char buffer[16];
	int _x, _y, _w, _h;
	/* the target "coordinates" seem to be in a PAL sized plane
	 * TODO: check this in the driver sources */
	int xres = 720; /* proc_get_hex("/proc/stb/vmpeg/0/xres") */
	int yres = 576; /* proc_get_hex("/proc/stb/vmpeg/0/yres") */
	lt_debug("%s: x:%d y:%d w:%d h:%d ow:%d oh:%d\n", __func__, x, y, w, h, osd_w, osd_h);
	if (x == -1 && y == -1 && w == -1 && h == -1)
	{
		_w = xres;
		_h = yres;
		_x = 0;
		_y = 0;
	}
	else
	{
		_x = x * xres / osd_w;
		_w = w * xres / osd_w;
		_y = y * yres / osd_h;
		_h = h * yres / osd_h;
	}
	lt_debug("%s: x:%d y:%d w:%d h:%d xr:%d yr:%d\n", __func__, _x, _y, _w, _h, xres, yres);
	sprintf(buffer, "%x", _x);
	proc_put("/proc/stb/vmpeg/0/dst_left", buffer, strlen(buffer));
	sprintf(buffer, "%x", _y);
	proc_put("/proc/stb/vmpeg/0/dst_top", buffer, strlen(buffer));
	sprintf(buffer, "%x", _w);
	proc_put("/proc/stb/vmpeg/0/dst_width", buffer, strlen(buffer));
	sprintf(buffer, "%x", _h);
	proc_put("/proc/stb/vmpeg/0/dst_height", buffer, strlen(buffer));
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	rate = proc_get_hex("/proc/stb/vmpeg/0/framerate");
	rate /= 1000;
	width = proc_get_hex("/proc/stb/vmpeg/0/xres");
	height = proc_get_hex("/proc/stb/vmpeg/0/yres");
}

void cVideo::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __FUNCTION__, Mode);
	/*
	 * { 0, LOCALE_OPTIONS_OFF },
	 * { 1, LOCALE_OPTIONS_ON  },
	 * { 2, LOCALE_AUDIOMENU_AVSYNC_AM }
	 */
#if 0
	switch(Mode)
	{
		case 0:
			ioctl(fd, MPEG_VID_SYNC_OFF);
			break;
		case 1:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_VID);
			break;
		default:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_AUD);
			break;
	}
#endif
};

int cVideo::SetStreamType(VIDEO_FORMAT type)
{
	static const char *VF[] = {
		"VIDEO_FORMAT_MPEG2",
		"VIDEO_FORMAT_MPEG4",
		"VIDEO_FORMAT_VC1",
		"VIDEO_FORMAT_JPEG",
		"VIDEO_FORMAT_GIF",
		"VIDEO_FORMAT_PNG"
	};
	int t;
	lt_debug("%s type=%s\n", __FUNCTION__, VF[type]);

	switch (type)
	{
		case VIDEO_FORMAT_MPEG4:
			t = VIDEO_STREAMTYPE_MPEG4_H264;
			break;
		case VIDEO_FORMAT_VC1:
			t = VIDEO_STREAMTYPE_VC1;
			break;
		case VIDEO_FORMAT_MPEG2:
		default:
			t = VIDEO_STREAMTYPE_MPEG2;
			break;
	}

	if (ioctl(fd, VIDEO_SET_STREAMTYPE, t) < 0)
		lt_info("%s VIDEO_SET_STREAMTYPE(%d) failed: %m\n", __func__, t);
	return 0;
}

void cVideo::routeVideo(int standby)
{
#if 0
	lt_debug("%s(%d)\n", __FUNCTION__, standby);

	int avsfd = open("/dev/stb/tdsystem", O_RDONLY);
	if (avsfd < 0)
	{
		perror("open tdsystem");
		return;
	}

	/* in standby, we always route VCR scart to the TV. Once there is a UI
	   to configure this, we can think more about this... */
	if (standby)
	{
		lt_info("%s set fastblank and pin8 to follow VCR SCART, route VCR to TV\n", __FUNCTION__);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, (unsigned char)3) < 0)
			perror("IOC_AVS_FASTBLANK_SET, 3");
		/* TODO: should probably depend on aspect ratio setting */
		if (ioctl(avsfd, IOC_AVS_SCART_PIN8_FOLLOW_VCR) < 0)
			perror("IOC_AVS_SCART_PIN8_FOLLOW_VCR");
		if (ioctl(avsfd, IOC_AVS_ROUTE_VCR2TV) < 0)
			perror("IOC_AVS_ROUTE_VCR2TV");
	} else {
		unsigned char fblk = 1;
		lt_info("%s set fastblank=%d pin8=%dV, route encoder to TV\n", __FUNCTION__, fblk, scartvoltage);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, fblk) < 0)
			perror("IOC_AVS_FASTBLANK_SET, fblk");
		if (!noscart && ioctl(avsfd, IOC_AVS_SCART_PIN8_SET, scartvoltage) < 0)
			perror("IOC_AVS_SCART_PIN8_SET");
		if (ioctl(avsfd, IOC_AVS_ROUTE_ENC2TV) < 0)
			perror("IOC_AVS_ROUTE_ENC2TV");
	}
	close(avsfd);
#endif
}

void cVideo::FastForwardMode(int mode)
{
#if 0
	lt_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_FASTFORWARD, mode);
#endif
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	if (ioctl(fd, VIDEO_GET_PTS, &pts) < 0)
		lt_info("%s: GET_PTS failed (%m)\n", __func__);
	return pts;
}