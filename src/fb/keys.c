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

#include "main.h"

#ifdef HAVE_LINUX_KD_H
#include <linux/kd.h>
#endif

#include "keys.h"

static kbinding_entry_t default_binding_table[] =
{
	{ "Reduce the output window size by factor 1.2.",
	  "WindowReduce", ACTID_WINDOWREDUCE, "less", KEYMOD_NOMOD, 0 },
	{ "set video output window to 50%",
	  "Window50", ACTID_WINDOW50, "1", KEYMOD_META, 0 },
	{ "set video output window to 100%",
	  "Window100", ACTID_WINDOW100, "2", KEYMOD_META, 0 },
	{ "set video output window to 200%",
	  "Window200", ACTID_WINDOW200, "3", KEYMOD_META, 0 },
	{ "Enlarge the output window size by factor 1.2.",
	  "WindowEnlarge", ACTID_WINDOWENLARGE, "greater", KEYMOD_NOMOD, 0 },
	{ "Select next sub picture (subtitle) channel.",
	  "SpuNext", ACTID_SPU_NEXT, "period", KEYMOD_NOMOD, 0 },
	{ "Select previous sub picture (subtitle) channel.",
	  "SpuPrior", ACTID_SPU_PRIOR, "comma", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of control window.",
	  "ControlShow", ACTID_CONTROLSHOW, "c", KEYMOD_META, 0 },
	{ "Visibility toggle of output window visibility.",
	  "ToggleWindowVisibility", ACTID_TOGGLE_WINOUT_VISIBLITY,
	  "h", KEYMOD_NOMOD, 0 },
	{ "Select next audio channel.",
	  "AudioChannelNext", ACTID_AUDIOCHAN_NEXT, "plus", KEYMOD_NOMOD, 0 },
	{ "Select previous audio channel.",
	  "AudioChannelPrior", ACTID_AUDIOCHAN_PRIOR,
	  "minus", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of playlist editor window.",
	  "PlaylistEditor", ACTID_PLAYLIST, "p", KEYMOD_META, 0 },
	{ "Playback pause toggle.",
	  "Pause", ACTID_PAUSE, "space", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of UI windows.",
	  "ToggleVisiblity", ACTID_TOGGLE_VISIBLITY, "g", KEYMOD_NOMOD, 0 },
	{ "Fullscreen toggle.",
	  "ToggleFullscreen", ACTID_TOGGLE_FULLSCREEN, "f", KEYMOD_NOMOD, 0 },
#ifdef HAVE_XINERAMA
	{ "Xinerama fullscreen toggle.",
	  "ToggleXineramaFullscr", ACTID_TOGGLE_XINERAMA_FULLSCR,
	  "F", KEYMOD_NOMOD, 0 },
#endif
	{ "Aspect ratio values toggle.",
	  "ToggleAspectRatio", ACTID_TOGGLE_ASPECT_RATIO,
	  "a", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of stream infos window.",
	  "StreamInfosShow", ACTID_STREAM_INFOS, "i", KEYMOD_META, 0 },
	{ "Display stream information using OSD.",
	  "OSDStreamInfos", ACTID_OSD_SINFOS, "i", KEYMOD_CONTROL, 0 },
	{ "Interlaced mode toggle.",
	  "ToggleInterleave", ACTID_TOGGLE_INTERLEAVE, "i", KEYMOD_NOMOD, 0 },
	{ "Quit the program.",
	  "Quit", ACTID_QUIT, "q", KEYMOD_NOMOD, 0 },
	{ "Start playback.",
	  "Play", ACTID_PLAY, "Return", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of the setup window.",
	  "SetupShow", ACTID_SETUP, "s", KEYMOD_META, 0 },
	{ "Stop playback.",
	  "Stop", ACTID_STOP, "S", KEYMOD_NOMOD, 0 },
	{ "Select and play next mrl in the playlist.",
	  "NextMrl", ACTID_MRL_NEXT, "Next", KEYMOD_NOMOD, 0 },
	{ "Select and play previous mrl in the playlist.",
	  "PriorMrl", ACTID_MRL_PRIOR, "Prior", KEYMOD_NOMOD, 0 },
	{ "Visibility toggle of the event sender window.",
	  "EventSenderShow", ACTID_EVENT_SENDER, "e", KEYMOD_META, 0 },
	{ "Edit selected mediamark.",
	  "MediamarkEditor", ACTID_MMKEDITOR, "e", KEYMOD_CONTROL, 0 },
	{ "Eject the current medium.",
	  "Eject", ACTID_EJECT, "e", KEYMOD_NOMOD, 0 },
	{ "Set position to numeric-argument%% of current stream.",
	  "SetPosition%", ACTID_SET_CURPOS, "slash", KEYMOD_NOMOD, 0 },
	{ "Set position to beginning of current stream.",
	  "SetPosition0%", ACTID_SET_CURPOS_0,	  "0", KEYMOD_NOMOD, 0 },
	{ "Set position to 10%% of current stream.",
	  "SetPosition10%", ACTID_SET_CURPOS_10, "1", KEYMOD_NOMOD, 0 },
	{ "Set position to 20%% of current stream.",
	  "SetPosition20%", ACTID_SET_CURPOS_20, "2", KEYMOD_NOMOD, 0 },
	{ "Set position to 30%% of current stream.",
	  "SetPosition30%", ACTID_SET_CURPOS_30, "3", KEYMOD_NOMOD, 0 },
	{ "Set position to 40%% of current stream.",
	  "SetPosition40%", ACTID_SET_CURPOS_40, "4", KEYMOD_NOMOD, 0 },
	{ "Set position to 50%% of current stream.",
	  "SetPosition50%", ACTID_SET_CURPOS_50, "5", KEYMOD_NOMOD, 0 },
	{ "Set position to 60%% of current stream.",
	  "SetPosition60%", ACTID_SET_CURPOS_60, "6", KEYMOD_NOMOD, 0 },
	{ "Set position to 70%% of current stream.",
	  "SetPosition70%", ACTID_SET_CURPOS_70, "7", KEYMOD_NOMOD, 0 },
	{ "Set position to 80%% of current stream.",
	  "SetPosition80%", ACTID_SET_CURPOS_80, "8", KEYMOD_NOMOD, 0 },
	{ "Set position to 90%% of current stream.",
	  "SetPosition90%", ACTID_SET_CURPOS_90, "9", KEYMOD_NOMOD, 0 },
	{ "Enter the number 0.",
	  "Number0", ACTID_EVENT_NUMBER_0, "0", KEYMOD_MOD3, 0 },
	{ "Enter the number 1.",
	  "Number1", ACTID_EVENT_NUMBER_1, "1", KEYMOD_MOD3, 0 },
	{ "Enter the number 2.",
	  "Number2", ACTID_EVENT_NUMBER_2, "2", KEYMOD_MOD3, 0 },
	{ "Enter the number 3.",
	  "Number3", ACTID_EVENT_NUMBER_3, "3", KEYMOD_MOD3, 0 },
	{ "Enter the number 4.",
	  "Number4", ACTID_EVENT_NUMBER_4, "4", KEYMOD_MOD3, 0 },
	{ "Enter the number 5.",
	  "Number5", ACTID_EVENT_NUMBER_5, "5", KEYMOD_MOD3, 0 },
	{ "Enter the number 6.",
	  "Number6", ACTID_EVENT_NUMBER_6, "6", KEYMOD_MOD3, 0 },
	{ "Enter the number 7.",
	  "Number7", ACTID_EVENT_NUMBER_7, "7", KEYMOD_MOD3, 0 },
	{ "Enter the number 8.",
	  "Number8", ACTID_EVENT_NUMBER_8, "8", KEYMOD_MOD3, 0 },
	{ "Enter the number 9.",
	  "Number9", ACTID_EVENT_NUMBER_9, "9", KEYMOD_MOD3, 0 },
	{ "Add 10 to the next entered number.",
	  "Number10add", ACTID_EVENT_NUMBER_10_ADD, "plus", KEYMOD_MOD3, 0 },
	{ "Set position back by numeric argument in current stream.",
	  "SeekRelative-", ACTID_SEEK_REL_m, "Up", KEYMOD_MOD3, 0 }, 
	{ "Set position to -60 seconds in current stream.",
	  "SeekRelative-60", ACTID_SEEK_REL_m60, "Left", KEYMOD_NOMOD, 0 }, 
	{ "Set position forward by numeric argument in current stream.",
	  "SeekRelative+", ACTID_SEEK_REL_p, "Up", KEYMOD_META, 0 }, 
	{ "Set position to +60 seconds in current stream.",
	  "SeekRelative+60", ACTID_SEEK_REL_p60, "Right", KEYMOD_NOMOD, 0 },
	{ "Set position to -30 seconds in current stream.",
	  "SeekRelative-30", ACTID_SEEK_REL_m30, "Left", KEYMOD_META, 0 }, 
	{ "Set position to +30 seconds in current stream.",
	  "SeekRelative+30", ACTID_SEEK_REL_p30, "Right", KEYMOD_META, 0 },
	{ "Set position to -15 seconds in current stream.",
	  "SeekRelative-15", ACTID_SEEK_REL_m15, "Left", KEYMOD_CONTROL, 0 },
	{ "Set position to +15 seconds in current stream.",
	  "SeekRelative+15", ACTID_SEEK_REL_p15, "Right", KEYMOD_CONTROL, 0 },
	{ "Set position to -7 seconds in current stream.",
	  "SeekRelative-7", ACTID_SEEK_REL_m7, "Left", KEYMOD_MOD3, 0 }, 
	{ "Set position to +7 seconds in current stream.",
	  "SeekRelative+7", ACTID_SEEK_REL_p7, "Right", KEYMOD_MOD3, 0 },
	{ "Visibility toggle of mrl browser window.",
	  "MrlBrowser", ACTID_MRLBROWSER, "m", KEYMOD_META, 0 },
	{ "Audio muting toggle.",
	  "Mute", ACTID_MUTE, "m", KEYMOD_CONTROL, 0 },
	{ "Change audio syncing.",
	  "AudioVideoDecay+", ACTID_AV_SYNC_p3600, "m", KEYMOD_NOMOD, 0 },
	{ "Change audio syncing.",
	  "AudioVideoDecay-", ACTID_AV_SYNC_m3600, "n", KEYMOD_NOMOD, 0 },
	{ "Reset audio video syncing offset.",
	  "AudioVideoDecayReset", ACTID_AV_SYNC_RESET,
	  "Home", KEYMOD_NOMOD, 0 },
	{ "Increment playback speed.",
	  "SpeedFaster", ACTID_SPEED_FAST, "Up", KEYMOD_NOMOD, 0 },
	{ "Decrement playback speed.",
	  "SpeedSlower", ACTID_SPEED_SLOW, "Down", KEYMOD_NOMOD, 0 },
	{ "Reset playback speed.",
	  "SpeedReset", ACTID_SPEED_RESET, "Down", KEYMOD_META, 0 },
	{ "Increment audio volume.",
	  "Volume+", ACTID_pVOLUME, "V", KEYMOD_NOMOD, 0 },
	{ "Decrement audio volume.",
	  "Volume-", ACTID_mVOLUME, "v", KEYMOD_NOMOD, 0 },
	{ "Take a snapshot (Internal image fetch and save).",
	  "Snapshot", ACTID_SNAPSHOT, "t", KEYMOD_NOMOD, 0 },
	{ "Resize output window to stream size1:1.",
	  "Zoom1:1", ACTID_ZOOM_1_1, "s", KEYMOD_NOMOD, 0 },
	{ "Grab pointer toggle.",
	  "GrabPointer", ACTID_GRAB_POINTER, "Insert", KEYMOD_NOMOD, 0 },
	{ "Jump to Menu.",
	  "Menu", ACTID_EVENT_MENU1, "Escape", KEYMOD_NOMOD, 0 },
	{ "Jump to Title Menu.",
	  "TitleMenu", ACTID_EVENT_MENU2, "F1", KEYMOD_NOMOD, 0 },
	{ "Jump to Root Menu.",
	  "RootMenu", ACTID_EVENT_MENU3, "F2", KEYMOD_NOMOD, 0 },
	{ "Jump to Subpicture Menu.",
	  "SubpictureMenu", ACTID_EVENT_MENU4, "F3", KEYMOD_NOMOD, 0 },
	{ "Jump to Audio Menu.",
	  "AudioMenu", ACTID_EVENT_MENU5, "F4", KEYMOD_NOMOD, 0 },
	{ "Jump to Angle Menu.",
	  "AngleMenu", ACTID_EVENT_MENU6, "F5", KEYMOD_NOMOD, 0 },
	{ "Jump to Part Menu.",
	  "PartMenu", ACTID_EVENT_MENU7, "F6", KEYMOD_NOMOD, 0 },
	{ "Up event.",
	  "EventUp", ACTID_EVENT_UP, "KP_Up", KEYMOD_NOMOD, 0 },
	{ "Down event.",
	  "EventDown", ACTID_EVENT_DOWN, "KP_Down", KEYMOD_NOMOD, 0 },
	{ "Left event.",
	  "EventLeft", ACTID_EVENT_LEFT, "KP_Left", KEYMOD_NOMOD, 0 },
	{ "Right event.",
	  "EventRight", ACTID_EVENT_RIGHT, "KP_Right", KEYMOD_NOMOD, 0 },
	{ "Previous event.",
	  "EventPrior", ACTID_EVENT_PRIOR, "KP_Prior", KEYMOD_NOMOD, 0 },
	{ "Next event.",
	  "EventNext", ACTID_EVENT_NEXT, "KP_Next", KEYMOD_NOMOD, 0 },
	{ "Previous angle event.",
	  "EventAnglePrior", ACTID_EVENT_ANGLE_PRIOR,
	  "KP_End", KEYMOD_NOMOD, 0 },
	{ "Next angle event.",
	  "EventAngleNext", ACTID_EVENT_ANGLE_NEXT,
	  "KP_Home", KEYMOD_NOMOD, 0 },
	{ "Select event.",
	  "EventSelect", ACTID_EVENT_SELECT, "KP_Enter", KEYMOD_NOMOD, 0 },
	{ "Zoom into video.",
	  "ZoomIn", ACTID_ZOOM_IN, "z", KEYMOD_NOMOD, 0 },
	{ "Zoom out of video.",
	  "ZoomOut", ACTID_ZOOM_OUT, "Z", KEYMOD_NOMOD, 0 },
	{ "Zoom into video horizontally, distorting aspect ratio.",
	  "ZoomInX", ACTID_ZOOM_X_IN, "z", KEYMOD_CONTROL, 0 },
	{ "Zoom out of video horizontally, distorting aspect ratio.",
	  "ZoomOutX", ACTID_ZOOM_X_OUT, "Z", KEYMOD_CONTROL, 0 },
	{ "Zoom into video vertically, distorting aspect ratio.",
	  "ZoomInY", ACTID_ZOOM_Y_IN, "z", KEYMOD_META, 0 },
	{ "Zoom out of video vertically, distorting aspect ratio.",
	  "ZoomOutY", ACTID_ZOOM_Y_OUT, "Z", KEYMOD_META, 0 },
	{ "Reset zooming.",
	  "ZoomReset", ACTID_ZOOM_RESET,
	  "z", KEYMOD_CONTROL | KEYMOD_META, 0 },
	{ "Toggle TV modes on the DXR3.",
	  "ToggleTVmode", ACTID_TOGGLE_TVMODE,
	  "o", KEYMOD_CONTROL | KEYMOD_META, 0 },
	{ "Visibility toggle of log viewer.",
	  "ViewlogShow", ACTID_VIEWLOG, "l", KEYMOD_META, 0 },
	{ "Loop mode toggle.",
	  "ToggleLoopMode", ACTID_LOOPMODE, "l", KEYMOD_NOMOD, 0 },
	{ "Key binding editor.",
	  "KeyBindingEditor", ACTID_KBEDIT, "k", KEYMOD_META, 0 },
	{ "Switch Monitor to DPMS standby mode.",
	  "DPMSStandby", ACTID_DPMSSTANDBY, "d", KEYMOD_NOMOD, 0 },
	{ "Display Mrl/Ident toggle.",
	  "MrlIdentToggle", ACTID_MRLIDENTTOGGLE, "t", KEYMOD_CONTROL, 0 },
	{ "Scan playlist to grab stream infos.",
	  "ScanPlaylistInfo", ACTID_SCANPLAYLIST, "s", KEYMOD_CONTROL, 0 },
	{ "Add a mediamark from current playback.",
	  "AddMediamark", ACTID_ADDMEDIAMARK, "a", KEYMOD_CONTROL, 0 },
	{ "Download a skin from the skin server.",
	  "SkinDownload", ACTID_SKINDOWNLOAD, "d", KEYMOD_CONTROL, 0 },
	{ "Open file selector.",
	  "FileSelector", ACTID_FILESELECTOR, "o", KEYMOD_CONTROL, 0 },
	{ "Select a subtitle file",
	  "SubSelector", ACTID_SUBSELECT, "S", KEYMOD_CONTROL, 0 },
	{ 0, 0, 0, 0, 0, 0 }
};

/* Return a key binding entry matching with action string or 0. */
kbinding_entry_t *kbindings_lookup_action(const char *action)
{
	kbinding_entry_t *kbt = default_binding_table;
	int i;

	/* CHECKME: Not case sensitive */
	for(i = 0; kbt[i].action; i++)
		if(!strcasecmp(kbt[i].action, action))
			return &kbt[i];

	return 0;   /* Unknown action */
}

int init_keyboard(void)
{
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
	return 1;
}

void exit_keyboard(void)
{
#ifdef HAVE_LINUX_KD_H
	if(ioctl(fbxine.tty_fd, KDSETMODE, KD_TEXT) == -1)
		perror("Failed to set /dev/tty to text mode");
#endif
	close(fbxine.tty_fd);
}

char wait_for_key(void)
{
	char key;
	
	/* Press 'enter'. */
	read(fbxine.tty_fd, &key, 1);

	return key;
}
