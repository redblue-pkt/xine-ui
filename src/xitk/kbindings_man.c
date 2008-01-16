/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "kbindings_common.h"


gGui_t  *gGui;

int main(int argc, char **argv) {
  int   i;

  gGui              = (gGui_t *) xine_xmalloc(sizeof(gGui_t));
  gGui->keymap_file = (char *) xine_xmalloc(XITK_PATH_MAX + XITK_NAME_MAX + 2);
  sprintf(gGui->keymap_file, "%s/%s/%s", xine_get_homedir(), ".xine", "keymap");
  
  if((gGui->kbindings = kbindings_init_kbinding())) {
    kbinding_entry_t **k = gGui->kbindings->entry;
    
    for(i = 0; k[i]->action != NULL; i++) {
      
      if(k[i]->is_alias || (!strcasecmp(k[i]->key, "VOID")))
	continue;
      
      printf(".IP \"");
      
      if(k[i]->modifier != KEYMOD_NOMOD) {
	char buf[256];
	
	memset(&buf, 0, sizeof(buf));
	
	if(k[i]->modifier & KEYMOD_CONTROL)
	  strcpy(buf, "\\fBC\\fP");
	if(k[i]->modifier & KEYMOD_META) {
	  if(buf[0])
	    strlcat(buf, "\\-", sizeof(buf));
	  strlcat(buf, "\\fBM\\fP", sizeof(buf));
	}
	if(k[i]->modifier & KEYMOD_MOD3) {
	  if(buf[0])
	    strlcat(buf, "\\-", sizeof(buf));
	  strlcat(buf, "\\fBM3\\fP", sizeof(buf));
	}
	if(k[i]->modifier & KEYMOD_MOD4) {
	  if(buf[0])
	    strlcat(buf, "\\-", sizeof(buf));
	  strlcat(buf, "\\fBM4\\fP", sizeof(buf));
	}
	if(k[i]->modifier & KEYMOD_MOD5) {
	  if(buf[0])
	    strlcat(buf, "\\-", sizeof(buf));
	  strlcat(buf, "\\fBM5\\fP", sizeof(buf));
	}
	printf("%s\\-", buf);
      }
      
      puts("\\fB");
      if(strlen(k[i]->key) > 1)
	puts("\\<");
      
      if(!strncmp(k[i]->key, "KP_", 3))
	printf("Keypad %s", (k[i]->key + 3));
      else
	puts(k[i]->key);
      
      if(strlen(k[i]->key) > 1)
	puts("\\>");
      puts("\\fP");
      
      puts("\"\n");
      
      printf("%c%s\n", (toupper(k[i]->comment[0])), (k[i]->comment + 1));
    }
    
    kbindings_free_kbinding(&gGui->kbindings);
  }

  free(gGui->keymap_file);
  free(gGui);

  return 0;
}
