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
 * $Id$
 *
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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       250 /* 210*/

static char            *btnfontname  = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
static char            *fontname     = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";

typedef struct {
  apply_callback_t              callback;
  void                         *user_data;

  xitk_window_t                *xwin;

  xitk_widget_list_t           *widget_list;

  xitk_widget_t                *mrl;
  xitk_widget_t                *ident;
  xitk_widget_t                *sub;
  xitk_widget_t                *start;
  xitk_widget_t                *end;
  xitk_widget_t                *offset;

  mediamark_t                 **mmk;

  int                           running;
  int                           visible;
  xitk_register_key_t           widget_key;
} _mmk_editor_t;

static _mmk_editor_t  *mmkeditor = NULL;

extern gGui_t   *gGui;

typedef struct {
  char                *data;
  char               **lines;
  int                  numl;
  
  int                  entries;
  char                *type;
} playlist_t;

typedef mediamark_t **(*playlist_guess_func_t)(playlist_t *, const char *);

static char *_download_file(const char *filename, int *size) {
  download_t  *download;
  char        *buf = NULL;
  
  if((!filename) || (!strlen(filename))) {
    fprintf(stderr, "%s(): Empty or NULL filename.\n", __XINE_FUNCTION__);
    return NULL;
  }
  
  download = (download_t *) xine_xmalloc(sizeof(download_t));
  download->buf    = NULL;
  download->error  = NULL;
  download->size   = 0;
  download->status = 0; 
  
  if((network_download(filename, download))) {
    *size = download->size;
    buf = (char *) xine_xmalloc(*size);
    memcpy(buf, download->buf, *size);
  }
  else
    xine_error("Unable to download '%s':\n%s.", filename, download->error);

  if(download->buf)
    free(download->buf);
  if(download->error)
    free(download->error);
  free(download);
  
  return buf;
}

static int _file_exist(char *filename) {
  struct stat  st;
  
  if(filename && (stat(filename, &st) == 0))
    return 1;
  
  return 0;
}

static char *_read_file(const char *filename, int *size) {
  struct stat  st;
  char        *buf = NULL;
  int          fd, bytes_read;
  char        *extension;
  
  if((!filename) || (!strlen(filename))) {
    fprintf(stderr, "%s(): Empty or NULL filename.\n", __XINE_FUNCTION__);
    return NULL;
  }
  
  extension = strrchr(filename, '.');
  if(extension && 
     ((!strncasecmp(extension, ".asx", 4)) || 
      (!strncasecmp(extension, ".smi", 4)) || (!strncasecmp(extension, ".smil", 5))) && 
     ((!strncasecmp(filename, "http://", 7)) || (!strncasecmp(filename, "ftp://", 6))))
    return _download_file(filename, size);
  
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
  
  if((buf = (char *) xine_xmalloc(*size + 1)) == NULL) {
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

  buf[*size] = '\0';

  return buf;
}

int mediamark_store_mmk(mediamark_t **mmk, 
			const char *mrl, const char *ident, const char *sub, 
			int start, int end, int offset) {

  if((mmk != NULL) && (mrl != NULL)) {
    
    (*mmk) = (mediamark_t *) xine_xmalloc(sizeof(mediamark_t));
    (*mmk)->mrl    = strdup(mrl);
    (*mmk)->ident  = strdup((ident != NULL) ? ident : mrl);
    (*mmk)->sub    = (sub != NULL) ? strdup(sub) : NULL;
    (*mmk)->start  = start;
    (*mmk)->end    = end;
    (*mmk)->offset = offset;
    (*mmk)->played = 0;
    return 1;
  }

  return 0;
}

mediamark_t *mediamark_clone_mmk(mediamark_t *mmk) {
  mediamark_t *cmmk = NULL;

  if(mmk && mmk->mrl) {
    cmmk = (mediamark_t *) xine_xmalloc(sizeof(mediamark_t));
    cmmk->mrl    = strdup(mmk->mrl);
    cmmk->ident  = (mmk->ident) ? strdup(mmk->ident) : NULL;
    cmmk->sub    = (mmk->sub) ? strdup(mmk->sub) : NULL;
    cmmk->start  = mmk->start;
    cmmk->end    = mmk->end;
    cmmk->offset = mmk->offset;
    cmmk->played = mmk->played;
  }

  return cmmk;
}

int mediamark_free_mmk(mediamark_t **mmk) {
  if((*mmk) != NULL) {
    SAFE_FREE((*mmk)->ident);
    SAFE_FREE((*mmk)->mrl);
    SAFE_FREE((*mmk)->sub);
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
 * Split lines from playlist->data
 */
static int playlist_split_data(playlist_t *playlist) {
  
  if(playlist->data) {
    char *buf, *p;

    /* strsep modify oriinal string, avoid its corruption */
    buf = (char *) xine_xmalloc(strlen(playlist->data));
    memcpy(buf, playlist->data, strlen(playlist->data));
    playlist->numl = 0;

    while((p = xine_strsep(&buf, "\n")) != NULL) {
      if(p && (p <= buf) && (strlen(p))) {

	if(playlist->numl == 0)
	  playlist->lines = (char **) xine_xmalloc(sizeof(char *) * 2);
	else
	  playlist->lines = (char **) realloc(playlist->lines, sizeof(char *) * (playlist->numl + 2));
	
	while((*(p + strlen(p) - 1) == '\n') || (*(p + strlen(p) - 1) == '\r'))
	  *(p + strlen(p) - 1) = '\0';

	playlist->lines[playlist->numl++] = strdup(p);
	playlist->lines[playlist->numl] = NULL;
      }
    }
    
    free(buf);
  }
  
  return (playlist->numl > 0);
}

static void set_pos_to_value(char **p) {
  assert(*p != NULL);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Playlists guessing
 */
static mediamark_t **guess_pls_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  char         *extension;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      
      extension = strrchr(filename, '.');
      
      if((extension) && (!strcasecmp(extension, ".pls"))) {
	char *pls_content;
	int   size;
	
	if((pls_content = _read_file(filename, &size)) != NULL) {
	  
	  playlist->data = pls_content;
	  
	  if(playlist_split_data(playlist)) {
	    int   valid_pls = 0;
	    int   entries_pls = 0;
	    int   linen = 0;
	    char *ln;
	    
	    while((ln = playlist->lines[linen++]) != NULL) {
	      if(ln) {
		
		if(valid_pls) {
		  
		  if(entries_pls) {
		    int   entry;
		    char  buffer[2048];
		    
		    memset(buffer, 0, sizeof(buffer));
		    
		    if((sscanf(ln, "File%d=%s", &entry, &buffer[0])) == 2)
		      mediamark_store_mmk(&mmk[(entry - 1)], buffer, NULL, NULL, 0, -1, 0);
		    
		  }
		  else {
		    if((sscanf(ln, "NumberOfEntries=%d", &entries_pls)) == 1) {
		      if(entries_pls) {
			playlist->entries = entries_pls;
			mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * (entries_pls + 1));
		      }
		    }
		  }
		  
		}
		else if((!strcasecmp(ln, "[playlist]")))
		  valid_pls = 1;
		
	      }
	    }
	    
	    if(valid_pls && entries_pls) {
	      mmk[entries_pls] = NULL;
	      playlist->type = strdup("PLS");
	      return mmk;
	    }

	    while(playlist->numl) {
	      free(playlist->lines[playlist->numl - 1]);
	      playlist->numl--;
	    }
	    SAFE_FREE(playlist->lines);
	  }
	  
	  free(pls_content);
	}
      }
    }
  }
  
  return NULL;
}

static mediamark_t **guess_m3u_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  
  if(filename) {
    if(playlist_check_for_file(filename)) {
      char *m3u_content;
      int   size;
      
      if((m3u_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = m3u_content;
	
	if(playlist_split_data(playlist)) {
	  int   valid_m3u   = 0;
	  int   entries_m3u = 0;
	  char *ptitle      = NULL;
	  char *title       = NULL;
	  char *path;
	  char *origin = NULL;
	  int   linen = 0;
	  char *ln;
	  

	  path = strrchr(filename, '/');
	  if(path && (path > filename)) {
	    origin = (char *) xine_xmalloc((path - filename) + 2);
	    strncat(origin, filename, (path - filename));
	    strcat(origin, "/");
	  }
	  
	  while((ln = playlist->lines[linen++]) != NULL) {
	    if(ln) {
	      
	      if(valid_m3u) {
		
		if(!strncmp(ln, "#EXTINF", 7)) {
		  if((ptitle = strchr(ln, ',')) != NULL) {
		    ptitle++;
		    xine_strdupa(title, ptitle);
		  }
		}
		else {
		  char  buffer[_PATH_MAX + _NAME_MAX + 1];
		  char *entry;
		  
		  if(entries_m3u == 0)
		    mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		  else
		    mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_m3u + 2));

		  entry = ln;

		  if(origin) {
		    memset(&buffer, 0, sizeof(buffer));
		    sprintf(buffer, "%s", origin);
		    
		    if((buffer[strlen(buffer) - 1] == '/') && (*ln == '/'))
		      buffer[strlen(buffer) - 1] = '\0';
		    
		    sprintf(buffer, "%s%s", buffer, ln);
		    
		    if(_file_exist(buffer))
		      entry = buffer;
		  }
		    
		  mediamark_store_mmk(&mmk[entries_m3u], entry, title, NULL, 0, -1, 0);
		  playlist->entries = ++entries_m3u;
		  ptitle = title = NULL;
		}
		
	      }
	      else if((!strcasecmp(ln, "#EXTM3U")))
		valid_m3u = 1;
	    }
	  }
	  
	  if(valid_m3u && entries_m3u) {
	    mmk[entries_m3u] = NULL;
	    playlist->type = strdup("M3U");
	  }

	  if(origin)
	    free(origin);

	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	  SAFE_FREE(playlist->lines);
	}
	
	free(m3u_content);
      }
    }
  }
  
  return mmk;
}

static mediamark_t **guess_sfv_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      char  *extension;
  
      extension = strrchr(filename, '.');
      
      if((extension) && (!strcasecmp(extension, ".sfv"))) {
	char *sfv_content;
	int   size;
	
	if((sfv_content = _read_file(filename, &size)) != NULL) {
	  
	  playlist->data = sfv_content;

	  if(playlist_split_data(playlist)) {
	    int    valid_sfv = 0;
	    int    entries_sfv = 0;
	    char  *path;
	    char  *origin = NULL;
	    int    linen = 0;
	    char  *ln;

	    path = strrchr(filename, '/');
	    if(path && (path > filename)) {
	      origin = (char *) xine_xmalloc((path - filename) + 2);
	      strncat(origin, filename, (path - filename));
	      strcat(origin, "/");
	    }
	    
	    while((ln = playlist->lines[linen++]) != NULL) {
	      
	      if(ln) {
		
		if(valid_sfv) {
		  
		  if(strncmp(ln, ";", 1)) {
		    char  mentry[_PATH_MAX + _NAME_MAX + 1];
		    char  buffer[_PATH_MAX + _NAME_MAX + 1];
		    char *entry;
		    int   crc;
		    
		    if((sscanf(ln, "%s %x", &mentry[0], &crc)) == 2) {
		      
		      if(entries_sfv == 0)
			mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		      else
			mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_sfv + 2));
		      
		      entry = mentry;
		      
		      if(origin) {
			memset(&buffer, 0, sizeof(buffer));
			sprintf(buffer, "%s", origin);
			
			if((buffer[strlen(buffer) - 1] == '/') && (mentry[0] == '/'))
			  buffer[strlen(buffer) - 1] = '\0';
			
			sprintf(buffer, "%s%s", buffer, mentry);
			
			if(_file_exist(buffer))
			  entry = buffer;
		      }

		      mediamark_store_mmk(&mmk[entries_sfv], entry, NULL, NULL, 0, -1, 0);
		      playlist->entries = ++entries_sfv;
		    }
		  }
		}
		else if(strlen(ln) > 1){
		  long int   size;
		  int        h, m, s;
		  int        Y, M, D;
		  char       fn[_PATH_MAX + _NAME_MAX + 1];
		  int        n, t;
		  
		  if((sscanf(ln, ";%ld %d:%d.%d %d-%d-%d %s",
			     &size, &h, &m, &s, &Y, &M, &D, &fn[0])) == 8) {
		    char  *p = ln;
		    
		    while((*p != '\0') && (*p != '[')) p++;
		    
		    if(p && ((sscanf(p, "[%dof%d]", &n, &t)) == 2))
		      valid_sfv = 1;
		  }
		}		
	      }
	    }

	    if(valid_sfv && entries_sfv) {
	      mmk[entries_sfv] = NULL;
	      playlist->type = strdup("SFV");
	    }
	    
	    if(origin)
	      free(origin);

	    while(playlist->numl) {
	      free(playlist->lines[playlist->numl - 1]);
	      playlist->numl--;
	    }
	    SAFE_FREE(playlist->lines);
	  }
	  free(sfv_content);
	}
      }
    }
  }

  return mmk;
}

static mediamark_t **guess_raw_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    if(playlist_check_for_file(filename)) {
      char *raw_content;
      int   size;
      
      if((raw_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = raw_content;
	
	if(playlist_split_data(playlist)) {
	  char *path;
	  char *origin = NULL;
	  int   entries_raw = 0;
	  int   linen = 0;
	  char *ln;
	  
	  while((ln = playlist->lines[linen++]) != NULL) {
	    if(ln) {
	      
	      if((strncmp(ln, ";", 1)) && (strncmp(ln, "#", 1))) {
		char  buffer[_PATH_MAX + _NAME_MAX + 1];
		char *entry;
		
		path = strrchr(filename, '/');
		if(path && (path > filename)) {
		  origin = (char *) xine_xmalloc((path - filename) + 2);
		  strncat(origin, filename, (path - filename));
		  strcat(origin, "/");
		}

		if(entries_raw == 0)
		  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		else
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_raw + 2));
		
		entry = ln;

		if(origin) {
		  memset(&buffer, 0, sizeof(buffer));
		  sprintf(buffer, "%s", origin);
		  
		  if((buffer[strlen(buffer) - 1] == '/') && (*ln == '/'))
		    buffer[strlen(buffer) - 1] = '\0';
		  
		  sprintf(buffer, "%s%s", buffer, ln);
		  
		  if(_file_exist(buffer))
		    entry = buffer;
		}
		
		mediamark_store_mmk(&mmk[entries_raw], entry, NULL, NULL, 0, -1, 0);
		playlist->entries = ++entries_raw;
	      }
	    }
	  }
	  
	  if(entries_raw) {
	    mmk[entries_raw] = NULL;
	    playlist->type = strdup("RAW");
	  }

	  if(origin)
	    free(origin);

	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	  SAFE_FREE(playlist->lines);
	}

	free(raw_content);
      }
    }
  }
  
  return mmk;
}

static mediamark_t **guess_toxine_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  
  if(filename) {
    if(playlist_check_for_file(filename)) {
      int   entries_tox = 0;
      char *tox_content;
      int   size;
      
      if((tox_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = tox_content;
	
	if(playlist_split_data(playlist)) {
	  char    buffer[23768];
	  char   *p, *pp;
	  int     start = 0;
	  int     linen = 0;
	  char   *ln;
	  
	  memset(&buffer, 0, sizeof(buffer));
	  p = buffer;

	  while((ln = playlist->lines[linen++]) != NULL) {
	    
	    if(!start) {
	      memset(&buffer, 0, sizeof(buffer));
	      p = buffer;
	    }

	    if((ln) && (strlen(ln))) {
	      
	      if(strncmp(ln, "#", 1)) {

		pp = ln;
		
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
			int           mmkf_members[6];
			char         *line;
			char         *m;
			
			mmkf.sub        = NULL;
			mmkf.start      = 0;
			mmkf.end        = -1;
			mmkf_members[0] = 0;  /* ident */
			mmkf_members[1] = -1; /* mrl */
			mmkf_members[2] = 0;  /* start */
			mmkf_members[3] = 0;  /* end */
			mmkf_members[4] = 0;  /* offset */
			mmkf_members[5] = 0;  /* sub */
			
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
			    else if(!strncasecmp(key, "subtitle", 8)) {
			      if(mmkf_members[5] == 0) {
				mmkf_members[5] = 1;
				set_pos_to_value(&key);
				xine_strdupa(mmkf.sub, key);
			      }
			    }
			    else if(!strncasecmp(key, "offset", 6)) {
			      if(mmkf_members[4] == 0) {
				mmkf_members[4] = 1;
				set_pos_to_value(&key);
				mmkf.offset = strtol(key, &key, 10);
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
			printf("sub:   '%s'\n", mmkf.sub);
			printf("start:  %d\n", mmkf.start);
			printf("end:    %d\n", mmkf.end);
			printf("offset: %d\n", mmkf.offset);
#endif
			
			if(entries_tox == 0)
			  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
			else
			  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_tox + 2));
			mediamark_store_mmk(&mmk[entries_tox], 
					    mmkf.mrl, mmkf.ident, mmkf.sub, 
					    mmkf.start, mmkf.end, mmkf.offset);
			
			playlist->entries = ++entries_tox;
			
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
	  __discard: ;
	  }
	  
	  if(entries_tox) {
	    mmk[entries_tox] = NULL;
	    playlist->type = strdup("TOX");
	  }
	  
	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	  SAFE_FREE(playlist->lines);
	}
	free(tox_content);
      }
    }
  }
  return mmk;
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
      if((result = xml_parser_build_tree(&xml_tree)) != XML_PARSER_OK)
	goto __failure;

      if(!strcasecmp(xml_tree->name, "ASX")) {

	asx_prop = xml_tree->props;

	while((asx_prop) && (strcasecmp(asx_prop->name, "VERSION")))
	  asx_prop = asx_prop->next;
	
	if(asx_prop) {
	  int  version_major, version_minor = 0;

	  if((((sscanf(asx_prop->value, "%d.%d", &version_major, &version_minor)) == 2) ||
	      ((sscanf(asx_prop->value, "%d", &version_major)) == 1)) && 
	     ((version_major == 3) && (version_minor == 0))) {
	    
	  __parse_anyway:
	    asx_entry = xml_tree->child;
	    while(asx_entry) {
	      if((!strcasecmp(asx_entry->name, "ENTRY")) ||
		 (!strcasecmp(asx_entry->name, "ENTRYREF"))) {
		char *title  = NULL;
		char *href   = NULL;
		char *author = NULL;

		asx_ref = asx_entry->child;
		while(asx_ref) {
		  
		  if(!strcasecmp(asx_ref->name, "TITLE")) {

		    if(!title)
		      title = asx_ref->data;

		  }
		  else if(!strcasecmp(asx_ref->name, "AUTHOR")) {

		    if(!author)
		      author = asx_ref->data;

		  }
		  else if(!strcasecmp(asx_ref->name, "REF")) {
		    
		    for(asx_prop = asx_ref->props; asx_prop; asx_prop = asx_prop->next) {

		      if(!strcasecmp(asx_prop->name, "HREF")) {

			if(!href)
			  href = asx_prop->value;
		      }
		      
		      if(href)
			break;
		    }
		  }		    
		  
		  asx_ref = asx_ref->next;
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
		  
		  if(entries_asx == 0)
		    mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		  else
		    mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_asx + 2));
		  
		  mediamark_store_mmk(&mmk[entries_asx], href, real_title, NULL, 0, -1, 0);
		  playlist->entries = ++entries_asx;
		}
		
		href = title = author = NULL;
	      }
	      asx_entry = asx_entry->next;
	    }
	  }
	  else
	    fprintf(stderr, "%s(): Wrong ASX version: %s\n", __XINE_FUNCTION__, asx_prop->value);

	}
	else {
	  fprintf(stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
	  fprintf(stderr, "%s(): last chance: try to parse it anyway\n", __XINE_FUNCTION__);
	  goto __parse_anyway;
	}
	
      }
      else
	fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
      
      xml_parser_free_tree(xml_tree);
    __failure:

      /* Maybe it's 'ASF <url> */
      if(entries_asx == 0) {
	
	playlist->data = asx_content;
	
	if(playlist_split_data(playlist)) {
	  int    linen = 0;
	  char  *ln;

	  while((ln = playlist->lines[linen++]) != NULL) {
	    
	    if(!strncasecmp("ASF", ln, 3)) {
	      char *p = ln + 3;
	      
	      while(p && ((*p == ' ') || (*p == '\t')))
		p++;

	      if(p && strlen(p)) {
		if(entries_asx == 0)
		  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
		else
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_asx + 2));
		
		mediamark_store_mmk(&mmk[entries_asx], p, p, NULL, 0, -1, 0);
		playlist->entries = ++entries_asx;
	      }
	    }
	  }
	}
      }

      free(asx_content);
      
      if(entries_asx) {
	mmk[entries_asx] = NULL;
	playlist->type = strdup("ASX3");
	return mmk;
      }
    }
  }
  
  return NULL;
}

static void gx_get_entries(playlist_t *playlist, 
			   mediamark_t ***mmk, int *entries, xml_node_t *entry) {
  xml_property_t  *prop;
  xml_node_t      *ref;
  xml_node_t      *node = entry;  
  
  while(node) {
    if(!strcasecmp(node->name, "ENTRY")) {
      char *title  = NULL;
      char *href   = NULL;
      int   start  = 0;
      
      ref = node->child;
      while(ref) {
	
	if(!strcasecmp(ref->name, "TITLE")) {

	  if(!title)
	    title = ref->data;
	  
	}
	else if(!strcasecmp(ref->name, "REF")) {

	  for(prop = ref->props; prop; prop = prop->next) {
	    if(!strcasecmp(prop->name, "HREF")) {
	      
	      if(!href)
		href = prop->value;
	    }
	    
	    if(href)
	      break;
	  }
	}		    
	else if(!strcasecmp(ref->name, "TIME")) {
	  prop = ref->props;
	  start = strtol(prop->value, &prop->value, 10);
	}
	
	ref = ref->next;
      }
      
      if(href && strlen(href)) {
	char *real_title = NULL;
	char *atitle     = NULL;
	int   len        = 0;
	
	if(title && strlen(title)) {
	  xine_strdupa(atitle, title);
	  atitle = atoa(atitle);
	  len = strlen(atitle);
	  
	  len++;
	}
	
	if(atitle && strlen(atitle)) {
	  real_title = (char *) alloca(len);
	  sprintf(real_title, "%s", atitle);
	  
	}
	
	if(*entries == 0)
	  (*mmk) = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
	else
	  (*mmk) = (mediamark_t **) realloc((*mmk), sizeof(mediamark_t *) * (*entries + 2));
	
	mediamark_store_mmk(&(*mmk)[*entries], href, real_title, NULL, start, -1, 0);
	playlist->entries = ++(*entries);
      }
      
      href = title = NULL;
    }
    node = node->next;
  }
}
static mediamark_t **guess_gx_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    char            *gx_content;
    int              size;
    int              result;
    xml_node_t      *xml_tree, *gx_entry;
    xml_property_t  *gx_prop;


    if((gx_content = _read_file(filename, &size)) != NULL) {
      int entries_gx = 0;

      xml_parser_init(gx_content, size, XML_PARSER_CASE_INSENSITIVE);
      if((result = xml_parser_build_tree(&xml_tree)) != XML_PARSER_OK)
	goto __failure;

      if(!strcasecmp(xml_tree->name, "GXINEMM")) {

	gx_prop = xml_tree->props;

	while((gx_prop) && (strcasecmp(gx_prop->name, "VERSION")))
	  gx_prop = gx_prop->next;
	
	if(gx_prop) {
	  int  version_major;

	  if(((sscanf(gx_prop->value, "%d", &version_major)) == 1) && (version_major == 1)) {
	    
	    gx_entry = xml_tree->child;
	    while(gx_entry) {
	      
	      if(!strcasecmp(gx_entry->name, "SUB"))
		gx_get_entries(playlist, &mmk, &entries_gx, gx_entry->child);
	      
	      if(!strcasecmp(gx_entry->name, "ENTRY"))
		gx_get_entries(playlist, &mmk, &entries_gx, gx_entry);
	      
	      gx_entry = gx_entry->next;
	    }
	  }
	  else
	    fprintf(stderr, "%s(): Wrong GXINEMM version: %s\n", __XINE_FUNCTION__, gx_prop->value);

	}
	else
	  fprintf(stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
	
      }
      else
	fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
      
      xml_parser_free_tree(xml_tree);
    __failure:
      
      free(gx_content);
      
      if(entries_gx) {
	mmk[entries_gx] = NULL;
	playlist->type = strdup("GXMM");
	return mmk;
      }
    }
  }
  
  return NULL;
}

/*
 * ********************************** SMIL BEGIN ***********************************
 */
#undef DEBUG_SMIL

#ifdef DEBUG_SMIL
static int offset = 0;
#define palign {int i; for(i = 0; i < offset; i++) { printf(" "); } } while(0)
#endif

typedef struct smil_node_s smil_node_t;

typedef struct {
  char   *anchor;

  int     repeat;
  int     begin;
  int     end;

  int     clip_begin;
  int     clip_end;

  int     duration;

} smil_property_t;

struct smil_node_s {
  mediamark_t      *mmk;
  smil_property_t   prop;
  smil_node_t      *next;
};

typedef struct {
  /* Header */
  char   *title;
  char   *author;
  char   *copyright;
  char   *year;
  char   *base;
  /* End Header */
  
  smil_node_t *first;
  smil_node_t *node;
  
} smil_t;

/* protos */
static void smil_seq(smil_t *, smil_node_t **, xml_node_t *, smil_property_t *);
static void smil_par(smil_t *, smil_node_t **, xml_node_t *, smil_property_t *);
static void smil_switch(smil_t *, smil_node_t **, xml_node_t *, smil_property_t *);
static void smil_repeat(int, smil_node_t *, smil_node_t **, smil_property_t *);

static void smil_init_smil_property(smil_property_t *sprop) {
  sprop->anchor     = NULL;
  sprop->repeat     = 1;
  sprop->begin      = 0;
  sprop->clip_begin = 0;
  sprop->end        = -1;
  sprop->clip_end   = -1;
  sprop->duration   = 0;
}
static smil_node_t *smil_new_node(void) {
  smil_node_t *node;

  node       = (smil_node_t *) xine_xmalloc(sizeof(smil_node_t));
  node->mmk  = NULL;
  node->next = NULL;

  smil_init_smil_property(&(node->prop));

  return node;
}
static mediamark_t *smil_new_mediamark(void) {
  mediamark_t *mmk;

  mmk         = (mediamark_t *) xine_xmalloc(sizeof(mediamark_t));
  mmk->mrl    = NULL;
  mmk->ident  = NULL;
  mmk->sub    = NULL;
  mmk->start  = 0;
  mmk->end    = -1;
  mmk->played = 0;

  return mmk;
}
static mediamark_t *smil_duplicate_mmk(mediamark_t *ommk) {
  mediamark_t *mmk = NULL;
  
  if(ommk) {
    mmk         = smil_new_mediamark();
    mmk->mrl    = strdup(ommk->mrl);
    mmk->ident  = strdup(ommk->ident ? ommk->ident : ommk->mrl);
    mmk->sub    = ommk->sub ? strdup(ommk->sub) : NULL;
    mmk->start  = ommk->start;
    mmk->end    = ommk->end;
    mmk->played = ommk->played;
  }
  
  return mmk;
}
static void smil_free_properties(smil_property_t *smil_props) {
  if(smil_props) {
    if(smil_props->anchor)
      free(smil_props->anchor);
  }
}

#ifdef DEBUG_SMIL
static void smil_dump_header(smil_t *smil) {
  printf("title: %s\n", smil->title);
  printf("author: %s\n", smil->author);
  printf("copyright: %s\n", smil->copyright);
  printf("year: %s\n", smil->year);
  printf("base: %s\n", smil->base);
}

static void smil_dump_tree(smil_node_t *node) {
  if(node->mmk) {
    printf("mrl:   %s\n", node->mmk->mrl);
    printf(" ident: %s\n", node->mmk->ident);
    printf("  sub:   %s\n", node->mmk->sub);
    printf("   start: %d\n", node->mmk->start);
    printf("    end:   %d\n", node->mmk->end);
  }

  if(node->next)
    smil_dump_tree(node->next);
}
#endif

static char *smil_get_prop_value(xml_property_t *props, const char *propname) {
  xml_property_t  *prop;

  for(prop = props; prop; prop = prop->next) {
    if(!strcasecmp(prop->name, propname)) {
      return prop->value;
    }
  }
  return NULL;
}

static int smil_get_time_in_seconds(const char *time_str) {
  int    retval = 0;
  int    hours, mins, secs, msecs;
  int    unit = 0; /* 1 = ms, 2 = s, 3 = min, 4 = h */

  if(strstr(time_str, "ms"))
    unit = 1;
  else if(strstr(time_str, "s"))
    unit = 2;
  else if(strstr(time_str, "min"))
    unit = 3;
  else if(strstr(time_str, "h"))
    unit = 4;
  else
    unit = 2;
  
  if(unit == 0) {
    if((sscanf(time_str, "%d:%d:%d.%d", &hours, &mins, &secs, &msecs)) == 4)
      retval = (hours * 60 * 60) + (mins * 60) + secs + ((int) msecs / 1000);
    else if((sscanf(time_str, "%d:%d:%d", &hours, &mins, &secs)) == 3)
      retval = (hours * 60 * 60) + (mins * 60) + secs;
    else if((sscanf(time_str, "%d:%d.%d", &mins, &secs, &msecs)) == 3)
      retval = (mins * 60) + secs + ((int) msecs / 1000);
    else if((sscanf(time_str, "%d:%d", &mins, &secs)) == 2) {
      retval = (mins * 60) + secs;
    }
  }
  else {
    int val, dec, args;

    if(((args = sscanf(time_str, "%d.%d", &val, &dec)) == 2) ||
       ((args = sscanf(time_str, "%d", &val)) == 1)) {
      switch(unit) {
      case 1: /* ms */
	retval = (int) val / 1000;
	break;
      case 2: /* s */
	retval = val + ((args == 2) ? ((int)((((float) dec) * 10) / 1000)) : 0);
	break;
      case 3: /* min */
	retval = (val * 60) + ((args == 2) ? (dec * 10 * 60 / 100) : 0);
	break;
      case 4: /* h */
	retval = (val * 60 * 60) + ((args == 2) ? (dec * 10 * 60 / 100) : 0);
	break;
      }
    }
  }

  return retval;
}
static void smil_header(smil_t *smil, xml_property_t *props) {
  xml_property_t  *prop;

#ifdef DEBUG_SMIL
  offset += 2;
#endif

  for(prop = props; prop; prop = prop->next) {
#ifdef DEBUG_SMIL
    palign;
    printf("%s(): prop_name '%s', value: '%s'\n", __func__,
    	   prop->name, prop->value ? prop->value : "<NULL>");
#endif
    
    if(!strcasecmp(prop->name, "NAME")) {
      
      if(!strcasecmp(prop->value, "TITLE"))
	smil->title = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "AUTHOR"))
	smil->author = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "COPYRIGHT"))
	smil->copyright = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "YEAR"))
	smil->year = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "BASE"))
	smil->base = smil_get_prop_value(prop, "CONTENT");

    }
  }
#ifdef DEBUG_SMIL
  palign;
  printf("---\n");
  offset -= 2;
#endif
}

static void smil_get_properties(smil_property_t *dprop, xml_property_t *props) {
  xml_property_t  *prop;
  
#ifdef DEBUG_SMIL
  offset += 2;
#endif
  for(prop = props; prop; prop = prop->next) {

    if(!strcasecmp(prop->name, "REPEAT")) {
      dprop->repeat = strtol(prop->value, &prop->value, 10);
#ifdef DEBUG_SMIL
      palign;
      printf("REPEAT: %d\n", dprop->repeat);
#endif
    }
    else if(!strcasecmp(prop->name, "BEGIN")) {
      dprop->begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("BEGIN: %d\n", dprop->begin);
#endif
    }
    else if((!strcasecmp(prop->name, "CLIP-BEGIN")) || (!strcasecmp(prop->name, "CLIPBEGIN"))) { 
      dprop->clip_begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("CLIP-BEGIN: %d\n", dprop->clip_begin);
#endif
    }
    else if(!strcasecmp(prop->name, "END")) {
      dprop->end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("END: %d\n", dprop->end);
#endif
    }
    else if((!strcasecmp(prop->name, "CLIP-END")) || (!strcasecmp(prop->name, "CLIPEND"))) { 
      dprop->clip_end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("CLIP-END: %d\n", dprop->clip_end);
#endif
    }
    else if(!strcasecmp(prop->name, "DUR")) {
      dprop->duration = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("DURATION: %d\n", dprop->duration);
#endif
    }
    else if(!strcasecmp(prop->name, "HREF")) {
      
      if(dprop->anchor)
	free(dprop->anchor);
      
      dprop->anchor = strdup(prop->value);
      
#ifdef DEBUG_SMIL
      palign;
      printf("HREF: %s\n", dprop->anchor);
#endif
      
    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}

static void smil_properties(smil_t *smil, smil_node_t **snode, 
			    xml_property_t *props, smil_property_t *sprop) {
  xml_property_t  *prop;
  int              gotmrl = 0;

#ifdef DEBUG_SMIL
  offset += 2;
#endif
  for(prop = props; prop; prop = prop->next) {

#ifdef DEBUG_SMIL
    palign;
    printf("%s(): prop_name '%s', value: '%s'\n", __func__,
	   prop->name, prop->value ? prop->value : "<NULL>");
#endif
    
    if(prop->name) {
      if((!strcasecmp(prop->name, "SRC")) || (!strcasecmp(prop->name, "HREF"))) {
	
	gotmrl++;
	
	if((*snode)->mmk == NULL)
	  (*snode)->mmk = smil_new_mediamark();
	
	/*
	  handle BASE, ANCHOR
	*/
	if(sprop && sprop->anchor)
	  (*snode)->mmk->mrl = strdup(sprop->anchor);
	else {
	  char  buffer[((smil->base) ? (strlen(smil->base) + 1) : 0) + strlen(prop->value) + 1];
	  char *p;

	  memset(&buffer, 0, sizeof(buffer));
	  p = buffer;

	  if(smil->base && (!strstr(prop->value, "://"))) {
	    strcat(p, smil->base);
	    
	    if(buffer[strlen(buffer) - 1] != '/')
	      strcat(p, "/");
	  }
	  
	  strcat(p, prop->value);
	  
	  (*snode)->mmk->mrl = strdup(buffer);
	}
      }
      else if(!strcasecmp(prop->name, "TITLE")) {
	
	gotmrl++;
	
	if((*snode)->mmk == NULL) {
	  (*snode)->mmk = smil_new_mediamark();
	}
	
	(*snode)->mmk->ident = strdup(prop->value);
      }
      else if(!strcasecmp(prop->name, "REPEAT")) {
	(*snode)->prop.repeat = strtol(prop->value, &prop->value, 10);
      }
      else if(!strcasecmp(prop->name, "BEGIN")) {
	(*snode)->prop.begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("begin: %d\n",(*snode)->prop.begin);
#endif
      }
      else if((!strcasecmp(prop->name, "CLIP-BEGIN")) || (!strcasecmp(prop->name, "CLIPBEGIN"))) { 
	(*snode)->prop.clip_begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("clip-begin: %d\n",(*snode)->prop.clip_begin);
#endif
      }
      else if(!strcasecmp(prop->name, "END")) {
	(*snode)->prop.end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("end: %d\n",(*snode)->prop.end);
#endif
      }
      else if((!strcasecmp(prop->name, "CLIP-END")) || (!strcasecmp(prop->name, "CLIPEND"))) { 
	(*snode)->prop.clip_end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("clip-end: %d\n",(*snode)->prop.clip_end);
#endif
      }
      else if(!strcasecmp(prop->name, "DUR")) {
	(*snode)->prop.duration = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	palign;
	printf("DURATION: %d\n", (*snode)->prop.duration);
#endif
      }
    }
  }

  if(gotmrl) {
    smil_node_t *node = (*snode);
    int          repeat = node->prop.repeat;

    if((*snode)->mmk->ident == NULL)
      (*snode)->mmk->ident = strdup((*snode)->mmk->mrl);
    
    if(sprop) {
      if(sprop->clip_begin && (node->prop.clip_begin == 0))
	node->mmk->start = sprop->clip_begin;
      else
	node->mmk->start = node->prop.clip_begin;
      
      if(sprop->clip_end && (node->prop.clip_end == -1))
	node->mmk->end = sprop->clip_end;
      else
	node->mmk->end = node->prop.clip_end;
      
      if(sprop->duration && (node->prop.duration == 0))
      	node->mmk->end = node->mmk->start + sprop->duration;
      else
      	node->mmk->end = node->mmk->start + node->prop.duration;
      
    }
    else {
      node->mmk->start = node->prop.clip_begin;
      node->mmk->end = node->prop.clip_end;
      if(node->prop.duration)
	node->mmk->end = node->mmk->start + node->prop.duration;
    }
    
    smil_repeat((repeat <= 1) ? 1 : repeat, node, snode, sprop);
  }

#ifdef DEBUG_SMIL
  palign;
  printf("---\n");
  offset -= 2;
#endif
}

static void smil_repeat(int times, smil_node_t *from, 
			smil_node_t **snode, smil_property_t *sprop) {
  int  i;
  
  if(times > 1) {
    smil_node_t *node, *to;
    
    to = (*snode);
    
    for(i = 0; i < (times - 1); i++) {
      int found = 0;

      node = from;
      
      while(!found) {
	smil_node_t *nnode = smil_new_node();
	
	nnode->mmk  = smil_duplicate_mmk(node->mmk);
	
	(*snode)->next = nnode;
	(*snode) = nnode;
	
	if(node == to)
	  found = 1;
	
	node = node->next;
      }
    }
  }
  else {
    smil_node_t *nnode = smil_new_node();

    (*snode)->next = nnode;
    (*snode) = (*snode)->next;
  }
}
/*
 * Sequence playback
 */
static void smil_seq(smil_t *smil, 
		     smil_node_t **snode, xml_node_t *node, smil_property_t *sprop) { 
  xml_node_t      *seq;
  
#ifdef DEBUG_SMIL
  offset += 2;
#endif
  
  for(seq = node; seq; seq = seq->next) {
    
    if(!strcasecmp(seq->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);
      
#ifdef DEBUG_SMIL
      offset += 2;
      palign; 
      printf("seq NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_seq(smil, snode, seq->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(seq->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);
      
#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("seq NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_par(smil, snode, seq->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(seq->name, "SWITCH")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);
      
#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("seq NEW SWITCH\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_switch(smil, snode, seq->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(seq->name, "VIDEO")) || 
	    (!strcasecmp(seq->name, "AUDIO")) ||
	    (!strcasecmp(seq->name, "A"))) {
      smil_property_t  smil_props;
      
      smil_init_smil_property(&smil_props);
      
      if(seq->child) {
	xml_node_t *child = seq->child;
	
	while(child) {

	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }

	  child = child->next;
	}
      }
      
      smil_properties(smil, snode, seq->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}
/*
 * Parallel playback
 */
static void smil_par(smil_t *smil, smil_node_t **snode, xml_node_t *node, smil_property_t *sprop) {
  xml_node_t      *par;
   
#ifdef DEBUG_SMIL
  offset += 2;
#endif
  
  for(par = node; par; par = par->next) {
    
    if(!strcasecmp(par->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign; 
      printf("par NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_seq(smil, snode, par->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(par->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("par NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_par(smil, snode, par->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(par->name, "SWITCH")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("par NEW SWITCH\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_switch(smil, snode, par->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(par->name, "VIDEO")) || 
	    (!strcasecmp(par->name, "AUDIO")) ||
	    (!strcasecmp(par->name, "A"))) {
      smil_property_t  smil_props;

      smil_init_smil_property(&smil_props);
      
      if(par->child) {
	xml_node_t *child = par->child;

	while(child) {
	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }
	  child = child->next;
	}
      }

      smil_properties(smil, snode, par->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }

#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}
static void smil_switch(smil_t *smil, smil_node_t **snode, xml_node_t *node, smil_property_t *sprop) { 
  xml_node_t      *nswitch;

#ifdef DEBUG_SMIL
  offset += 2;
#endif
  
  for(nswitch = node; nswitch; nswitch = nswitch->next) {
    
    if(!strcasecmp(nswitch->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);
      
#ifdef DEBUG_SMIL
      offset += 2;
      palign; 
      printf("switch NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, nswitch->props);
      smil_seq(smil, snode, nswitch->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(nswitch->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);
      
#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("switch NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, nswitch->props);
      smil_par(smil, snode, nswitch->child, &smil_props);
      smil_repeat(smil_props.repeat, node, snode, &smil_props);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(nswitch->name, "VIDEO")) || 
	    (!strcasecmp(nswitch->name, "AUDIO")) ||
	    (!strcasecmp(nswitch->name, "A"))) {
      smil_property_t  smil_props;
      
      smil_init_smil_property(&smil_props);
      
      if(nswitch->child) {
	xml_node_t *child = nswitch->child;
	
	while(child) {

	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }

	  child = child->next;
	}
      }
      
      smil_properties(smil, snode, nswitch->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}

static void smil_add_mmk(int *num, mediamark_t ***mmk, smil_node_t *node) {
  if(node->mmk) {
    if(*num == 0)
      (*mmk) = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
    else
      (*mmk) = (mediamark_t **) realloc((*mmk), sizeof(mediamark_t *) * (*num + 2));

    (*mmk)[*num] = node->mmk;
    *num += 1;
  }
  
  if(node->next)
    smil_add_mmk(num, mmk, node->next);
}
static int smil_fill_mmk(smil_t *smil, mediamark_t ***mmk) {
  int num = 0;
  
  if(smil->first)
    smil_add_mmk(&num, mmk, smil->first);
  
  return num;
}

static void smil_free_node(smil_node_t *node) {
  if(node->next)
    smil_free_node(node->next);

  /* mmk shouldn't be fried */
  smil_free_properties(&(node->prop));
  free(node);
}
static void smil_free_smil(smil_t *smil) {
  if(smil->first)
    smil_free_node(smil->first);

  if(smil->title)
    free(smil->title);
  if(smil->author)
    free(smil->author);
  if(smil->copyright)
    free(smil->copyright);
  if(smil->year)
    free(smil->year);
  if(smil->base)
    free(smil->base);
}

static mediamark_t **guess_smil_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    char            *smil_content;
    int              size;
    int              result;
    xml_node_t      *xml_tree, *smil_entry, *smil_ref;
    smil_t           smil;
    
    if((smil_content = _read_file(filename, &size)) != NULL) {
      int entries_smil = 0;

      xml_parser_init(smil_content, size, XML_PARSER_CASE_INSENSITIVE);
      if((result = xml_parser_build_tree(&xml_tree)) != XML_PARSER_OK)
	goto __failure;
      
      memset(&smil, 0, sizeof(smil_t));

      if(!strcasecmp(xml_tree->name, "SMIL")) {

	smil.first = smil.node = smil_new_node();
	smil_entry = xml_tree->child;

	while(smil_entry) {
	  
#ifdef DEBUG_SMIL
	  printf("smil_entry '%s'\n", smil_entry->name);
#endif	  
	  if(!strcasecmp(smil_entry->name, "HEAD")) {
#ifdef DEBUG_SMIL
	    printf("+---------+\n");
	    printf("| IN HEAD |\n");
	    printf("+---------+\n");
#endif	    
	    smil_ref = smil_entry->child;
	    while(smil_ref) {
#ifdef DEBUG_SMIL
	      printf("  smil_ref '%s'\n", smil_ref->name);
#endif	      
	      smil_header(&smil, smil_ref->props);
	      
	      smil_ref = smil_ref->next;
	    }
	  }
	  else if(!strcasecmp(smil_entry->name, "BODY")) {
#ifdef DEBUG_SMIL
	    printf("+---------+\n");
	    printf("| IN BODY |\n");
	    printf("+---------+\n");
#endif
	    __kino:
	    smil_ref = smil_entry->child;
	    while(smil_ref) {
#ifdef DEBUG_SMIL
	      printf("  smil_ref '%s'\n", smil_ref->name);
#endif
	      if(!strcasecmp(smil_ref->name, "SEQ")) {
		smil_property_t  smil_props;
		smil_node_t     *node = smil.node;
		
#ifdef DEBUG_SMIL
		palign;
		printf("SEQ Found\n");
#endif		
		smil_init_smil_property(&smil_props);

		smil_get_properties(&smil_props, smil_ref->props);
		smil_seq(&smil, &(smil.node), smil_ref->child, &smil_props);
		smil_repeat(smil_props.repeat, node, &(smil.node), &smil_props);
		smil_free_properties(&smil_props);
	      }
	      else if(!strcasecmp(smil_ref->name, "PAR")) {
		smil_property_t  smil_props;
		smil_node_t     *node = smil.node;
	
#ifdef DEBUG_SMIL
		palign;
		printf("PAR Found\n");
#endif
		
		smil_init_smil_property(&smil_props);

		smil_get_properties(&smil_props, smil_ref->props);
		smil_par(&smil, &(smil.node), smil_ref->child, &smil_props);
		smil_repeat(smil_props.repeat, node, &(smil.node), &smil_props);
		smil_free_properties(&smil_props);
	      }
	      else if(!strcasecmp(smil_ref->name, "A")) {
#ifdef DEBUG_SMIL
		palign;
		printf("  IN A\n");
#endif
		smil_properties(&smil, &(smil.node), smil_ref->props, NULL);
	      }
	      else if(!strcasecmp(smil_ref->name, "SWITCH")) {
		smil_property_t  smil_props;
		smil_node_t     *node = smil.node;
	
#ifdef DEBUG_SMIL
		palign;
		printf("SWITCH Found\n");
#endif
		
		smil_init_smil_property(&smil_props);
		
		smil_get_properties(&smil_props, smil_ref->props);
		smil_switch(&smil, &(smil.node), smil_ref->child, &smil_props);
		smil_repeat(smil_props.repeat, node, &(smil.node), &smil_props);
		smil_free_properties(&smil_props);
	      }
	      else {
		smil_properties(&smil, &smil.node, smil_ref->props, NULL);
	      }

	      smil_ref = smil_ref->next;
	    }
	  }
	  else if(!strcasecmp(smil_entry->name, "SEQ")) { /* smil2, kino format(no body) */
	    goto __kino;
	  }
	  
	  smil_entry = smil_entry->next;
	}
	
#ifdef DEBUG_SMIL
	printf("DUMPING TREE:\n");
	printf("-------------\n");
	smil_dump_tree(smil.first);
	
	printf("DUMPING HEADER:\n");
	printf("---------------\n");
	smil_dump_header(&smil);
#endif

	entries_smil = smil_fill_mmk(&smil, &mmk);
	smil_free_smil(&smil);
      }
      else
	fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __func__, xml_tree->name);
      
      xml_parser_free_tree(xml_tree);
      
    __failure:
      free(smil_content);
      
      if(entries_smil) {
	mmk[entries_smil] = NULL;
	playlist->entries = entries_smil;
	playlist->type    = strdup("SMIL");
	return mmk;
      }
    }
  }
  
  return NULL;
}
/*
 * ********************************** SMIL END ***********************************
 */

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

void mediamark_add_entry(const char *mrl, const char *ident, 
			 const char *sub, int start, int end, int offset) {
  if(!gGui->playlist.num)
    gGui->playlist.mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
  else { 
    if(gGui->playlist.num > 1) {
      gGui->playlist.mmk = (mediamark_t **) 
	realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 1));
    }
  }
  
  if(mediamark_store_mmk(&gGui->playlist.mmk[gGui->playlist.num], 
			 mrl, ident, sub, start, end, offset))
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

void mediamark_reset_played_state(void) {
  
  if(gGui->playlist.num) {
    int i;
    
    for(i = 0; i < gGui->playlist.num; i++)
      gGui->playlist.mmk[i]->played = 0;
  }
}

int mediamark_all_played(void) {
  
  if(gGui->playlist.num) {
    int i;
    
    for(i = 0; i < gGui->playlist.num; i++) {
      if(gGui->playlist.mmk[i]->played == 0)
	return 0;
    }
  }

  return 1;
}

int mediamark_get_shuffle_next(void) {
  int  next = 0;
  
  if(gGui->playlist.num >= 3) {
    int    remain = gGui->playlist.num;
    int    entries[remain];
    float  num = (float) gGui->playlist.num;
    int    found = 0;
    
    memset(&entries, 0, sizeof(int) * remain);

    srandom((unsigned int)time(NULL));
    entries[gGui->playlist.cur] = 1;

    do {
      next = (int) (num * random() / RAND_MAX);

      if(next != gGui->playlist.cur) {

	if(gGui->playlist.mmk[next]->played == 0)
	  found = 1;
	else if(entries[next] == 0) {
	  entries[next] = 1;
	  remain--;
	}

	if((!found) && (!remain))
	  found = 2; /* No more choice */

      }

    } while(!found);

    if(found == 2)
      next = gGui->playlist.cur;

  }
  else if(gGui->playlist.num)
    next = !gGui->playlist.cur;
  
  return next;
}

void mediamark_replace_entry(mediamark_t **mmk, 
			     const char *mrl, const char *ident, const char *sub,
			     int start, int end, int offset) {
  SAFE_FREE((*mmk)->mrl);
  SAFE_FREE((*mmk)->ident);
  SAFE_FREE((*mmk)->sub);
  (*mmk)->start  = 0;
  (*mmk)->end    = -1;
  (*mmk)->offset = 0;

  (void) mediamark_store_mmk(mmk, mrl, ident, sub, start, end, offset);
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

const char *mediamark_get_current_sub(void) {

  if(gGui->playlist.mmk && gGui->playlist.num)
    return gGui->playlist.mmk[gGui->playlist.cur]->sub;

  return NULL;
}

void mediamark_free_entry(int offset) {

  if(gGui->playlist.num && (offset < gGui->playlist.num)) {
    if(mediamark_free_mmk(&gGui->playlist.mmk[offset]))
      gGui->playlist.num--;
  }
}

int mediamark_concat_mediamarks(const char *_filename) {
  playlist_t             *playlist;
  int                     i, found;
  mediamark_t           **mmk = NULL;
  const char             *filename = _filename;
  playlist_guess_func_t   guess_functions[] = {
    guess_asx_playlist,
    guess_gx_playlist,
    guess_smil_playlist,
    guess_toxine_playlist,
    guess_pls_playlist,
    guess_m3u_playlist,
    guess_sfv_playlist,
    guess_raw_playlist,
    NULL
  };

  if(_filename) {
    if(!strncasecmp("file:/", _filename, 6))
      filename = (_filename + 6);
  }

  playlist = (playlist_t *) xine_xmalloc(sizeof(playlist_t));

  for(i = 0, found = 0; (guess_functions[i] != NULL) && (found == 0); i++) {
    if((mmk = guess_functions[i](playlist, filename)) != NULL)
      found = 1;
  }
  
  if(found) {
#ifdef DEBUG
    printf("Playlist file (%s) is valid (%s).\n", filename, playlist->type);
#endif
  }
  else {
    fprintf(stderr, _("Playlist file (%s) is invalid.\n"), filename);
    SAFE_FREE(playlist);
    return 0;
  }

  gGui->playlist.cur = gGui->playlist.num;

  for(i = 0; i < playlist->entries; i++)
    mediamark_add_entry(mmk[i]->mrl, mmk[i]->ident, mmk[i]->sub, 
			mmk[i]->start, mmk[i]->end, mmk[i]->offset);
  
  for(i = 0; i < playlist->entries; i++)
    (void) mediamark_free_mmk(&mmk[i]);
  
  SAFE_FREE(mmk);
  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);

  return 1;
}

void mediamark_load_mediamarks(const char *_filename) {
  playlist_t             *playlist;
  int                     i, found, onum;
  mediamark_t           **mmk = NULL;
  mediamark_t           **ommk;
  const char             *filename = _filename;
  playlist_guess_func_t   guess_functions[] = {
    guess_asx_playlist,
    guess_gx_playlist,
    guess_smil_playlist,
    guess_toxine_playlist,
    guess_pls_playlist,
    guess_m3u_playlist,
    guess_sfv_playlist,
    guess_raw_playlist,
    NULL
  };

  if(_filename) {
    if(!strncasecmp("file:/", _filename, 6))
      filename = (_filename + 6);
  }

  playlist = (playlist_t *) xine_xmalloc(sizeof(playlist_t));

  for(i = 0, found = 0; (guess_functions[i] != NULL) && (found == 0); i++) {
    if((mmk = guess_functions[i](playlist, filename)) != NULL)
      found = 1;
  }
  
  if(found) {
#ifdef DEBUG
    printf("Playlist file (%s) is valid (%s).\n", filename, playlist->type);
#endif
  }
  else {
    fprintf(stderr, _("Playlist file (%s) is invalid.\n"), filename);
    SAFE_FREE(playlist);
    return;
  }

  ommk = gGui->playlist.mmk;
  onum = gGui->playlist.num;
  
  gGui->playlist.mmk = mmk;
  gGui->playlist.num = playlist->entries;

  if(gGui->playlist.loop == PLAYLIST_LOOP_SHUFFLE)
    gGui->playlist.cur = mediamark_get_shuffle_next();
  else
    gGui->playlist.cur = 0;

  for(i = 0; i < onum; i++)
    (void) mediamark_free_mmk(&ommk[i]);

  SAFE_FREE(ommk);

  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);
}

void mediamark_save_mediamarks(const char *filename) {
  char *fullfn;
  char *pn;
  char *fn;
  int   status = 1;

  if(!gGui->playlist.num)
    return;

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
	fprintf(fd, "\tsubtitle = %s;\n", gGui->playlist.mmk[i]->sub);
	fprintf(fd, "\tstart = %d;\n", gGui->playlist.mmk[i]->start);
	fprintf(fd, "\tend = %d;\n", gGui->playlist.mmk[i]->end);
	fprintf(fd, "\toffset = %d;\n", gGui->playlist.mmk[i]->offset);
	fprintf(fd,"};\n");
      }

      fprintf(fd, "# END\n");
      
      fclose(fd);
#ifdef DEBUG
      printf("Playlist '%s' saved.\n", filename);
#endif
    }
    else
      fprintf(stderr, _("Unable to save playlist (%s): %s.\n"), filename, strerror(errno));

  }
}

int mrl_looks_playlist(char *mrl) {
  char *extension;
  
  extension = strrchr(mrl, '.');
  if(extension && /* All known playlist ending */
     ((!strncasecmp(extension, ".asx", 4))  ||
      (!strncasecmp(extension, ".smi", 4))  ||
      (!strncasecmp(extension, ".smil", 5)) ||
      (!strncasecmp(extension, ".pls", 4))  ||
      (!strncasecmp(extension, ".m3u", 4))  ||
      (!strncasecmp(extension, ".sfv", 4))  ||
      (!strncasecmp(extension, ".tox", 4)))) {
    return 1;
  }
  
 return 0;
}

/*
 *  EDITOR
 */
static void mmkeditor_exit(xitk_widget_t *w, void *data) {

  if(mmkeditor) {
    window_info_t wi;

    mmkeditor->running = 0;
    mmkeditor->visible = 0;
    
    if((xitk_get_window_info(mmkeditor->widget_key, &wi))) {
      config_update_num ("gui.mmk_editor_x", wi.x);
      config_update_num ("gui.mmk_editor_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    mmkeditor->mmk = NULL;
    
    xitk_unregister_event_handler(&mmkeditor->widget_key);
    
    xitk_destroy_widgets(mmkeditor->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, mmkeditor->xwin);
    
    mmkeditor->xwin = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(mmkeditor->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(mmkeditor->widget_list);
    
    free(mmkeditor);
    mmkeditor = NULL;
  }
}

int mmk_editor_is_visible(void) {
  
  if(mmkeditor != NULL)
    return mmkeditor->visible;
  
  return 0;
}

int mmk_editor_is_running(void) {
  
  if(mmkeditor != NULL)
    return mmkeditor->running;
  
  return 0;
}

void mmk_editor_toggle_visibility(void) {
  if(mmkeditor != NULL) {
    if (mmkeditor->visible && mmkeditor->running) {
      XLockDisplay(gGui->display);
      if(gGui->use_root_window) {
	if(xitk_is_window_visible(gGui->display, xitk_window_get_window(mmkeditor->xwin)))
	  XIconifyWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin), gGui->screen);
	else
	  XMapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
      }
      else {
	mmkeditor->visible = 0;
	xitk_hide_widgets(mmkeditor->widget_list);
	XUnmapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
      }
      XUnlockDisplay(gGui->display);
    } else {
      if(mmkeditor->running) {
	mmkeditor->visible = 1;
	xitk_show_widgets(mmkeditor->widget_list);
	XLockDisplay(gGui->display);
	XRaiseWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin)); 
	XMapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin)); 
	if(!gGui->use_root_window)
	  XSetTransientForHint(gGui->display, 
			       xitk_window_get_window(mmkeditor->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(mmkeditor->xwin));
      }
    }
  }
}

void mmk_editor_raise_window(void) {
  if(mmkeditor != NULL) {
    if(mmkeditor->xwin) {
      if(mmkeditor->visible && mmkeditor->running) {
	XLockDisplay(gGui->display);
	XUnmapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
	XRaiseWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
	XMapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
	if(!gGui->use_root_window)	
	  XSetTransientForHint(gGui->display, 
			       xitk_window_get_window(mmkeditor->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(mmkeditor->xwin));
      }
    }
  }
}

void mmk_editor_end(void) {
  mmkeditor_exit(NULL, NULL);
}

void mmkeditor_set_mmk(mediamark_t **mmk) {

  if(mmkeditor) {
    mmkeditor->mmk = mmk;
    
    xitk_inputtext_change_text(mmkeditor->mrl, (*mmk)->mrl);
    xitk_inputtext_change_text(mmkeditor->ident, (*mmk)->ident);
    xitk_inputtext_change_text(mmkeditor->sub, (*mmk)->sub);
    xitk_intbox_set_value(mmkeditor->start, (*mmk)->start);
    xitk_intbox_set_value(mmkeditor->end, (*mmk)->end);
    xitk_intbox_set_value(mmkeditor->offset, (*mmk)->offset);
  }
}

static void mmkeditor_apply(xitk_widget_t *w, void *data) {
  const char *ident, *mrl, *sub;
  int         start, end, offset;

  if(mmkeditor->mmk) {
    
    mrl    = xitk_inputtext_get_text(mmkeditor->mrl); 
    ident  = xitk_inputtext_get_text(mmkeditor->ident);
    sub    = xitk_inputtext_get_text(mmkeditor->sub);
    start  = xitk_intbox_get_value(mmkeditor->start);
    end    = xitk_intbox_get_value(mmkeditor->end);
    offset = xitk_intbox_get_value(mmkeditor->offset);

    if(start < 0)
      start = 0;

    if(end < -1)
      end = -1;

    mediamark_replace_entry(mmkeditor->mmk, mrl, ident, sub, start, end, offset);

    if(mmkeditor->callback)
      mmkeditor->callback(mmkeditor->user_data);
      
  }
  
}

static void mmk_fileselector_cancel_callback(filebrowser_t *fb) {
  sprintf(gGui->curdir, "%s", (filebrowser_get_current_dir(fb)));
}
static void mmk_fileselector_callback(filebrowser_t *fb) {
  char *file;

  sprintf(gGui->curdir, "%s", (filebrowser_get_current_dir(fb)));

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    if(file)
      xitk_inputtext_change_text(mmkeditor->sub, file);
    free(file);
  }
  
}
static void mmkeditor_select_sub(xitk_widget_t *w, void *data) {
  filebrowser_callback_button_t  cbb[2];
  
  cbb[0].label = _("Select");
  cbb[0].callback = mmk_fileselector_callback;
  cbb[0].need_a_file = 1;
  cbb[1].callback = mmk_fileselector_cancel_callback;
  cbb[1].need_a_file = 0;
  (void *) create_filebrowser(_("Pick a subtitle file"), gGui->curdir, &cbb[0], NULL, &cbb[1]);
}

void mmk_edit_mediamark(mediamark_t **mmk, apply_callback_t callback, void *data) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_inputtext_widget_t     inp;
  xitk_intbox_widget_t        ib;
  xitk_pixmap_t              *bg;
  xitk_widget_t              *b;
  int                         x, y, w, width, height;

  if(mmkeditor) {
    if(!mmkeditor->visible)
      mmkeditor->visible = !mmkeditor->visible;
    mmk_editor_raise_window();
    mmkeditor_set_mmk(mmk);
    return;
  }

  mmkeditor = (_mmk_editor_t *) xine_xmalloc(sizeof(_mmk_editor_t));
  
  mmkeditor->callback = callback;
  mmkeditor->user_data = data;

  x = xine_config_register_num(gGui->xine, "gui.mmk_editor_x", 
			       100,
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  y = xine_config_register_num(gGui->xine, "gui.mmk_editor_y",
			       100,
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  
  /* Create window */
  mmkeditor->xwin = xitk_window_create_dialog_window(gGui->imlib_data, _("Mediamark Editor"), x, y,
						     WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(mmkeditor->xwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  mmkeditor->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(mmkeditor->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(mmkeditor->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(mmkeditor->xwin)));
  xitk_widget_list_set(mmkeditor->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  XITK_WIDGET_INIT(&ib, gGui->imlib_data);

  xitk_window_get_window_size(mmkeditor->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(mmkeditor->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay (gGui->display);

  x = 5;
  y = 35;
  draw_outter_frame(gGui->imlib_data, bg, _("Identifier"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, 20 + 15 + 10);

  x = 15;
  y += 5;
  w = WINDOW_WIDTH - 30;
  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	   (mmkeditor->ident = 
	    xitk_noskin_inputtext_create(mmkeditor->widget_list, &inp,
					 x, y, w, 20,
					 "Black", "Black", fontname)));
  xitk_set_widget_tips_default(mmkeditor->ident, _("Mediamark Identifier"));
  xitk_enable_and_show_widget(mmkeditor->ident);

  x = 5;
  y += 40;
  draw_outter_frame(gGui->imlib_data, bg, _("Mrl"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, 20 + 15 + 10);

  x = 15;
  y += 5;
  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	    (mmkeditor->mrl = 
	     xitk_noskin_inputtext_create(mmkeditor->widget_list, &inp,
					  x, y, w, 20,
					  "Black", "Black", fontname)));
  xitk_set_widget_tips_default(mmkeditor->mrl, _("Mediamark Mrl"));
  xitk_enable_and_show_widget(mmkeditor->mrl);

  x = 5;
  y += 40;
  draw_outter_frame(gGui->imlib_data, bg, _("Subtitle"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, 20 + 15 + 10);

  x = 15;
  y += 5;
  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	    (mmkeditor->sub = 
	     xitk_noskin_inputtext_create(mmkeditor->widget_list, &inp,
					  x, y, w - 115, 20,
					  "Black", "Black", fontname)));
  xitk_set_widget_tips_default(mmkeditor->mrl, _("Subtitle File"));
  xitk_enable_and_show_widget(mmkeditor->sub);

  x = WINDOW_WIDTH - 115;
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Sub File");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_select_sub; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)), 
	   (b = xitk_noskin_labelbutton_create(mmkeditor->widget_list, 
					       &lb, x, y - 2, 100, 23,
					       "Black", "Black", "White", btnfontname)));
  xitk_set_widget_tips_default(b, _("Select a subtitle file to use together with the mrl."));
  xitk_enable_and_show_widget(b);

  x = 5;
  y += 40;
  w = 60;
  draw_outter_frame(gGui->imlib_data, bg, _("Start at"), btnfontname, 
		    x, y, w + 60, 20 + 15 + 10);

  x = 35;
  y += 5;
  ib.skin_element_name = NULL;
  ib.value             = 0;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor->widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	    (mmkeditor->start = 
	     xitk_noskin_intbox_create(mmkeditor->widget_list, &ib, 
				       x, y, w, 20, NULL, NULL, NULL)));
  xitk_set_widget_tips_default(mmkeditor->start, _("Mediamark start time (secs)."));
  xitk_enable_and_show_widget(mmkeditor->start);

  x += w + 20 + 15;
  y -= 5;
  draw_outter_frame(gGui->imlib_data, bg, _("End at"), btnfontname, 
		    x, y, w + 60, 20 + 15 + 10);

  x += 30;
  y += 5;
  ib.skin_element_name = NULL;
  ib.value             = -1;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor->widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	    (mmkeditor->end = 
	     xitk_noskin_intbox_create(mmkeditor->widget_list, &ib, 
				       x, y, w, 20, NULL, NULL, NULL)));
  xitk_set_widget_tips_default(mmkeditor->end, _("Mediamark end time (secs)."));
  xitk_enable_and_show_widget(mmkeditor->end);

  x += w + 20 + 15;
  y -= 5;
  draw_outter_frame(gGui->imlib_data, bg, _("A/V offset"), btnfontname, 
		    x, y, w + 60, 20 + 15 + 10);

  x += 30;
  y += 5;
  ib.skin_element_name = NULL;
  ib.value             = 0;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor->widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)),
	    (mmkeditor->offset = 
	     xitk_noskin_intbox_create(mmkeditor->widget_list, &ib, 
				       x, y, w, 20, NULL, NULL, NULL)));
  xitk_set_widget_tips_default(mmkeditor->end, _("Mediamark end time (secs)."));
  xitk_enable_and_show_widget(mmkeditor->offset);

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_apply; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)), 
	   (b = xitk_noskin_labelbutton_create(mmkeditor->widget_list, 
					       &lb, x, y, 100, 23,
					       "Black", "Black", "White", btnfontname)));
  xitk_set_widget_tips_default(b, _("Apply the changes to the playlist."));
  xitk_enable_and_show_widget(b);

  x = WINDOW_WIDTH - 115;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(mmkeditor->widget_list)), 
	   (b = xitk_noskin_labelbutton_create(mmkeditor->widget_list, 
					       &lb, x, y, 100, 23,
					       "Black", "Black", "White", btnfontname)));
  xitk_set_widget_tips_default(b, _("Discard changes and dismiss the window."));
  xitk_enable_and_show_widget(b);

  xitk_window_change_background(gGui->imlib_data, mmkeditor->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);

  mmkeditor->widget_key = xitk_register_event_handler("mmkeditor", 
						      (xitk_window_get_window(mmkeditor->xwin)),
						      NULL,
						      NULL,
						      NULL,
						      mmkeditor->widget_list,
						      NULL);
  
  mmkeditor->visible = 1;
  mmkeditor->running = 1;
  mmkeditor_set_mmk(mmk);
  mmk_editor_raise_window();
  
  while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(mmkeditor->xwin)))
    xine_usec_sleep(5000);

  XLockDisplay(gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(mmkeditor->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay(gGui->display);
}
