/*
 * retrospy.c Version 1.0
 *
 * Copyright (c) 2020 RetroSpy Technologies
 *
 * Based on jstest.c Version 1.2
 */

 /*
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
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  * 02110-1301 USA.
  */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <linux/input.h>
#include <linux/joystick.h>

#include "axbtnmap.h"

char *axis_names[ABS_MAX + 1] = {
"X", "Y", "Z", "Rx", "Ry", "Rz", "Throttle", "Rudder",
"Wheel", "Gas", "Brake", "?", "?", "?", "?", "?",
"Hat0X", "Hat0Y", "Hat1X", "Hat1Y", "Hat2X", "Hat2Y", "Hat3X", "Hat3Y",
"?", "?", "?", "?", "?", "?", "?",
};

char *button_names[KEY_MAX - BTN_MISC + 1] = {
"Btn0", "Btn1", "Btn2", "Btn3", "Btn4", "Btn5", "Btn6", "Btn7", "Btn8", "Btn9",
"?", "?", "?", "?", "?", "?","LeftBtn", "RightBtn", "MiddleBtn", "SideBtn", 
"ExtraBtn", "ForwardBtn", "BackBtn", "TaskBtn", "?", "?", "?", "?", "?", "?", 
"?", "?", "Trigger", "ThumbBtn", "ThumbBtn2", "TopBtn", "TopBtn2", "PinkieBtn",
"BaseBtn", "BaseBtn2", "BaseBtn3", "BaseBtn4", "BaseBtn5", "BaseBtn6", 
"BtnDead", "BtnA", "BtnB", "BtnC", "BtnX", "BtnY", "BtnZ", "BtnTL", "BtnTR", 
"BtnTL2", "BtnTR2", "BtnSelect", "BtnStart", "BtnMode", "BtnThumbL", 
"BtnThumbR", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
"?", "?", "?", "?", "WheelBtn", "Gear up",
};

#define NAME_LENGTH 128

int main(int argc, char **argv)
{
	int fd, i;
	unsigned char axes = 2;
	unsigned char buttons = 2;
	int version = 0x000800;
	char name[NAME_LENGTH] = "Unknown";
	uint16_t btnmap[BTNMAP_SIZE];
	uint8_t axmap[AXMAP_SIZE];
	int btnmapok = 1;

	if ((fd = open(argv[argc - 1], O_NONBLOCK | O_RDONLY)) < 0) {
		perror("jstest");
		return 1;
	}

	ioctl(fd, JSIOCGVERSION, &version);
	ioctl(fd, JSIOCGAXES, &axes);
	ioctl(fd, JSIOCGBUTTONS, &buttons);
	ioctl(fd, JSIOCGNAME(NAME_LENGTH), name);

	getaxmap(fd, axmap);
	getbtnmap(fd, btnmap);

	printf("Driver version is %d.%d.%d.\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	/* Determine whether the button map is usable. */
	for (i = 0; btnmapok && i < buttons; i++) {
		if (btnmap[i] < BTN_MISC || btnmap[i] > KEY_MAX) {
			btnmapok = 0;
			break;
		}
	}
	if (!btnmapok) {
		/* btnmap out of range for names. Don't print any. */
		puts("jstest is not fully compatible with your kernel. Unable to retrieve button map!");
		printf("Joystick (%s) has %d axes ", name, axes);
		printf("and %d buttons.\n", buttons);
	}
	else {
		printf("Joystick (%s) has %d axes (", name, axes);
		for (i = 0; i < axes; i++)
			printf("%s%s", i > 0 ? ", " : "", axis_names[axmap[i]]);
		puts(")");

		printf("and %d buttons (", buttons);
		for (i = 0; i < buttons; i++) {
			printf("%s%s", i > 0 ? ", " : "", button_names[btnmap[i] - BTN_MISC]);
		}
		puts(").");
	}

	struct js_event js;

	// 8 bits each for num axes and buttons
	// 1 bit for each button state
	// 32 bits for each axis
	int bufSize = sizeof(char) * (8 + 8 + buttons + axes * 32);

	char *outputBuf = calloc(sizeof(char), bufSize + 1);
	memset(outputBuf, '0', bufSize);

	char *currPos = outputBuf;
	// encode num axes and buttons in one byte each, LSB first
	for (i = 0; i < 8; ++currPos, ++i) {
		*currPos = '0' + ((axes & (1 << i)) != 0);
	}

	for (i = 0; i < 8; ++currPos, ++i) {
		*currPos = '0' + ((buttons & (1 << i)) != 0);
	}

	printf("%s\n", outputBuf);

	currPos = NULL;

	char *buttonsPtr = outputBuf + 8 + 8;
	char *axesPtr = buttonsPtr + buttons;

	while (1) {
		// calculate current state by emptying event queue every 1ms
		usleep(1000);
		while (read(fd, &js, sizeof(struct js_event)) > 0) {
			switch (js.type & ~JS_EVENT_INIT) {
			case JS_EVENT_BUTTON:
				buttonsPtr[js.number] = js.value != 0 ? '1' : '0';
				break;
			case JS_EVENT_AXIS:
				for (int i = 0; i < sizeof(int) * 8; ++i) {
					axesPtr[js.number * 32 + i] = ((js.value & (1 << i)) != 0) ? '1' : '0';
				}
				break;
			}

			if (0 != errno && EAGAIN != errno) {
				perror("jstest");
				return errno;
			}
		}

		printf("%s\n", outputBuf);
	}
}