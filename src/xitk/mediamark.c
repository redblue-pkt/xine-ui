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
  if(extension && (!strncasecmp(extension, ".asx", 4)) && 
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
			const char *mrl, const char *ident, const char *sub, int start, int end) {

  if((mmk != NULL) && (mrl != NULL)) {
    
    (*mmk) = (mediamark_t *) xine_xmalloc(sizeof(mediamark_t));
    (*mmk)->mrl    = strdup(mrl);
    (*mmk)->ident  = strdup((ident != NULL) ? ident : mrl);
    (*mmk)->sub    = (sub != NULL) ? strdup(sub) : NULL;
    (*mmk)->start  = start;
    (*mmk)->end    = end;
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
		      mediamark_store_mmk(&mmk[(entry - 1)], buffer, NULL, NULL, 0, -1);
		    
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
		    
		  mediamark_store_mmk(&mmk[entries_m3u], entry, title, NULL, 0, -1);
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

		      mediamark_store_mmk(&mmk[entries_sfv], entry, NULL, NULL, 0, -1);
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
		
		mediamark_store_mmk(&mmk[entries_raw], entry, NULL, NULL, 0, -1);
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
			int           mmkf_members[5];
			char         *line;
			char         *m;
			
			mmkf.sub        = NULL;
			mmkf.start      = 0;
			mmkf.end        = -1;
			mmkf_members[0] = 0;  /* ident */
			mmkf_members[1] = -1; /* mrl */
			mmkf_members[2] = 0;  /* start */
			mmkf_members[3] = 0;  /* end */
			mmkf_members[4] = 0;  /* sub */
			
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
			      if(mmkf_members[4] == 0) {
				mmkf_members[4] = 1;
				set_pos_to_value(&key);
				xine_strdupa(mmkf.sub, key);
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
#endif
			
			if(entries_tox == 0)
			  mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
			else
			  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_tox + 2));
			mediamark_store_mmk(&mmk[entries_tox], 
					    mmkf.mrl, mmkf.ident, mmkf.sub, mmkf.start, mmkf.end);
			
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
	  __discard:
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
		  
		  mediamark_store_mmk(&mmk[entries_asx], href, real_title, NULL, 0, -1);
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
	else
	  fprintf(stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
	
      }
      else
	fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
      
      xml_parser_free_tree(xml_tree);
    __failure:
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

void mediamark_add_entry(const char *mrl, const char *ident, const char *sub, int start, int end) {
  if(!gGui->playlist.num)
    gGui->playlist.mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
  else { 
    if(gGui->playlist.num > 1) {
      gGui->playlist.mmk = (mediamark_t **) 
	realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 1));
    }
  }
  
  if(mediamark_store_mmk(&gGui->playlist.mmk[gGui->playlist.num], mrl, ident, sub, start, end))
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
			     int start, int end) {
  SAFE_FREE((*mmk)->mrl);
  SAFE_FREE((*mmk)->ident);
  SAFE_FREE((*mmk)->sub);
  (*mmk)->start = 0;
  (*mmk)->end = -1;

  (void) mediamark_store_mmk(mmk, mrl, ident, sub, start, end);
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

int mediamark_concat_mediamarks(const char *filename) {
  playlist_t             *playlist;
  int                     i, found;
  mediamark_t           **mmk = NULL;
  playlist_guess_func_t   guess_functions[] = {
    guess_asx_playlist,
    guess_toxine_playlist,
    guess_pls_playlist,
    guess_m3u_playlist,
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
    return 0;
  }

  gGui->playlist.cur = gGui->playlist.num;

  for(i = 0; i < playlist->entries; i++)
    mediamark_add_entry(mmk[i]->mrl, mmk[i]->ident, mmk[i]->sub, mmk[i]->start, mmk[i]->end);
  
  for(i = 0; i < playlist->entries; i++)
    (void) mediamark_free_mmk(&mmk[i]);
  
  SAFE_FREE(mmk);
  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);

  return 1;
}

void mediamark_load_mediamarks(const char *filename) {
  playlist_t             *playlist;
  int                     i, found, onum;
  mediamark_t           **mmk = NULL;
  mediamark_t           **ommk;
  playlist_guess_func_t   guess_functions[] = {
    guess_asx_playlist,
    guess_toxine_playlist,
    guess_pls_playlist,
    guess_m3u_playlist,
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

/*
 *  EDITOR
 */
static void mmkeditor_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;
  
  if(mmkeditor) {

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
      mmkeditor->visible = 0;
      xitk_hide_widgets(mmkeditor->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin));
      XUnlockDisplay(gGui->display);
    } else {
      if(mmkeditor->running) {
	mmkeditor->visible = 1;
	xitk_show_widgets(mmkeditor->widget_list);
	XLockDisplay(gGui->display);
	XRaiseWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin)); 
	XMapWindow(gGui->display, xitk_window_get_window(mmkeditor->xwin)); 
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
  }
}

static void mmkeditor_apply(xitk_widget_t *w, void *data) {
  const char *ident, *mrl, *sub;
  int         start, end;

  if(mmkeditor->mmk) {
    
    mrl = xitk_inputtext_get_text(mmkeditor->mrl); 
    ident = xitk_inputtext_get_text(mmkeditor->ident);
    sub = xitk_inputtext_get_text(mmkeditor->sub);
    start = xitk_intbox_get_value(mmkeditor->start);
    end = xitk_intbox_get_value(mmkeditor->end);

    if(start < 0)
      start = 0;

    if(end < -1)
      end = -1;

    mediamark_replace_entry(mmkeditor->mmk, mrl, ident, sub, start, end);

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
