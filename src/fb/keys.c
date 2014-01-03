/* 
 * Copyright (C) 2000-2011 the xine project
 * 
 * This file is part of xine, a unix video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "main.h"
#include "actions.h"

#ifdef HAVE_LINUX_KD_H
#include <linux/kd.h>
#endif

#include "keys.h"

#define K_PPAGE ('\033' | ('\133'<<8) | ('\065'<<16) | ('\176'<<24))
#define K_NPAGE ('\033' | ('\133'<<8) | ('\066'<<16) | ('\176'<<24))
#define K_UP    ('\033' | ('\133'<<8) | ('\101'<<16))
#define K_DOWN  ('\033' | ('\133'<<8) | ('\102'<<16))
#define K_RIGHT ('\033' | ('\133'<<8) | ('\103'<<16))
#define K_LEFT  ('\033' | ('\133'<<8) | ('\104'<<16))
#define K_ENTER ('\015')
#define K_CTRLZ ('\032')

static int do_getc(void)
{
	unsigned char c[4];
	int           n, i, k;
	
	n = read(fbxine.tty_fd, c, 4 );
	for (k = 0, i = 0; i < n; i++)
		k |= c[i] << (i << 3);

	return k;
}

static int default_key_action(int key)
{
	switch(key)
	{
		case K_PPAGE: return ACTID_SEEK_REL_p600;
		case K_NPAGE: return ACTID_SEEK_REL_m600;
		case K_UP:    return ACTID_SEEK_REL_p60;
		case K_DOWN:  return ACTID_SEEK_REL_m60;
		case K_RIGHT: return ACTID_SEEK_REL_p10;
		case K_LEFT:  return ACTID_SEEK_REL_m10;

		case K_ENTER: return ACTID_PLAY;

		case '\033': /* ESC */
		case 'q': 
		case 'Q': return ACTID_QUIT;

		case 'm': return ACTID_MUTE;

		case 'o': return ACTID_OSD_SINFOS;

		case 'j': return ACTID_SUBSELECT;

		case 'p': 
		case ' ': return ACTID_PAUSE;

		case 's': return ACTID_STOP;

		case '/': return ACTID_mVOLUME;
		case '*': return ACTID_pVOLUME;
			
		case 'K': return ACTID_EVENT_UP;
		case 'J': return ACTID_EVENT_DOWN;
		case 'H': return ACTID_EVENT_LEFT;
		case 'L': return ACTID_EVENT_RIGHT;
		case 'M': return ACTID_EVENT_MENU1;
		case 'S': return ACTID_EVENT_SELECT;
		case 'i': return ACTID_TOGGLE_INTERLEAVE;

		case 'z':     return ACTID_ZOOM_IN;
		case 'Z':     return ACTID_ZOOM_OUT;
		case K_CTRLZ: return ACTID_ZOOM_RESET; /* This is ^Z */
	}
	
	return 0;
}

static __attribute__((noreturn)) void *fbxine_keyboard_loop(void *dummy)
{
	pthread_detach(pthread_self());

	for(;;) {
		pthread_testcancel();
		do_action(default_key_action(do_getc()));
	}

	pthread_exit (NULL);
}

static void exit_keyboard(void)
{
	if (fbxine.keyboard_thread)
		pthread_cancel(fbxine.keyboard_thread);

	tcsetattr(fbxine.tty_fd, TCSANOW, &fbxine.ti_save);

#ifdef HAVE_LINUX_KD_H
	if (ioctl(fbxine.tty_fd, KDSETMODE, KD_TEXT) == -1)
		perror("Failed to set /dev/tty to text mode");
#endif

	close(fbxine.tty_fd);
}

int fbxine_init_keyboard(void)
{
	static struct fbxine_callback exit_callback;
	
	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_keyboard);
	fbxine_register_abort(&exit_callback,(fbxine_callback_t)exit_keyboard);

	if (isatty(STDIN_FILENO))
		fbxine.tty_fd = dup(STDIN_FILENO);
	else
		fbxine.tty_fd = open("/dev/tty", O_RDWR);
	if (fbxine.tty_fd == -1)
	{
		perror("Failed to open /dev/tty");
		return 0;
	}

	fcntl(fbxine.tty_fd, F_SETFD, FD_CLOEXEC);

#ifdef HAVE_LINUX_KD_H
	if (ioctl(fbxine.tty_fd, KDSETMODE, KD_GRAPHICS) == -1)
		perror("Failed to set /dev/tty to graphics mode");
#endif

	tcgetattr(fbxine.tty_fd, &fbxine.ti_save);

	fbxine.ti_cur = fbxine.ti_save;
	fbxine.ti_cur.c_cc[VMIN]  = 1;
	fbxine.ti_cur.c_cc[VTIME] = 0;
	fbxine.ti_cur.c_iflag    &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
							INLCR  | IGNCR  | ICRNL  | IXON);
	fbxine.ti_cur.c_lflag    &= ~(ECHO | ECHONL | ISIG | ICANON);
	
	if (tcsetattr(fbxine.tty_fd, TCSAFLUSH, &fbxine.ti_cur) == -1) 
	{
		perror("Failed to change terminal attributes");
		close(fbxine.tty_fd);
		return 0;
	}

	pthread_create(&fbxine.keyboard_thread, 0, fbxine_keyboard_loop, 0);
	return 1;
}
