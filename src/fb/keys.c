/* 
 * Copyright (C) 2000-2003 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>

#include "main.h"
#include "actions.h"

#ifdef HAVE_LINUX_KD_H
#include <linux/kd.h>
#endif

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif

#include "keys.h"

static int default_key_action(int key)
{
	switch(key)
	{
		case KEY_PPAGE: return ACTID_SEEK_REL_p600;
		case KEY_NPAGE: return ACTID_SEEK_REL_m600;
		case KEY_UP:    return ACTID_SEEK_REL_p60;
		case KEY_DOWN:  return ACTID_SEEK_REL_m60;
		case KEY_RIGHT: return ACTID_SEEK_REL_p10;
		case KEY_LEFT:  return ACTID_SEEK_REL_m10;

		case KEY_ENTER: return ACTID_PLAY;

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
	}
	
	return 0;
}

static void *fbxine_keyboard_loop(void *dummy)
{
	pthread_detach(pthread_self());

	for(;;)
		do_action(default_key_action(getch()));
	
	return 0;
}

static void exit_keyboard(void)
{
	pthread_cancel(fbxine.keyboard_thread);

	endwin();

#ifdef HAVE_LINUX_KD_H
	if(ioctl(fbxine.tty_fd, KDSETMODE, KD_TEXT) == -1)
		perror("Failed to set /dev/tty to text mode");
#endif
	
	close(fbxine.tty_fd);
}

int fbxine_init_keyboard(void)
{
	static struct fbxine_callback exit_callback;
	
	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_keyboard);
	fbxine_register_abort(&exit_callback,(fbxine_callback_t)exit_keyboard);
	
	initscr();
	keypad(stdscr, TRUE);
	cbreak();
	noecho();
	intrflush(stdscr, FALSE);
	meta(stdscr, TRUE);

	fbxine.tty_fd = open("/dev/tty", O_RDWR);
	if(fbxine.tty_fd == -1)
        {
		perror("Failed to open /dev/tty");
		return 0;
	}

#ifdef HAVE_LINUX_KD_H
	if(ioctl(fbxine.tty_fd, KDSETMODE, KD_GRAPHICS) == -1)
	{
		perror("Failed to set /dev/tty to graphics mode");
		return 0;
        }
#endif
	pthread_create(&fbxine.keyboard_thread, 0, fbxine_keyboard_loop, 0);
	return 1;
}
