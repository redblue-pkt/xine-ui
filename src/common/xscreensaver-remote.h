/* xscreensaver-command, Copyright (c) 1991-1998
 *  by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *
 * Xine specific-code by Daniel Caujolle-Bert (segfault@club-internet.fr>
 */

#ifndef _XSCREENSAVER_REMOTE_H_
#define _XSCREENSAVER_REMOTE_H_

void xscreensaver_remote_init(Display *);
int is_xscreensaver_running(Display *);
int xscreensaver_kill_server(Display *);
void xscreensaver_start_server(void);

#endif /* _XSCREENSAVER_REMOTE_H_ */
