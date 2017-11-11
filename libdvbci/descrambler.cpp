#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <linux/dvb/ca.h>

#include "misc.h"
#include "descrambler.h"

#include <config.h>

static const char * FILENAME = "[descrambler]";

static int desc_fd = -1;
static int desc_user_count = 0;

#if HAVE_ARM_HARDWARE

//static const char *descrambler_filename = "/dev/dvb/adapter0/ca0";
//static const char *descrambler_filename = "/dev/dvb/adapter0/ca1";
static const char *descrambler_filename = "/dev/ciplus_ca0";

enum ca_descr_data_type {
	CA_DATA_IV,
	CA_DATA_KEY,
};

enum ca_descr_parity {
	CA_PARITY_EVEN,
	CA_PARITY_ODD,
};

struct ca_descr_data {
	unsigned int index;
	enum ca_descr_parity parity;
	enum ca_descr_data_type data_type;
	unsigned int length;
	unsigned char *data;
};

#define CA_SET_DESCR_DATA _IOW('o', 137, struct ca_descr_data)
//#define CA_SET_DESCR_DATA _IOW('o', 10, struct ca_descr_data)

int descrambler_set_key(int index, int parity, unsigned char *data)
{
	struct ca_descr_data d;
	int ret;

	printf("%s -> %s %s\n", FILENAME, __FUNCTION__, descrambler_filename);

	if (descrambler_open())
	{
		printf("Complete Data-> Index: (%d) Parity: (%d) -> ", index, parity);
		hexdump(data, 32);

		d.index = index;
		d.parity = (ca_descr_parity)parity;
		d.data_type = CA_DATA_KEY;
		d.length = 16;
		d.data = data;

		printf("AES Index: (%d) Parity: (%d) -> ", d.index, d.parity);
		hexdump(d.data, 16);

		ret = ioctl(desc_fd, CA_SET_DESCR_DATA, &d);
		if (ret)
		{
			printf("CA_SET_DESCR_DATA (AES) index=%d parity=%d (errno=%d %s)\n", index, parity, errno, strerror(errno));
		}

		d.index = index;
		d.parity = (ca_descr_parity)parity;
		d.data_type = CA_DATA_IV;
		d.length = 16;
		d.data = data + 16;

		printf("IV Index: (%d) Parity: (%d) -> ", d.index, d.parity);
		hexdump(d.data, 16);

		ret = ioctl(desc_fd, CA_SET_DESCR_DATA, &d);
		if (ret)
		{
			printf("CA_SET_DESCR_DATA (IV) index=%d parity=%d (errno=%d %s)\n", index, parity, errno, strerror(errno));
		}

		descrambler_close();
	}
	return 0;
}

#else

static const char *descrambler_filename = "/dev/dvb/adapter0/ca3";

/* Byte 0 to 15 are AES Key, Byte 16 to 31 are IV */

int descrambler_set_key(int index, int parity, unsigned char *data)
{
	struct ca_descr_data d;

	printf("%s -> %s %s\n", FILENAME, __FUNCTION__, descrambler_filename);

	index |= 0x100;

	if (descrambler_open())
	{
		d.index = index;
		d.parity = (ca_descr_parity)parity;
		d.data_type = CA_DATA_KEY;
		d.length = 32;
		d.data = data;
#if 0
		printf("Index: %d Parity: (%d) -> ", d.index, d.parity);
		hexdump(d.data, 32);
#endif
		if (ioctl(desc_fd, CA_SET_DESCR_DATA, &d))
		{
			printf("CA_SET_DESCR_DATA index=%d parity=%d (errno=%d %s)\n", index, parity, errno, strerror(errno));
		}

		printf("Index: %d Parity: (%d) -> ", d.index, d.parity);
		hexdump(d.data, 32);

		descrambler_close();
	}
	return 0;
}
#endif

/* we don't use this for sh4 ci cam ! */

int descrambler_set_pid(int index, int enable, int pid)
{
	struct ca_pid p;
#if HAVE_ARM_HARDWARE
	unsigned int flags = 0x80;

	if (index)
		flags |= 0x40;

	if (enable)
		flags |= 0x20;

	p.index = flags;
	p.pid = pid;
#else
	p.index = index;
	if (enable)
		p.pid = pid;
	else
		p.pid = -1;
#endif

	printf("CA_SET_PID pid=0x%04x index=0x%04x\n", p.pid, p.index);
	if (ioctl(desc_fd, CA_SET_PID, &p) == -1)
		printf("CA_SET_PID pid=0x%04x index=0x%04x (errno=%d %s)\n", p.pid, p.index, errno, strerror(errno));

	return 0;
}


bool descrambler_open(void)
{
	desc_fd = open(descrambler_filename, O_RDWR | O_NONBLOCK );
	if (desc_fd <= 0) {
		printf("cannot open %s\n", descrambler_filename);
		return false;
	}
	return true;
}

int descrambler_init(void)
{
	desc_user_count++;
	printf("%s -> %s %d\n", FILENAME, __FUNCTION__, desc_user_count);
	return 0;
}

void descrambler_close(void)
{
	close(desc_fd);
	desc_fd = -1;
}

void descrambler_deinit(void)
{
	desc_user_count--;
	if (desc_user_count <= 0 && desc_fd > 0)
		descrambler_close();
}