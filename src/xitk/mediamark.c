/*
** Copyright (C) 2002 Daniel Caujolle-Bert <segfault@club-internet.fr>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"

extern int       errno;

extern gGui_t   *gGui;

typedef struct {
  FILE               *fd;
  char                buf[256];
  char               *ln;
  
  int                 entries;
  char               *type;
} playlist_t;

typedef mediamark_t **(*playlist_guess_func_t)(playlist_t *, const char *);

static char *_read_file(const char *filename, int *size) {
  struct stat  st;
  char        *buf = NULL;
  int          fd, bytes_read;
  
  if((!filename) || (!strlen(filename))) {
    fprintf(stderr, "%s(): Empty or NULL filename.\n", __XINE_FUNCTION__);
    return NULL;
  }
  
  if(stat(filename, &st) < 0) {
    fprintf(stderr, "%s(): Unable to stat() '%s' file: %s.\n",
	    __XINE_FUNCTION__, filename, strerror(errno));
    return NULL;
  }

  if((*size = st.st_size) == 0) {
    fprintf(stderr, "%s(): File '%s' is empty.\n", __XINE_FUNCTION__, filename);
    return NULL;
  }

  if((fd = open(filename, O_RDONLY)) == -1) {
    fprintf(stderr, "%s(): open(%s) failed: %s.\n", __XINE_FUNCTION__, filename, strerror(errno));
    return NULL;
  }
  
  if((buf = (char *) xine_xmalloc(*size)) == NULL) {
    fprintf(stderr, "%s(): xine_xmalloc() failed.\n", __XINE_FUNCTION__);
    close(fd);
    return NULL;
  }
  
  if((bytes_read = read(fd, buf, *size)) != *size) {
    fprintf(stderr, "%s(): read() return wrong size (%d / %d): %s.\n",
	    __XINE_FUNCTION__, bytes_read, *size, strerror(errno));
    *size = bytes_read;
  }

  close(fd);

  return buf;
}

int mediamark_store_mmk(mediamark_t **mmk, const char *mrl, const char *ident, int start, int end) {

  if((mmk != NULL) && (mrl != NULL)) {
    
    (*mmk) = (mediamark_t *) xine_xmalloc(sizeof(mediamark_t));
    (*mmk)->mrl   = strdup(mrl);
    (*mmk)->ident = strdup((ident != NULL) ? ident : mrl);
    (*mmk)->start = start;
    (*mmk)->end   = end;
    return 1;
  }

  return 0;
}

static int _mediamark_free_mmk(mediamark_t **mmk) {
  if((*mmk) != NULL) {
    SAFE_FREE((*mmk)->ident);
    SAFE_FREE((*mmk)->mrl);
    SAFE_FREE((*mmk));
    return 1;
  }
  return 0;
}

/*
 * Check if filename is a regular file
 */
static int playlist_check_for_file(const char *filename) {
  struct stat   pstat;
  
  if(filename) {

    if(((stat(filename, &pstat)) > -1) && (S_ISREG(pstat.st_mode)))
      return 1;

  }
  return 0;
}

/*
 * Cleanup the EOL ('\n','\r',' ')
 */
static void playlist_clean_eol(playlist_t *playlist) {
  char *p;

  p = playlist->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      
      p++;
    }
    
    while(p > playlist->ln) {
      --p;
      
      if(*p == ' ') 
	*p = '\0';
      else
	break;
    }
  }
}

/*
 * Get next line from opened file.
 */
static int playlist_get_next_line(playlist_t *playlist) {

 __get_next_line:

  playlist->ln = fgets(playlist->buf, 256, playlist->fd);
  
  while(playlist->ln && (*playlist->ln == ' ' || *playlist->ln == '\t')) ++playlist->ln;
  
  playlist_clean_eol(playlist);
  
  if(playlist->ln && !strlen(playlist->ln))
    goto __get_next_line;

  return((playlist->ln != NULL));
}

static void set_pos_to_value(char **p) {
  assert(*p != NULL);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Playlists guessing
 */
static mediamark_t **guess_xmms_playlist_pls(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  char         *extension;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      extension = strrchr(filename, '.');
      
      if((extension) && (!strcmp(extension, ".pls"))) {
	
	if((playlist->fd = fopen(filename, "r")) != NULL) {
	  int valid_pls = 0;
	  int entries_pls = 0;

	  while(playlist_get_next_line(playlist)) {
	    if(playlist->ln) {
	      
	      if(valid_pls) {
		
		if(entries_pls) {
		  int   entry;
		  char  buffer[2048];

		  memset(buffer, 0, sizeof(buffer));

		  if((sscanf(playlist->ln, "File%d=%s", &entry, &buffer[0])) == 2)
		    mediamark_store_mmk(&mmk[(entry - 1)], buffer, NULL, 0, -1);
		  
		}
		else {
		  if((sscanf(playlist->ln, "NumberOfEntries=%d", &entries_pls)) == 1) {
		    if(entries_pls) {
		      playlist->entries = entries_pls;
		      mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * (entries_pls + 1));
		    }
		  }
		}
		
	      }
	      else if((!strcmp(playlist->ln, "[playlist]")))
		valid_pls = 1;

	    }
	  }
	  
	  fclose(playlist->fd);
	  
	  if(valid_pls && entries_pls) {
	    mmk[entries_pls] = NULL;
	    playlist->type = strdup("PLS");
	    return mmk;
	  }
	}
      }
    }
  }
  
  return NULL;
}

static mediamark_t **guess_xmms_playlist_m3u(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  char         *extension;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      extension = strrchr(filename, '.');
      
      if((extension) && (!strcmp(extension, ".m3u"))) {
	
	if((playlist->fd = fopen(filename, "r")) != NULL) {
	  int valid_pls = 0;
	  int entries_pls = 0;
	  
	  while(playlist_get_next_line(playlist)) {

	    if(playlist->ln) {
	      
	      if(valid_pls) {
		entries_pls++;
		
		if(entries_pls == 1)
		  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		else
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_pls + 1));
		
		mediamark_store_mmk(&mmk[(entries_pls - 1)], playlist->ln, NULL, 0, -1);
		playlist->entries = entries_pls;
		
	      }
	      else {
		struct stat   vstat;
		
		/* 
		 * Validate the playlist by stating first found entry.
		 */
		if((stat(playlist->ln, &vstat)) > -1)
		  valid_pls = 1;

	      }

	    }
	  }
	  
	  fclose(playlist->fd);
	  
	  if(valid_pls && entries_pls) {
	    mmk[entries_pls] = NULL;
	    playlist->type = strdup("M3U");
	    return mmk;
	  }
	}
      }
    }
  }

  return NULL;
}

static mediamark_t **guess_xmms_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  
  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      if((playlist->fd = fopen(filename, "r")) != NULL) {
	int valid_pls = 0;
	int entries_pls = 0;
	
	while(playlist_get_next_line(playlist)) {

	  if(playlist->ln) {
	    
	    if(valid_pls) {
	      
	      if(strncmp(playlist->ln, "#", 1)) {
		
		entries_pls++;

		if(entries_pls == 1)
		  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		else
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_pls + 1));
		
		mediamark_store_mmk(&mmk[(entries_pls - 1)], playlist->ln, NULL, 0, -1);
		playlist->entries = entries_pls;

	      }

	    }
	    else if((!strcmp(playlist->ln, "#EXTM3U")))
	      valid_pls = 1;
	  }
	}
	
	fclose(playlist->fd);
	
	if(valid_pls && entries_pls) {
	  mmk[entries_pls] = NULL;
	  playlist->type = strdup("XMMS");
	  return mmk;
	}
      }
    }
  }
  
  return NULL;
}

static mediamark_t **guess_sfv_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  char         *extension;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      extension = strrchr(filename, '.');
      
      if((extension) && (!strcmp(extension, ".sfv"))) {
	
	if((playlist->fd = fopen(filename, "r")) != NULL) {
	  int valid_pls = 0;
	  int entries_pls = 0;
	  
	  while(playlist_get_next_line(playlist)) {
	    
	    if(playlist->ln) {
	      
	      if(valid_pls) {
		
		if(strncmp(playlist->ln, ";", 1)) {
		  char  entry[_PATH_MAX + _NAME_MAX + 1];
		  int   crc;

		  if((sscanf(playlist->ln, "%s %x", &entry[0], &crc)) == 2) {
		    
		    entries_pls++;
		    
		    if(entries_pls == 1)
		      mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		    else
		      mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_pls + 1));

		    mediamark_store_mmk(&mmk[(entries_pls - 1)], entry, NULL, 0, -1);
		    playlist->entries = entries_pls;

		  }
		}
	      }
	      else if(strlen(playlist->ln) > 1){
		long int   size;
		int        h, m, s;
		int        Y, M, D;
		char       fn[_PATH_MAX + _NAME_MAX + 1];
		int        n, t;

		if((sscanf(playlist->ln, ";%ld %d:%d.%d %d-%d-%d %s",
			   &size, &h, &m, &s, &Y, &M, &D, &fn[0])) == 8) {
		  char  *p = playlist->ln;
		  
		  while((*p != '\0') && (*p != '[')) p++;
		  
		  if(p && ((sscanf(p, "[%dof%d]", &n, &t)) == 2))
		    valid_pls = 1;
		}
		
	      }
	      
	    }
	  }
	  
	  fclose(playlist->fd);
	  
	  if(valid_pls && entries_pls) {
	    mmk[entries_pls] = NULL;
	    playlist->type = strdup("SFV");
	    return mmk;
	  }
	}
      }
    }
  }

  return NULL;
}

static mediamark_t **guess_raw_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      if((playlist->fd = fopen(filename, "r")) != NULL) {
	int entries_pls = 0;
	
	while(playlist_get_next_line(playlist)) {
	  
	  if(playlist->ln) {
	    
	    if((strncmp(playlist->ln, ";", 1)) &&
	       (strncmp(playlist->ln, "#", 1))) {
	      
	      entries_pls++;
	      
	      if(entries_pls == 1)
		mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
	      else
		mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_pls + 1));
	      
	      mediamark_store_mmk(&mmk[(entries_pls - 1)], playlist->ln, NULL, 0, -1);
	      playlist->entries = entries_pls;

	    }
	  }
	}
	
	fclose(playlist->fd);
	
	if(entries_pls) {
	  mmk[entries_pls] = NULL;
	  playlist->type = strdup("RAW");
	  return mmk;
	}
      }
    }
  }
  
  return NULL;
}

static mediamark_t **guess_toxine_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  
  if(filename) {
    if(playlist_check_for_file(filename)) {
      int entries_pls = 0;
      
      if((playlist->fd = fopen(filename, "r")) != NULL) {
	char    buffer[32768];
	char   *p, *pp;
	int     start = 0;
	
	memset(&buffer, 0, sizeof(buffer));
        p = buffer;
	
	while(playlist_get_next_line(playlist)) {
	  
	  if((playlist->ln) && (strlen(playlist->ln))) {
	    
	    if(strncmp(playlist->ln, "#", 1)) {
	      pp = playlist->ln;
	      
	      while(*pp != '\0') {
		if(!strncasecmp(pp, "entry {", 7)) {
		  if(!start) {
		    start = 1;
		    pp += 7;
		    memset(&buffer, 0, sizeof(buffer));
		    p = buffer;
		  }
		  else {
		    memset(&buffer, 0, sizeof(buffer));
		    p = buffer;
		    goto __discard;
		  }
		}
		if((*pp == '}') && (*(pp + 1) == ';')) {
		  if(start) {
		    start = 0;
		    pp += 2;
		    
		    { /* OKAY, check found string */
		      mediamark_t   mmkf;
		      int           mmkf_members[4];
		      char         *line;
		      char         *m;
		      
		      mmkf.start = 0;
		      mmkf.end = -1;
		      mmkf_members[0] = 0;  /* ident */
		      mmkf_members[1] = -1; /* mrl */
		      mmkf_members[2] = 0;  /* start */
		      mmkf_members[3] = 0;  /* end */
		      
		      xine_strdupa(line, (atoa(buffer)));
		      
		      while((m = xine_strsep(&line, ";")) != NULL) {
			char *key;
			
			key = atoa(m);
			if(strlen(key)) {
			  
			  if(!strncasecmp(key, "identifier", 10)) {
			    if(mmkf_members[0] == 0) {
			      mmkf_members[0] = 1;
			      set_pos_to_value(&key);
			      xine_strdupa(mmkf.ident, key);
			    }
			  }
			  else if(!strncasecmp(key, "start", 5)) {
			    if(mmkf_members[2] == 0) {
			      mmkf_members[2] = 1;
			      set_pos_to_value(&key);
			      mmkf.start = strtol(key, &key, 10);
			    }
			  }
			  else if(!strncasecmp(key, "end", 3)) {
			    if(mmkf_members[3] == 0) {
			      mmkf_members[3] = 1;
			      set_pos_to_value(&key);
			      mmkf.end = strtol(key, &key, 10);
			    }
			  }
			  else if(!strncasecmp(key, "mrl", 3)) {
			    if(mmkf_members[1] == -1) {
			      mmkf_members[1] = 1;
			      set_pos_to_value(&key);
			      xine_strdupa(mmkf.mrl, key);
			    }
			  }
			}
		      }
		      
		      if((mmkf_members[1] == 0) || (mmkf_members[1] == -1)) {
			/*  printf("wow, no mrl found\n"); */
			goto __discard;
		      }
		      
		      if(mmkf_members[0] == 0)
			xine_strdupa(mmkf.ident, mmkf.mrl);
		      
		      /*
			 STORE new mmk;
		       */
#if 0
		      printf("DUMP mediamark entry:\n");
		      printf("ident: '%s'\n", mmkf.ident);
		      printf("mrl:   '%s'\n", mmkf.mrl);
		      printf("start:  %d\n", mmkf.start);
		      printf("end:    %d\n", mmkf.end);
#endif
		      
		      entries_pls++;
		      
		      if(entries_pls == 1)
			mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		      else
			mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_pls + 1));
		      mediamark_store_mmk(&mmk[(entries_pls - 1)], 
					  mmkf.mrl, mmkf.ident, mmkf.start, mmkf.end);
		      
		      playlist->entries = entries_pls;
		      
		    }

		  }
		  else {
		    memset(&buffer, 0, sizeof(buffer));
		    p = buffer;
		    goto __discard;
		  }
		}
		
		if(*pp != '\0') {
		  *p = *pp;
		  p++;
		}
		pp++;
	      }

	    }
	  }
	__discard:
	}
	
	fclose(playlist->fd);
	
	if(entries_pls) {
	  mmk[entries_pls] = NULL;
	  playlist->type = strdup("TOX");
	  return mmk;
	}
      }
    }
  }
  return NULL;
}

static mediamark_t **guess_asx_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    char            *asx_content;
    int              size;
    int              result;
    xml_node_t      *xml_tree, *asx_entry, *asx_ref;
    xml_property_t  *asx_prop;

    if((asx_content = _read_file(filename, &size)) != NULL) {
      int entries_asx = 0;

      xml_parser_init(asx_content, size, XML_PARSER_CASE_INSENSITIVE);
      if((result = xml_parser_build_tree(&xml_tree)) > 0)
	goto __failure;
      
      if(!strcmp(xml_tree->name, "ASX")) {

	asx_prop = xml_tree->props;

	while((asx_prop) && (strcmp(asx_prop->name, "VERSION")))
	  asx_prop = asx_prop->next;
	
	if(asx_prop) {
	  int  version_major, version_minor = 0;

	  if((((sscanf(asx_prop->value, "%d.%d", &version_major, &version_minor)) == 2) ||
	      ((sscanf(asx_prop->value, "%d", &version_major)) == 1)) && 
	     ((version_major == 3) && (version_minor == 0))) {
	    
	    asx_entry = xml_tree->child;
	    while(asx_entry) {
	      if((!strcmp(asx_entry->name, "ENTRY")) || (!strcmp(asx_entry->name, "ENTRYREF"))) {
		char *title  = NULL;
		char *href   = NULL;
		char *author = NULL;

		asx_ref = asx_entry->child;
		while(asx_ref) {
		  
		  if(!strcmp(asx_ref->name, "TITLE")) {

		    if(!title)
		      title = asx_ref->data;

		  }
		  else if(!strcmp(asx_ref->name, "AUTHOR")) {

		    if(!author)
		      author = asx_ref->data;

		  }
		  else if(!strcmp(asx_ref->name, "REF")) {
		    
		    for(asx_prop = asx_ref->props; asx_prop; asx_prop = asx_prop->next) {

		      if(!strcmp(asx_prop->name, "HREF")) {

			if(!href)
			  href = asx_prop->value;
		      }
		      
		      if(href)
			break;
		    }
		  }		    

		  if(href && strlen(href)) {
		    char *atitle     = NULL;
		    char *aauthor    = NULL;
		    char *real_title = NULL;
		    int   len        = 0;

		    if(title && strlen(title)) {
		      xine_strdupa(atitle, title);
		      atitle = atoa(atitle);
		      len = strlen(atitle);

		      if(author && strlen(author)) {
			xine_strdupa(aauthor, author);
			aauthor = atoa(aauthor);
			len += strlen(aauthor) + 3;
		      }
		      
		      len++;
		    }

		    if(atitle && strlen(atitle)) {
		      real_title = (char *) alloca(len);
		      sprintf(real_title, "%s", atitle);
		      
		      if(aauthor && strlen(aauthor))
			sprintf(real_title, "%s (%s)", real_title, aauthor);
		    }
		    
		    entries_asx++;
		    
		    if(entries_asx == 1)
		      mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		    else
		      mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_asx + 1));
		    
		    mediamark_store_mmk(&mmk[(entries_asx - 1)], href, real_title, 0, -1);
		    playlist->entries = entries_asx;
		    href = title = author = NULL;
		  }
		  
		  asx_ref = asx_ref->next;
		}
		href = title = author = NULL;
	      }
	      asx_entry = asx_entry->next;
	    }
	  }
	  else
	    fprintf(stderr, "%s(): Wrong ASX version: %s\n", __XINE_FUNCTION__, asx_prop->value);

	}
	else
	  fprintf(stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
	
      }
      else
	fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
      
      if(entries_asx) {
	mmk[entries_asx] = NULL;
	playlist->type = strdup("ASX3");
	return mmk;
      }
      
      xml_parser_free_tree(xml_tree);
 __failure:
      free(asx_content);
    }
  }
  
  return NULL;
}

/*
 * Public
 */
int mediamark_get_entry_from_id(const char *ident) {
  if(ident && gGui->playlist.num) {
    int i;
    
    for(i = 0; i < gGui->playlist.num; i++) {
      if(!strcasecmp(ident, gGui->playlist.mmk[i]->ident))
	return i;
    }
  }
  return -1;
}

void mediamark_add_entry(const char *mrl, const char *ident, int start, int end) {
  if(!gGui->playlist.num)
    gGui->playlist.mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
  else { 
    if(gGui->playlist.num > 1) {
      gGui->playlist.mmk = (mediamark_t **) 
	realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 1));
    }
  }
  
  if(mediamark_store_mmk(&gGui->playlist.mmk[gGui->playlist.num], mrl, ident, start, end))
    gGui->playlist.num++;
}

void mediamark_free_mediamarks(void) {
  
  if(gGui->playlist.num) {
    int i;
    
    for(i = 0; i < gGui->playlist.num; i++)
      mediamark_free_entry(i);
    
    SAFE_FREE(gGui->playlist.mmk);
    gGui->playlist.num = 0;
    gGui->playlist.cur = 0;
  }
}

void mediamark_replace_entry(mediamark_t **mmk, 
			     const char *mrl, const char *ident, int start, int end) {
  SAFE_FREE((*mmk)->mrl);
  SAFE_FREE((*mmk)->ident);
  (*mmk)->start = 0;
  (*mmk)->end = -1;

  (void) mediamark_store_mmk(mmk, mrl, ident, start, end);
}

const mediamark_t *mediamark_get_current_mmk(void) {

  if(gGui->playlist.mmk && gGui->playlist.num)
    return gGui->playlist.mmk[gGui->playlist.cur];

  return (mediamark_t *) NULL;
}

const char *mediamark_get_current_mrl(void) {

  if(gGui->playlist.mmk && gGui->playlist.num)
    return gGui->playlist.mmk[gGui->playlist.cur]->mrl;

  return NULL;
}

const char *mediamark_get_current_ident(void) {

  if(gGui->playlist.mmk && gGui->playlist.num)
    return gGui->playlist.mmk[gGui->playlist.cur]->ident;

  return NULL;
}

void mediamark_free_entry(int offset) {

  if(gGui->playlist.num && (offset < gGui->playlist.num)) {
    if(_mediamark_free_mmk(&gGui->playlist.mmk[offset]))
      gGui->playlist.num--;
  }
}

void mediamark_load_mediamarks(const char *filename) {
  playlist_t             *playlist;
  int                     i, found, onum;
  mediamark_t           **mmk = NULL;
  mediamark_t           **ommk;
  playlist_guess_func_t   guess_functions[] = {
    guess_asx_playlist,
    guess_toxine_playlist,
    guess_xmms_playlist_pls,
    guess_xmms_playlist_m3u,
    guess_xmms_playlist,
    guess_sfv_playlist,
    guess_raw_playlist,
    NULL
  };

  playlist = (playlist_t *) xine_xmalloc(sizeof(playlist_t));

  for(i = 0, found = 0; (guess_functions[i] != NULL) && (found == 0); i++) {
    if((mmk = guess_functions[i](playlist, filename)) != NULL)
      found = 1;
  }
  
  if(found)
    printf(_("Playlist file (%s) is valid (%s).\n"), filename, playlist->type);
  else {
    fprintf(stderr, _("Playlist file (%s) is invalid.\n"), filename);
    SAFE_FREE(playlist);
    return;
  }

  ommk = gGui->playlist.mmk;
  onum = gGui->playlist.num;
  
  gGui->playlist.mmk = mmk;
  gGui->playlist.num = playlist->entries;
  gGui->playlist.cur = 0;

  for(i = 0; i < onum; i++)
    (void) _mediamark_free_mmk(&ommk[i]);

  SAFE_FREE(ommk);

  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);
}

void mediamark_save_mediamarks(const char *filename) {
  char *fullfn;
  char *pn;
  char *fn;
  int   status = 1;

  xine_strdupa(fullfn, filename);
  
  pn = fullfn;

  fn = strrchr(fullfn, '/');
  
  if(fn) {
    *fn = '\0';
    fn++;
    status = mkdir_safe(pn);
  }
  else
    fn = pn;

  if(status && fn) {
    int   i;
    FILE *fd;

    if((fd = fopen(filename, "w")) != NULL) {

      fprintf(fd, "# toxine playlist\n");
      
      for(i = 0; i < gGui->playlist.num; i++) {
	fprintf(fd, "\nentry {\n");
	fprintf(fd, "\tidentifier = %s;\n", gGui->playlist.mmk[i]->ident);
	fprintf(fd, "\tmrl = %s;\n", gGui->playlist.mmk[i]->mrl);
	fprintf(fd, "\tstart = %d;\n", gGui->playlist.mmk[i]->start);
	fprintf(fd, "\tend = %d;\n", gGui->playlist.mmk[i]->end);
	fprintf(fd,"};\n");
      }

      fprintf(fd, "# END\n");
      
      fclose(fd);
      printf(_("Playlist '%s' saved.\n"), filename);
    }
    else
      fprintf(stderr, _("Unable to save playlist (%s): %s.\n"), filename, strerror(errno));

  }
}
