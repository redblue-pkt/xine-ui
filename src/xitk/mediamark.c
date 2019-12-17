/*
 * Copyright (C) 2000-2019 the xine project
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <X11/keysym.h>
#include "common.h"

#define WINDOW_WIDTH            525
#define WINDOW_HEIGHT           270

static struct {
  apply_callback_t              callback;
  void                         *user_data;

  xitk_window_t                *xwin;

  xitk_widget_list_t           *widget_list;

  xitk_widget_t                *mrl;
  xitk_widget_t                *ident;
  xitk_widget_t                *sub;
  xitk_widget_t                *start;
  xitk_widget_t                *end;
  xitk_widget_t                *av_offset;
  xitk_widget_t                *spu_offset;

  mediamark_t                 **mmk;

  int                           running;
  int                           visible;
  xitk_register_key_t           widget_key;
} mmkeditor;


typedef struct {
  char                         *data;
  char                        **lines;
  int                           numl;
  
  int                           entries;
  char                         *type;
} playlist_t;

typedef mediamark_t **(*playlist_guess_func_t)(playlist_t *, const char *);
typedef mediamark_t **(*playlist_xml_guess_func_t)(playlist_t *, const char *, char *xml_content, xml_node_t *);

static char *_download_file(const char *filename, int *size) {
  download_t  *download;
  char        *buf = NULL;
  
  if((!filename) || (!strlen(filename))) {
    fprintf(stderr, "%s(): Empty or NULL filename.\n", __XINE_FUNCTION__);
    return NULL;
  }
  
  download = (download_t *) calloc(1, sizeof(download_t));
  download->buf    = NULL;
  download->error  = NULL;
  download->size   = 0;
  download->status = 0; 
  
  if((network_download(filename, download))) {
    *size = download->size;
    buf = (char *) malloc(*size + 1);
    if (buf) {
      memcpy(buf, download->buf, *size);
      buf[*size] = 0;
    }
  }
  else
    xine_error (gGui, "Unable to download '%s': %s", filename, download->error);

  SAFE_FREE(download->buf);
  SAFE_FREE(download->error);
  free(download);
  
  return buf;
}

#if 0
static int _file_exist(char *filename) {
  struct stat  st;
  
  if(filename && (stat(filename, &st) == 0))
    return 1;
  
  return 0;
}
#endif

int mrl_look_like_playlist (const char *mrl) {
  /* TJ. I dont know whether someone really needs to treat
   * "foo/bar.m3under/the/table" as an m3u playlist.
   * Lets keep this behaviour for now, but make sure that
   * hls (.m3u8) goes to xine-lib verbatim. */
  const char *extension = strrchr (mrl, '.');
  if (extension) {
    /* All known playlist ending */
    if ((!strncasecmp (extension, ".asx", 4))  ||
        (!strncasecmp (extension, ".smi", 4))  ||
      /*(!strncasecmp (extension, ".smil", 5)) || caught by ".smi" */
        (!strncasecmp (extension, ".pls", 4))  ||
        (!strncasecmp (extension, ".sfv", 4))  ||
        (!strncasecmp (extension, ".xml", 4))  ||
        (!strncasecmp (extension, ".tox", 4))  ||
        (!strncasecmp (extension, ".fxd", 4)))
      return 1;
    if ((!strncasecmp (extension, ".m3u", 4)) && (extension[4] != '8'))
      return 1;
  }
  return 0;
}
 
static char *_read_file(const char *filename, int *size) {
  struct stat  st;
  char        *buf = NULL;
  int          fd, bytes_read;

  if((!filename) || (!strlen(filename))) {
    fprintf(stderr, "%s(): Empty or NULL filename.\n", __XINE_FUNCTION__);
    return NULL;
  }

  if (mrl_look_like_playlist (filename) && is_downloadable ((char *)filename))
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

  if((fd = xine_open_cloexec(filename, O_RDONLY)) == -1) {
    fprintf(stderr, "%s(): open(%s) failed: %s.\n", __XINE_FUNCTION__, filename, strerror(errno));
    return NULL;
  }
  
  if((buf = (char *) malloc(*size + 1)) == NULL) {
    fprintf(stderr, "%s(): malloc() failed.\n", __XINE_FUNCTION__);
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

int mediamark_have_alternates(mediamark_t *mmk) {
  if(mmk && mmk->alternates)
    return 1;

  return 0;
}
void mediamark_free_alternates(mediamark_t *mmk) {
  if(mmk && mediamark_have_alternates(mmk)) {
    alternate_t *alt = mmk->alternates;
    
    while(alt) {
      alternate_t *c_alt = alt;
      
      free(alt->mrl);

      c_alt = alt;
      alt   = alt->next;
      free(c_alt);
    }
    mmk->alternates = NULL;
    mmk->cur_alt    = NULL;
  }
}
char *mediamark_get_first_alternate_mrl(mediamark_t *mmk) {
  if(mmk && mmk->alternates) {
    mmk->cur_alt = mmk->alternates;
    return mmk->cur_alt->mrl;
  }
  return NULL;
}
char *mediamark_get_next_alternate_mrl(mediamark_t *mmk) {
  if(mmk && mmk->cur_alt) {
    if(mmk->cur_alt->next) {
      mmk->cur_alt = mmk->cur_alt->next;
      return mmk->cur_alt->mrl;
    }
  }
  return NULL;
}
char *mediamark_get_current_alternate_mrl(mediamark_t *mmk) {
  if(mmk && mmk->cur_alt)
    return mmk->cur_alt->mrl;

  return NULL;
}
void mediamark_append_alternate_mrl(mediamark_t *mmk, const char *mrl) {
  if(mmk && mrl) {
    alternate_t *alt;

    alt       = (alternate_t *) calloc(1, sizeof(alternate_t));
    alt->mrl  = strdup(mrl);
    alt->next = NULL;
    
    if(mmk->alternates) {
      alternate_t *p_alt = mmk->alternates;

      while(p_alt->next)
	p_alt = p_alt->next;
      p_alt->next = alt;
    }
    else
      mmk->alternates = alt;

  }
}
void mediamark_duplicate_alternates(mediamark_t *s_mmk, mediamark_t *d_mmk) {
  if(s_mmk && s_mmk->alternates && d_mmk) {
    alternate_t *alt;
    
    if((alt = s_mmk->alternates)) {
      alternate_t *c_alt, *p_alt = NULL, *t_alt = NULL;
      
      while(alt) {
	
	c_alt       = (alternate_t *) calloc(1, sizeof(alternate_t));
	c_alt->mrl  = strdup(alt->mrl);
	c_alt->next = NULL;
	
	if(!p_alt)
	  t_alt = p_alt = c_alt;
	else {
	  p_alt->next = c_alt;
	  p_alt       = c_alt;
	}
	
	alt = alt->next;
      }

      d_mmk->alternates = t_alt;
      d_mmk->cur_alt    = NULL;

    }
  }
}
int mediamark_got_alternate(mediamark_t *mmk) {
  if(mmk && mmk->got_alternate)
    return 1;
  return 0;
}
void mediamark_set_got_alternate(mediamark_t *mmk) {
  if(mmk)
    mmk->got_alternate = 1;
}
void mediamark_unset_got_alternate(mediamark_t *mmk) {
  if(mmk)
    mmk->got_alternate = 0;
}
int mediamark_store_mmk(mediamark_t **mmk, 
			const char *mrl, const char *ident, const char *sub, 
			int start, int end, int av_offset, int spu_offset) {
  
  if(mmk && mrl) {

    (*mmk) = (mediamark_t *) calloc(1, sizeof(mediamark_t));
    (*mmk)->mrl           = strdup(mrl);
    (*mmk)->ident         = strdup((ident != NULL) ? ident : mrl);
    (*mmk)->sub           = (sub != NULL) ? strdup(sub) : NULL;
    (*mmk)->start         = start;
    (*mmk)->end           = end;
    (*mmk)->av_offset     = av_offset;
    (*mmk)->spu_offset    = spu_offset;
    (*mmk)->played        = 0;
    (*mmk)->got_alternate = 0;
    (*mmk)->cur_alt       = NULL;
    (*mmk)->alternates    = NULL;

    return 1;
  }

  return 0;
}

mediamark_t *mediamark_clone_mmk(mediamark_t *mmk) {
  mediamark_t *cmmk = NULL;

  if(mmk && mmk->mrl) {
    cmmk = (mediamark_t *) calloc(1, sizeof(mediamark_t));
    cmmk->mrl           = strdup(mmk->mrl);
    cmmk->ident         = (mmk->ident) ? strdup(mmk->ident) : NULL;
    cmmk->sub           = (mmk->sub) ? strdup(mmk->sub) : NULL;
    cmmk->start         = mmk->start;
    cmmk->end           = mmk->end;
    cmmk->av_offset     = mmk->av_offset;
    cmmk->spu_offset    = mmk->spu_offset;
    cmmk->played        = mmk->played;
    cmmk->got_alternate = mmk->got_alternate;
    mediamark_duplicate_alternates(mmk, cmmk);
  }

  return cmmk;
}

int mediamark_free_mmk(mediamark_t **mmk) {
  if((*mmk) != NULL) {
    mediamark_free_alternates((*mmk));
    SAFE_FREE((*mmk)->ident);
    SAFE_FREE((*mmk)->mrl);
    SAFE_FREE((*mmk)->sub);
    SAFE_FREE((*mmk));
    return 1;
  }
  return 0;
}

/*
 * Split lines from playlist->data
 */
static int playlist_split_data(playlist_t *playlist) {
  
  if(playlist->data) {
    char *buf, *pbuf, *p;

    /* strsep modify original string, avoid its corruption */
    buf = strdup(playlist->data);
    playlist->numl = 0;

    pbuf = buf;
    while((p = xine_strsep(&pbuf, "\n")) != NULL) {
      if(p && (p <= pbuf) && (strlen(p))) {

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

  ABORT_IF_NULL(*p);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

static char *get_basedir(const char *filename) {
  char *path;
  char *origin = NULL;

  path = strrchr(filename, '/');
  if(path && (path > filename)) {
    origin = (char *) malloc((path - filename) + 2);
    snprintf(origin, (path-filename)+2, "%s", filename);
  }

  return origin;
}

static int is_mrl(const char *entry) {
  char *path;

  path = strstr(entry, ":/");
  if (path) {
    while (entry < path) {
      if (!isalnum(entry[0])) return 0;
      entry++;
    }
    return 1;
  }
  return 0;
}

static const char *concat_basedir(char *buffer, size_t size, const char *origin, const char *entry) {
  if(origin && entry[0] != '/' && !is_mrl(entry)) {
    snprintf(buffer, size, "%s%s", origin, entry);
    return buffer;
  } else {
    return entry;
  }
}

/*
 * Playlists guessing
 */
static mediamark_t **guess_pls_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  char         *extension;

  if(filename) {
    if(is_a_file((char *) filename) || is_downloadable((char *) filename)) {
      
      extension = strrchr(filename, '.');
      
      if((extension) && (!strncasecmp(extension, ".pls", 4))) {
	char *pls_content;
	int   size;
	
	if((pls_content = _read_file(filename, &size)) != NULL) {
	  
	  playlist->data = pls_content;
	  
	  if(playlist_split_data(playlist)) {
	    int   valid_pls     = 0;
	    int   entries_pls   = 0;
	    int   found_nument  = 0;
	    int   stored_nument = 0;
	    int   pl_line       = 0;
	    int   linen         = 0;
	    int   count         = 0;
	    char *ln, *origin;
	    const char *store_mrl;
	    char  buffer[_PATH_MAX + _NAME_MAX + 2];

	    origin = get_basedir(filename);
	    do {
	      
	      while((ln = playlist->lines[linen++]) != NULL) {
		if(ln) {

		  if(valid_pls) {
		    
		    if(entries_pls) {
		      int   entry;
		      
		      if((!strncasecmp(ln, "file", 4)) && ((sscanf(ln + 4, "%d=", &entry)) == 1)) {
			char *mrl = strchr(ln, '=');
			
			if(mrl)
			  mrl++;
			
			if((entry && mrl) && 
			   ((entry) <= entries_pls) && 
			   (mmk && (!mmk[(entry - 1)]))) {
			  stored_nument++;
			  store_mrl = concat_basedir(buffer, sizeof(buffer), origin, mrl);
			  mediamark_store_mmk(&mmk[(entry - 1)], store_mrl, NULL, NULL, 0, -1, 0, 0);
			}
			
		      }
		      
		    }
		    else {
		      if((!strncasecmp(ln, "numberofentries", 15))
			 && ((sscanf(ln + 15, "=%d", &entries_pls)) == 1)) {
			
			if(!found_nument) {
			  if(entries_pls) {
			    playlist->entries = entries_pls;
			    mmk = (mediamark_t **) calloc((entries_pls + 1), sizeof(mediamark_t *));
			    found_nument = 1;
			  }
			}

		      }
		    }
		    
		  }
		  else if((!strcasecmp(ln, "[playlist]"))) {
		    if(!valid_pls) {
		      valid_pls = 1;
		      pl_line = linen;
		    }
		  }
		  
		}
	      }
	      
	      count++;
	      linen = pl_line;

	    } while((playlist->entries && !stored_nument) && (count < 2));
	    free(origin);
	    
	    if(mmk && valid_pls && entries_pls) {
	      int i;
	      
	      mmk[entries_pls] = NULL;

	      /* Fill missing entries */
	      for(i = 0; i < entries_pls; i++) {
		if(!mmk[i])
		  mediamark_store_mmk(&mmk[i], _("!!Invalid entry!!"), NULL, NULL, 0, -1, 0, 0);
	      }
	      playlist->type = strdup("PLS");
	    }

	    while(playlist->numl) {
	      free(playlist->lines[playlist->numl - 1]);
	      playlist->numl--;
	    }

	    if(valid_pls && entries_pls) {
	      SAFE_FREE(playlist->lines);
	      free(pls_content);
	      return mmk;
	    }
	  }
	  SAFE_FREE(playlist->lines);
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
    if(is_a_file((char *) filename) || is_downloadable((char *) filename)) {
      char *m3u_content;
      int   size;
      
      if((m3u_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = m3u_content;
	
	if(playlist_split_data(playlist)) {
	  int   valid_m3u   = 0;
	  int   entries_m3u = 0;
	  char *title       = NULL;
	  char *origin;
	  int   linen = 0;
	  char *ln;

	  origin = get_basedir(filename);	  
	  
	  while((ln = playlist->lines[linen++]) != NULL) {
	    if(ln) {
	      
	      if(valid_m3u) {
		
		if(!strncmp(ln, "#EXTINF", 7)) {
                  char *ptitle;
		  if((ptitle = strchr(ln, ',')) != NULL) {
		    ptitle++;

		    SAFE_FREE(title);

		    if(strlen(ptitle))
		      title = strdup(ptitle);
		  }
		}
		else if (ln[0] != '#') {
		  char  buffer[_PATH_MAX + _NAME_MAX + 2];
		  const char *entry;
		  
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_m3u + 2));
		  entry = concat_basedir(buffer, sizeof(buffer), origin, ln);

		  mediamark_store_mmk(&mmk[entries_m3u], entry, title, NULL, 0, -1, 0, 0);
		  playlist->entries = ++entries_m3u;

		  SAFE_FREE(title);
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

	  free(origin);
          free(title);

	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	}
	SAFE_FREE(playlist->lines);
	
	free(m3u_content);
      }
    }
  }
  
  return mmk;
}

static mediamark_t **guess_sfv_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    if(is_a_file((char *) filename) || is_downloadable((char *) filename)) {
      char  *extension;
  
      extension = strrchr(filename, '.');
      
      if((extension) && (!strncasecmp(extension, ".sfv", 4))) {
	char *sfv_content;
	int   size;
	
	if((sfv_content = _read_file(filename, &size)) != NULL) {
	  
	  playlist->data = sfv_content;

	  if(playlist_split_data(playlist)) {
	    int    valid_sfv = 0;
	    int    entries_sfv = 0;
	    char  *origin;
	    int    linen = 0;
	    char  *ln;

	    origin = get_basedir(filename);
	    
	    while((ln = playlist->lines[linen++]) != NULL) {
	      
	      if(ln) {
		
		if(valid_sfv) {
		  
		  if(strncmp(ln, ";", 1)) {
		    char            buffer[_PATH_MAX + _NAME_MAX + 2];
		    const char     *entry;
		    long long int   crc = 0;
		    char           *p;

		    p = ln + strlen(ln) - 1;
		    if(p) {

		      while(p && (p > ln) && (*p != ' '))
			p--;
		      
		      if(p > ln) {
			*p = '\0';
			p++;
		      }

		      if(p && strlen(p)) {
			crc = strtoll(p, &p, 16);

			if((errno == ERANGE) || (errno == EINVAL))
			  crc = 0;

		      }

		      if(crc > 0) {
			
			mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_sfv + 2));
			
		  	entry = concat_basedir(buffer, sizeof(buffer), origin, ln);
			mediamark_store_mmk(&mmk[entries_sfv], entry, NULL, NULL, 0, -1, 0, 0);
			playlist->entries = ++entries_sfv;
		      }
		    }
		  }
		}
		else if(strlen(ln) > 1) {
		  long int   size;
		  int        h, m, s;
		  int        Y, M, D;
		  char       fn[2];
		  char       mon[4];
		  
		  if(((sscanf(ln, ";%ld %d:%d.%d %d-%d-%d %1s", &size, &h, &m, &s, &Y, &M, &D, &fn[0])) == 8) ||
		     ((sscanf(ln, ";%ld %3s %d %d:%d:%d %d %1s", &size, &mon[0], &D, &h, &m, &s, &Y, &fn[0])) == 8))
		    valid_sfv = 1;

		}		
	      }
	    }

	    if(valid_sfv && entries_sfv) {
	      mmk[entries_sfv] = NULL;
	      playlist->type = strdup("SFV");
	    }
	    
	    free(origin);

	    while(playlist->numl) {
	      free(playlist->lines[playlist->numl - 1]);
	      playlist->numl--;
	    }
	  }
	  SAFE_FREE(playlist->lines);
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
    if(is_a_file((char *) filename) || is_downloadable((char *) filename)) {
      char *raw_content;
      int   size;

      if((raw_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = raw_content;
	
	if(playlist_split_data(playlist)) {
	  char *origin;
	  int   entries_raw = 0;
	  int   linen = 0;
	  char *ln;
	  
	  origin = get_basedir(filename);

	  while((ln = playlist->lines[linen++]) != NULL) {
	    if(ln) {
	      
	      if((strncmp(ln, ";", 1)) && (strncmp(ln, "#", 1))) {
		char        buffer[_PATH_MAX + _NAME_MAX + 2];
		const char *entry;
		
		mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_raw + 2));
		
	  	entry = concat_basedir(buffer, sizeof(buffer), origin, ln);

		mediamark_store_mmk(&mmk[entries_raw], entry, NULL, NULL, 0, -1, 0, 0);
		playlist->entries = ++entries_raw;
	      }
	    }
	  }
	  
	  if(entries_raw) {
	    mmk[entries_raw] = NULL;
	    playlist->type = strdup("RAW");
	  }

	  free(origin);

	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	}
	SAFE_FREE(playlist->lines);
	free(raw_content);
      }
    }
  }
  
  return mmk;
}

static mediamark_t **guess_toxine_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;
  
  if(filename) {
    if(is_a_file((char *) filename) || is_downloadable((char *) filename)) {
      int   entries_tox = 0;
      char *tox_content;
      int   size;
      
      if((tox_content = _read_file(filename, &size)) != NULL) {
	
	playlist->data = tox_content;
	
	if(playlist_split_data(playlist)) {
	  char    buffer[32768], path[_PATH_MAX + _NAME_MAX + 2];
	  char   *p, *pp, *origin;
	  int     start = 0;
	  int     linen = 0;
	  char   *ln;
	  
	  memset(&buffer, 0, sizeof(buffer));
	  p = buffer;
	  origin = get_basedir(filename);

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
			int           mmkf_members[7];
			char         *line, *pline;
			char         *m;
			
			mmkf.ident      = NULL;
			mmkf.sub        = NULL;
			mmkf.start      = 0;
			mmkf.end        = -1;
			mmkf.av_offset  = 0;
			mmkf.spu_offset = 0;
			mmkf.mrl        = NULL;

			mmkf_members[0] = 0;  /* ident */
			mmkf_members[1] = -1; /* mrl */
			mmkf_members[2] = 0;  /* start */
			mmkf_members[3] = 0;  /* end */
			mmkf_members[4] = 0;  /* av offset */
			mmkf_members[5] = 0;  /* spu offset */
			mmkf_members[6] = 0;  /* sub */
			
			line = strdup(atoa(buffer));
			
			pline = line;
			while((m = xine_strsep(&pline, ";")) != NULL) {
			  char *key;
			  const char *entry;
			  
			  key = atoa(m);
			  if(strlen(key)) {
			    
			    if(!strncasecmp(key, "identifier", 10)) {
			      if(mmkf_members[0] == 0) {
				mmkf_members[0] = 1;
				set_pos_to_value(&key);
				mmkf.ident = strdup(key);
			      }
			    }
			    else if(!strncasecmp(key, "subtitle", 8)) {
			      if(mmkf_members[6] == 0) {
				set_pos_to_value(&key);
				/* Workaround old toxine playlist version bug */
				if(strcmp(key, "(null)")) {
				  mmkf_members[6] = 1;
				  entry = concat_basedir(path, sizeof(path), origin, key);
				  mmkf.sub = strdup(entry);
				}
			      }
			    }
			    else if(!strncasecmp(key, "spu_offset", 10)) {
			      if(mmkf_members[5] == 0) {
				mmkf_members[5] = 1;
				set_pos_to_value(&key);
				mmkf.spu_offset = strtol(key, &key, 10);
			      }
			    }
			    else if(!strncasecmp(key, "av_offset", 9)) {
			      if(mmkf_members[4] == 0) {
				mmkf_members[4] = 1;
				set_pos_to_value(&key);
				mmkf.av_offset = strtol(key, &key, 10);
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
				entry = concat_basedir(path, sizeof(path), origin, key);
				mmkf.mrl = strdup(entry);
			      }
			    }
			  }
			}
			
			if((mmkf_members[1] == 0) || (mmkf_members[1] == -1)) {
			  /*  printf("wow, no mrl found\n"); */
			  goto __discard;
			}
			
			if(mmkf_members[0] == 0)
			  mmkf.ident = mmkf.mrl;
			
			/*
			  STORE new mmk;
			*/
#if 0
			printf("DUMP mediamark entry:\n");
			printf("ident:     '%s'\n", mmkf.ident);
			printf("mrl:       '%s'\n", mmkf.mrl);
			printf("sub:       '%s'\n", mmkf.sub);
			printf("start:      %d\n", mmkf.start);
			printf("end:        %d\n", mmkf.end);
			printf("av_offset:  %d\n", mmkf.av_offset);
			printf("spu_offset: %d\n", mmkf.spu_offset);
#endif
			
			mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_tox + 2));
			
			mediamark_store_mmk(&mmk[entries_tox], 
					    mmkf.mrl, mmkf.ident, mmkf.sub, 
					    mmkf.start, mmkf.end, mmkf.av_offset, mmkf.spu_offset);
			playlist->entries = ++entries_tox;

			free(line);
			
			if(mmkf_members[0])
			  free(mmkf.ident);

			SAFE_FREE(mmkf.sub);

			free(mmkf.mrl);
		      }
		      
		    }
		    else {
		      memset(&buffer, 0, sizeof(buffer));
		      p = buffer;
		      goto __discard;
		    }
		  }
		  
		  if(*pp != '\0') {
		    /* buffer full? don't copy */
		    if (p - buffer < sizeof (buffer) - 1)
		      *p++ = *pp;
		    pp++;
		  }
		}
		
	      }
	    }
	  __discard: ;
	  }

	  free(origin);
	  
	  if(entries_tox) {
	    mmk[entries_tox] = NULL;
	    playlist->type = strdup("TOX");
	  }

	  while(playlist->numl) {
	    free(playlist->lines[playlist->numl - 1]);
	    playlist->numl--;
	  }
	}
	SAFE_FREE(playlist->lines);
	free(tox_content);
      }
    }
  }
  return mmk;
}

/*
 * XML based playlists
 */
static mediamark_t **xml_asx_playlist(playlist_t *playlist, const char *filename, char *xml_content, xml_node_t *xml_tree) {
  mediamark_t **mmk = NULL;

  if(xml_tree) {
    xml_node_t      *asx_entry, *asx_ref;
    xml_property_t  *asx_prop;
    int              entries_asx = 0;

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
	      char *sub    = NULL;

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
		    /* This is not part of the ASX specs */
		    else if(!strcasecmp(asx_prop->name, "SUBTITLE")) {
		      
		      if(!sub)
			sub = asx_prop->value;
		    }

		    if(href && sub)
		      break;
		  }
		}

		asx_ref = asx_ref->next;
	      }
		
	      if(href && strlen(href)) {
		/* Use the _orig pointers to store the string before
		   using atoa() that changes the pointer */
		char *atitle     = NULL, *atitle_orig  = NULL;
		char *aauthor    = NULL, *aauthor_orig = NULL;
		char *real_title = NULL;
		int   len        = 0;
		  
		if(title && strlen(title)) {
		  atitle_orig = atitle = strdup(title);
		  atitle = atoa(atitle);
		  len = strlen(atitle);
		    
		  if(author && strlen(author)) {
		    aauthor_orig = aauthor = strdup(author);
		    aauthor = atoa(aauthor);
		    len += strlen(aauthor) + 3;
		  }
		    
		  len++;
		}
		  
		if(atitle && strlen(atitle)) {
		  real_title = (char *) malloc(len);
		  strcpy(real_title, atitle);
		    
		  if(aauthor && strlen(aauthor))
		    snprintf(real_title+strlen(real_title), len-strlen(real_title), " (%s)", aauthor);
		}
		  
		mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_asx + 2));
		  
		mediamark_store_mmk(&mmk[entries_asx], href, real_title, sub, 0, -1, 0, 0);
		playlist->entries = ++entries_asx;

		SAFE_FREE(real_title);
		SAFE_FREE(atitle_orig);
		SAFE_FREE(aauthor_orig);
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
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif
      
    /* Maybe it's 'ASF <url> */
    if(entries_asx == 0) {
      
      playlist->data = xml_content;
      
      if(playlist_split_data(playlist)) {
	int    linen = 0;
	char  *ln;
	
	while((ln = playlist->lines[linen++]) != NULL) {
	  
	  if(!strncasecmp("ASF", ln, 3)) {
	    char *p = ln + 3;
	    
	    while(p && ((*p == ' ') || (*p == '\t')))
	      p++;
	    
	    if(p && strlen(p)) {
	      mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_asx + 2));
	      
	      mediamark_store_mmk(&mmk[entries_asx], p, p, NULL, 0, -1, 0, 0);
	      playlist->entries = ++entries_asx;
	    }
	  }
	}
      }
      SAFE_FREE(playlist->lines);
    }

    if(entries_asx) {
      mmk[entries_asx] = NULL;
      playlist->type = strdup("ASX3");
      return mmk;
    }
  }
  
  return NULL;
}

static void __gx_get_entries(playlist_t *playlist, mediamark_t ***mmk, int *entries, xml_node_t *entry) {
  xml_property_t  *prop;
  xml_node_t      *ref;
  xml_node_t      *node = entry;  
  
  while(node) {
    if(!strcasecmp(node->name, "SUB"))
      __gx_get_entries(playlist, mmk, entries, node->child);
    else if(!strcasecmp(node->name, "ENTRY")) {
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

	  for(prop = ref->props; prop; prop = prop->next) {
	    if(!strcasecmp(prop->name, "START")) {

	      if(prop->value && strlen(prop->value))
		start = atoi(prop->value);

	    }
	  }
	}

	ref = ref->next;
      }
      
      if(href && strlen(href)) {
	char *atitle = NULL;
	
	if(title && strlen(title)) {
	  atitle = strdup(title);
	  atitle = atoa(atitle);
	}

	(*mmk) = (mediamark_t **) realloc((*mmk), sizeof(mediamark_t *) * (*entries + 2));
	
	mediamark_store_mmk(&(*mmk)[*entries], href, (atitle && strlen(atitle)) ? atitle : NULL, NULL, start, -1, 0, 0);
	playlist->entries = ++(*entries);
	
	free(atitle);
      }
      
      href = title = NULL;
      start = 0;
    }

    node = node->next;
  }
}
static mediamark_t **xml_gx_playlist(playlist_t *playlist, const char *filename, char *xml_content, xml_node_t *xml_tree) {
  mediamark_t **mmk = NULL;

  if(xml_tree) {
    xml_node_t      *gx_entry;
    xml_property_t  *gx_prop;
    int              entries_gx = 0;

    if(!strcasecmp(xml_tree->name, "GXINEMM")) {
      
      gx_prop = xml_tree->props;
      
      while((gx_prop) && (strcasecmp(gx_prop->name, "VERSION")))
	gx_prop = gx_prop->next;
      
      if(gx_prop) {
	int  version_major;
	
	if(((sscanf(gx_prop->value, "%d", &version_major)) == 1) && (version_major == 1)) {

	  gx_entry = xml_tree->child;
	  while(gx_entry) {

	    if(!strcasecmp(gx_entry->name, "SUB")) {
	      __gx_get_entries(playlist, &mmk, &entries_gx, gx_entry->child);
	    }
	    else if(!strcasecmp(gx_entry->name, "ENTRY"))
	      __gx_get_entries(playlist, &mmk, &entries_gx, gx_entry);

	    gx_entry = gx_entry->next;
	  }
	}
	else
	  fprintf(stderr, "%s(): Wrong GXINEMM version: %s\n", __XINE_FUNCTION__, gx_prop->value);
      }
      else
	fprintf(stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
    }
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif
    
    if(entries_gx) {
      mmk[entries_gx] = NULL;
      playlist->type = strdup("GXMM");
      return mmk;
    }
  }

  return NULL;
}

static mediamark_t **xml_noatun_playlist(playlist_t *playlist, const char *filename, char *xml_content, xml_node_t *xml_tree) {
  mediamark_t **mmk = NULL;

  if(xml_tree) {
    xml_node_t      *noa_entry;
    xml_property_t  *noa_prop;
    int              entries_noa = 0;

    if(!strcasecmp(xml_tree->name, "PLAYLIST")) {
      int found = 0;
      
      noa_prop = xml_tree->props;
      
      while(noa_prop) {
	if((!strcasecmp(noa_prop->name, "CLIENT")) && (!strcasecmp(noa_prop->value, "NOATUN")))
	  found++;
	else if(!strcasecmp(noa_prop->name, "VERSION")) {
	  int  version_major;
	  
	  if(((sscanf(noa_prop->value, "%d", &version_major)) == 1) && (version_major == 1))
	    found++;
	}
	
	noa_prop = noa_prop->next;
      }
      
      if(found >= 2) {
	noa_entry = xml_tree->child;
	
	while(noa_entry) {
	  
	  if(!strcasecmp(noa_entry->name, "ITEM")) {
	    char *real_title = NULL;
	    char *title      = NULL;
	    char *album      = NULL;
	    char *artist     = NULL;
	    char *url        = NULL;
	    
	    for(noa_prop = noa_entry->props; noa_prop; noa_prop = noa_prop->next) {
	      if(!strcasecmp(noa_prop->name, "TITLE"))
		title = noa_prop->value;
	      else if(!strcasecmp(noa_prop->name, "ALBUM"))
		album = noa_prop->value;
	      else if(!strcasecmp(noa_prop->name, "ARTIST"))
		artist = noa_prop->value;
	      else if(!strcasecmp(noa_prop->name, "URL"))
		url = noa_prop->value;
	    }
	    
	    if(url) {
	      int   titlen = 0;
	      
	      /*
		title (artist - album)
	      */
	      if(title && (titlen = strlen(title))) {
		int artlen = 0;
		int alblen = 0;
		
		if(artist && (artlen = strlen(artist)) && album && (alblen = strlen(album))) {
		  int len = titlen + artlen + alblen + 7;
		  
		  real_title = (char *) malloc(len);
		  sprintf(real_title, "%s (%s - %s)", title, artist, album);
		}
		else if(artist && (artlen = strlen(artist))) {
		  int len = titlen + artlen + 4;
		  
		  real_title = (char *) malloc(len);
		  sprintf(real_title, "%s (%s)", title, artist);
		}
		else if(album && (alblen = strlen(album))) {
		  int len = titlen + alblen + 4;
		  
		  real_title = (char *) malloc(len);
		  sprintf(real_title, "%s (%s)", title, album);
		}
		else
		  real_title = strdup(title);
		
	      }
	      
	      mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_noa + 2));
	      
	      mediamark_store_mmk(&mmk[entries_noa], url, real_title, NULL, 0, -1, 0, 0);
	      playlist->entries = ++entries_noa;
	      
	      free(real_title);
	      
	    }
	  }
	  
	  noa_entry = noa_entry->next;
	}
      }
    }
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif    
    
    if(entries_noa) {
      mmk[entries_noa] = NULL;
      playlist->type = strdup("NOATUN");
      return mmk;
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
#define palign do { int i; for(i = 0; i < offset; i++) { printf(" "); } } while(0)
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

  node       = (smil_node_t *) calloc(1, sizeof(smil_node_t));
  node->mmk  = NULL;
  node->next = NULL;

  smil_init_smil_property(&(node->prop));

  return node;
}
static mediamark_t *smil_new_mediamark(void) {
  mediamark_t *mmk;

  mmk             = (mediamark_t *) calloc(1, sizeof(mediamark_t));
  mmk->mrl        = NULL;
  mmk->ident      = NULL;
  mmk->sub        = NULL;
  mmk->start      = 0;
  mmk->end        = -1;
  mmk->played     = 0;
  mmk->alternates = NULL;
  mmk->cur_alt    = NULL;

  return mmk;
}
static mediamark_t *smil_duplicate_mmk(mediamark_t *ommk) {
  mediamark_t *mmk = NULL;
  
  if(ommk) {
    mmk             = smil_new_mediamark();
    mmk->mrl        = strdup(ommk->mrl);
    mmk->ident      = strdup(ommk->ident ? ommk->ident : ommk->mrl);
    mmk->sub        = ommk->sub ? strdup(ommk->sub) : NULL;
    mmk->start      = ommk->start;
    mmk->end        = ommk->end;
    mmk->played     = ommk->played;
    mmk->alternates = NULL;
    mmk->cur_alt    = NULL;
  }
  
  return mmk;
}
static void smil_free_properties(smil_property_t *smil_props) {
  if(smil_props) {
    SAFE_FREE(smil_props->anchor);
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
  //int    hours, mins, secs, msecs;
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

  /* not reached
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
  else*/ {
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
    printf("%s(): prop_name '%s', value: '%s'\n", __XINE_FUNCTION__,
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
      
      SAFE_FREE(dprop->anchor);
      
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
    printf("%s(): prop_name '%s', value: '%s'\n", __XINE_FUNCTION__,
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
	    strlcat(p, smil->base, sizeof(buffer));
	    
	    if(buffer[strlen(buffer) - 1] != '/')
	      strlcat(p, "/", sizeof(buffer));
	  }
	  
	  strlcat(p, prop->value, sizeof(buffer));
	  
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

  SAFE_FREE(smil->title);
  SAFE_FREE(smil->author);
  SAFE_FREE(smil->copyright);
  SAFE_FREE(smil->year);
  SAFE_FREE(smil->base);
}

static mediamark_t **xml_smil_playlist(playlist_t *playlist, const char *filename, char *xml_content, xml_node_t *xml_tree) {
  mediamark_t **mmk = NULL;

  if(xml_tree) {
    xml_node_t      *smil_entry, *smil_ref;
    smil_t           smil;
    int              entries_smil = 0;

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
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif    
    
    if(entries_smil) {
      mmk[entries_smil] = NULL;
      playlist->entries = entries_smil;
      playlist->type    = strdup("SMIL");
      return mmk;
    }
  }
  
  return NULL;
}
/*
 * ********************************** SMIL END ***********************************
 */

static mediamark_t **xml_freevo_playlist(playlist_t *playlist, const char *filename, char *xml_content, xml_node_t *xml_tree) {
  mediamark_t **mmk = NULL;

  if(xml_tree) {
    xml_node_t      *fvo_entry;
    xml_property_t  *fvo_prop;
    int              entries_fvo = 0;
    
    if(!strcasecmp(xml_tree->name, "FREEVO")) {
      char *origin;
      char *url    = NULL;
      char *sub    = NULL;
      char *title  = NULL;

      origin = get_basedir(filename);
	  
      fvo_entry = xml_tree->child;
      
      while(fvo_entry) {
	if(!strcasecmp(fvo_entry->name, "MOVIE")) {
	  xml_node_t *sentry;

	  for(fvo_prop = fvo_entry->props; fvo_prop; fvo_prop = fvo_prop->next) {
	    if(!strcasecmp(fvo_prop->name, "TITLE")) {
	      title = fvo_prop->value;
	    }
	  }
	  
	  sentry = fvo_entry->child;
	  while(sentry) {
	    
	    if((!strcasecmp(sentry->name, "VIDEO")) || (!strcasecmp(sentry->name, "AUDIO"))) {
	      xml_node_t *ssentry = sentry->child;
	      
	      while(ssentry) {
		
		if(!strcasecmp(ssentry->name, "SUBTITLE")) {
		  sub = ssentry->data;
		}
		else if((!strcasecmp(ssentry->name, "URL")) || 
			(!strcasecmp(ssentry->name, "DVD")) || (!strcasecmp(ssentry->name, "VCD"))) {
		  url = strdup(ssentry->data);
		}
		else if(!strcasecmp(ssentry->name, "FILE")) {

		  if(origin) {
		    const size_t urlsize = strlen(origin) + strlen(ssentry->data) + 2;
		    url = (char *) malloc(urlsize);
		    strlcat(url, origin, urlsize);
		    
		    if((url[strlen(url) - 1] == '/') && (*ssentry->data == '/'))
		      url[strlen(url) - 1] = '\0';
		    
		    strlcat(url, ssentry->data, urlsize);
		    
		  }
		  else
		    url = strdup(ssentry->data);

		}

		if(url) {
		  
		  mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (entries_fvo + 2));
		  
		  mediamark_store_mmk(&mmk[entries_fvo], url, title, sub, 0, -1, 0, 0);
		  playlist->entries = ++entries_fvo;
		  
		  free(url);
		  url = NULL;
		}

		sub     = NULL;
		ssentry = ssentry->next;
	      }

	    }
	    
	    sentry = sentry->next;
	  }
	  
	}

	title     = NULL;
	fvo_entry = fvo_entry->next;
      }
      free(origin);
    }
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif    
    
    if(entries_fvo) {
      mmk[entries_fvo] = NULL;
      playlist->type = strdup("FREEVO");
      return mmk;
    }
  }

  return NULL;
}

static mediamark_t **guess_xml_based_playlist(playlist_t *playlist, const char *filename) {
  mediamark_t **mmk = NULL;

  if(filename) {
    char            *xml_content;
    int              size;
    int              result;
    xml_node_t      *xml_tree, *top_xml_tree;
    
    if((xml_content = _read_file(filename, &size)) != NULL) {
      int                         i;
      playlist_xml_guess_func_t   guess_functions[] = {
	xml_asx_playlist,
	xml_gx_playlist,
	xml_noatun_playlist,
	xml_smil_playlist,
	xml_freevo_playlist,
	NULL
      };

      xml_parser_init_R(xml_parser_t *xml, xml_content, size, XML_PARSER_CASE_INSENSITIVE);
      if((result = xml_parser_build_tree_R(xml, &xml_tree)) != XML_PARSER_OK)
	goto __failure;
      
      top_xml_tree = xml_tree;

      /* Check all playlists */
      for(i = 0; guess_functions[i]; i++) {
	if((mmk = guess_functions[i](playlist, filename, xml_content, xml_tree)))
	  break;
      }
      
      xml_parser_free_tree(top_xml_tree);
    __failure:
      
      xml_parser_finalize_R(xml);
      free(xml_content);
    }
  }
  
  return mmk;
}
/*
 * XML based playlists end.
 */

/*
 * Public
 */
int mediamark_get_entry_from_id(const char *ident) {
  gGui_t *gui = gGui;
  if(ident && gui->playlist.num) {
    int i;
    
    for(i = 0; i < gui->playlist.num; i++) {
      if(!strcasecmp(ident, gui->playlist.mmk[i]->ident))
	return i;
    }
  }
  return -1;
}

void mediamark_insert_entry(int index, const char *mrl, const char *ident, 
			    const char *sub, int start, int end, int av_offset, int spu_offset) {
  gGui_t *gui = gGui;
  char  autosub[2*XITK_PATH_MAX + XITK_NAME_MAX + 2];
  char  subpath[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  DIR           *dir;
  struct dirent *dentry;


  gui->playlist.mmk = (mediamark_t **) realloc(gui->playlist.mmk, sizeof(mediamark_t *) * (gui->playlist.num + 2));
  
  if(index < gui->playlist.num)
    memmove(&gui->playlist.mmk[index+1], &gui->playlist.mmk[index], 
	    (gui->playlist.num - index) * sizeof(gui->playlist.mmk[0]) );

  /*
   * If subtitle_autoload is enabled and subtitle is NULL
   * then try to see if a matching subtitle exist 
   */
  if(mrl && (!sub) && gui->subtitle_autoload) {

    if(mrl_look_like_file((char *) mrl)) {
      char        *know_subs = "sub,srt,asc,smi,ssa,ass,txt";
      char        *vsubs, *pvsubs;
      char        *_mrl, *ending, *ext, *path, *d_name;
      struct stat  pstat;
      
      _mrl = (char *) mrl;
      if(!strncasecmp(_mrl, "file:", 5))
	_mrl += 5;
      
      strlcpy(autosub, _mrl, sizeof(autosub));
      
      if((ending = strrchr(autosub, '.')))
	ending++;
      else {
	ending = autosub + strlen(autosub);
	*ending++ = '.';
      }
      
      vsubs = strdup(know_subs);

      pvsubs = vsubs;
      while((ext = xine_strsep(&pvsubs, ",")) && !sub) {
	strcpy(ending, ext);
	*(ending + strlen(ext) + 1) = '\0';
	
	if(((stat(autosub, &pstat)) > -1) && (S_ISREG(pstat.st_mode)) && strcmp(autosub, _mrl))
	  sub = autosub;
      }
      free(vsubs);

      /* try matching "<name>.*.<know_subs>" like used by opensubtitles */
      if( !sub ) {
        *ending = '\0';

        strlcpy(subpath, autosub, sizeof(subpath));
        
        if((d_name = strrchr(subpath, '/'))) {
          *d_name++ = '\0';
          path = subpath;
        } else {
          d_name = subpath;
          path = ".";
        }

        if((dir = opendir(path))) {
          while((dentry = readdir(dir))) {
            if( (strncmp(dentry->d_name, d_name, strlen(d_name)) == 0) &&
                (ending = strrchr(dentry->d_name, '.')) ) {

              if( strstr(know_subs, ending+1) ) {
                snprintf(autosub,sizeof(autosub),"%s/%s",path,dentry->d_name);
                autosub[sizeof(autosub)-1]='\0';
                if(((stat(autosub, &pstat)) > -1) && (S_ISREG(pstat.st_mode)) && strcmp(autosub, _mrl)) {
                  sub = autosub;
                  break;
                }
              }
            }
          }
          closedir(dir);
        }
      }
    }

  }

  if(mediamark_store_mmk(&gui->playlist.mmk[index], 
  			 mrl, ident, sub, start, end, av_offset, spu_offset))
    gui->playlist.num++;
}

void mediamark_append_entry(const char *mrl, const char *ident, 
			    const char *sub, int start, int end, int av_offset, int spu_offset) {
  gGui_t *gui = gGui;

  mediamark_insert_entry(gui->playlist.num, mrl, ident, sub, start, end, av_offset, spu_offset);
}

void mediamark_free_mediamarks(void) {
  gGui_t *gui = gGui;
  
  if(gui->playlist.num) {
    int i;
    
    for(i = 0; i < gui->playlist.num; i++)
      mediamark_free_entry(i);
    
    SAFE_FREE(gui->playlist.mmk);
    gui->playlist.num = 0;
    gui->playlist.cur = -1;
  }
}

void mediamark_reset_played_state(void) {
  gGui_t *gui = gGui;
  
  if(gui->playlist.num) {
    int i;
    
    for(i = 0; i < gui->playlist.num; i++)
      gui->playlist.mmk[i]->played = 0;
  }
}

int mediamark_all_played(void) {
  gGui_t *gui = gGui;
  
  if(gui->playlist.num) {
    int i;
    
    for(i = 0; i < gui->playlist.num; i++) {
      if(gui->playlist.mmk[i]->played == 0)
	return 0;
    }
  }

  return 1;
}

int mediamark_get_shuffle_next(void) {
  gGui_t *gui = gGui;
  int  next = 0;
  
  if(gui->playlist.num >= 3 && gui->playlist.cur >= 0) {
    int    remain = gui->playlist.num;
    int    entries[remain];
    float  num = (float) gui->playlist.num;
    int    found = 0;
    
    memset(&entries, 0, sizeof(int) * remain);

    srandom((unsigned int)time(NULL));
    entries[gui->playlist.cur] = 1;

    do {
      next = (int) (num * random() / RAND_MAX);

      if(next != gui->playlist.cur) {

	if(gui->playlist.mmk[next]->played == 0)
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
      next = gui->playlist.cur;

  }
  else if(gui->playlist.num == 2)
    next = !gui->playlist.cur;
  
  return next;
}

void mediamark_replace_entry(mediamark_t **mmk, 
			     const char *mrl, const char *ident, const char *sub,
			     int start, int end, int av_offset, int spu_offset) {
  SAFE_FREE((*mmk)->mrl);
  SAFE_FREE((*mmk)->ident);
  SAFE_FREE((*mmk)->sub);
  
  mediamark_free_alternates((*mmk));
  (*mmk)->start      = 0;
  (*mmk)->end        = -1;
  (*mmk)->av_offset  = 0;
  (*mmk)->spu_offset = 0;

  
  (void) mediamark_store_mmk(mmk, mrl, ident, sub, start, end, av_offset, spu_offset);
}

mediamark_t *mediamark_get_current_mmk(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur])
    return gui->playlist.mmk[gui->playlist.cur];

  return (mediamark_t *) NULL;
}

mediamark_t *mediamark_get_mmk_by_index(int index) {
  gGui_t *gui = gGui;

  if(index < gui->playlist.num && index >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[index])
    return gui->playlist.mmk[index];

  return (mediamark_t *) NULL;
}

const char *mediamark_get_current_mrl(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur] &&
     gui->playlist.cur < gui->playlist.num)
    return gui->playlist.mmk[gui->playlist.cur]->mrl;

  return NULL;
}

const char *mediamark_get_current_ident(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur])
    return gui->playlist.mmk[gui->playlist.cur]->ident;

  return NULL;
}

const char *mediamark_get_current_sub(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur])
    return gui->playlist.mmk[gui->playlist.cur]->sub;

  return NULL;
}

void mediamark_free_entry(int offset) {
  gGui_t *gui = gGui;

  if(offset < gui->playlist.num && offset >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[offset])
    if(mediamark_free_mmk(&gui->playlist.mmk[offset]))
      gui->playlist.num--;
}

int mediamark_concat_mediamarks(const char *_filename) {
  gGui_t *gui = gGui;
  playlist_t             *playlist;
  int                     i;
  mediamark_t           **mmk = NULL;
  const char             *filename = _filename;
  playlist_guess_func_t   guess_functions[] = {
    guess_xml_based_playlist,
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

  playlist = (playlist_t *) calloc(1, sizeof(playlist_t));

  for(i = 0; guess_functions[i]; i++) {
    if((mmk = guess_functions[i](playlist, filename)))
      break;
  }
  
  if(mmk) {
#ifdef DEBUG
    printf("Playlist file (%s) is valid (%s).\n", filename, playlist->type);
#endif
  }
  else {
    fprintf(stderr, _("Playlist file (%s) is invalid.\n"), filename);
    SAFE_FREE(playlist);
    return 0;
  }

  gui->playlist.cur = gui->playlist.num;

  for(i = 0; i < playlist->entries; i++)
    mediamark_append_entry(mmk[i]->mrl, mmk[i]->ident, mmk[i]->sub, 
			   mmk[i]->start, mmk[i]->end, mmk[i]->av_offset, mmk[i]->spu_offset);
  
  for(i = 0; i < playlist->entries; i++)
    (void) mediamark_free_mmk(&mmk[i]);
  
  SAFE_FREE(mmk);
  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);

  return 1;
}

void mediamark_load_mediamarks(const char *_filename) {
  gGui_t *gui = gGui;
  playlist_t             *playlist;
  int                     i, onum;
  mediamark_t           **mmk = NULL;
  mediamark_t           **ommk;
  const char             *filename = _filename;
  playlist_guess_func_t   guess_functions[] = {
    guess_xml_based_playlist,
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

  playlist = (playlist_t *) calloc(1, sizeof(playlist_t));

  for(i = 0; guess_functions[i]; i++) {
    if((mmk = guess_functions[i](playlist, filename)))
      break;
  }
  
  if(mmk) {
#ifdef DEBUG
    printf("Playlist file (%s) is valid (%s).\n", filename, playlist->type);
#endif
  }
  else {
    fprintf(stderr, _("Playlist file (%s) is invalid.\n"), filename);
    SAFE_FREE(playlist);
    return;
  }

  ommk = gui->playlist.mmk;
  onum = gui->playlist.num;
  
  gui->playlist.mmk = mmk;
  gui->playlist.num = playlist->entries;

  if(gui->playlist.loop == PLAYLIST_LOOP_SHUFFLE)
    gui->playlist.cur = mediamark_get_shuffle_next();
  else
    gui->playlist.cur = 0;
  
  for(i = 0; i < onum; i++)
    (void) mediamark_free_mmk(&ommk[i]);
  
  SAFE_FREE(ommk);

  SAFE_FREE(playlist->type);
  SAFE_FREE(playlist);
}

void mediamark_save_mediamarks(const char *filename) {
  gGui_t *gui = gGui;
  char *fullfn;
  char *pn;
  char *fn;
  int   status = 1;

  if(!gui->playlist.num)
    return;

  fullfn = strdup(filename);
  
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
    const char *store_item;
    char buffer[_PATH_MAX + _NAME_MAX + 2], current_dir[_PATH_MAX];

    if (getcwd(current_dir, sizeof(current_dir)) > 0) {
      if (current_dir[strlen(current_dir) - 1] != '/')
        strlcat(current_dir, "/", sizeof(current_dir));
    } else
      strcpy(current_dir, "");

    if((fd = fopen(filename, "w")) != NULL) {

      fprintf(fd, "# toxine playlist\n");
      
      for(i = 0; i < gui->playlist.num; i++) {
	fprintf(fd, "\nentry {\n");
	fprintf(fd, "\tidentifier = %s;\n", gui->playlist.mmk[i]->ident);
	store_item = concat_basedir(buffer, sizeof(buffer), current_dir, gui->playlist.mmk[i]->mrl);
	fprintf(fd, "\tmrl = %s;\n", store_item);
	if(gui->playlist.mmk[i]->sub) {
	  store_item = concat_basedir(buffer, sizeof(buffer), current_dir, gui->playlist.mmk[i]->sub);
	  fprintf(fd, "\tsubtitle = %s;\n", store_item);
	}
	if(gui->playlist.mmk[i]->start > 0)
	  fprintf(fd, "\tstart = %d;\n", gui->playlist.mmk[i]->start);
	if(gui->playlist.mmk[i]->end != -1)
	  fprintf(fd, "\tend = %d;\n", gui->playlist.mmk[i]->end);
	if(gui->playlist.mmk[i]->av_offset != 0)
	  fprintf(fd, "\tav_offset = %d;\n", gui->playlist.mmk[i]->av_offset);
	if(gui->playlist.mmk[i]->spu_offset != 0)
	  fprintf(fd, "\tspu_offset = %d;\n", gui->playlist.mmk[i]->spu_offset);
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

  free(fullfn);
}

int mrl_look_like_file(char *mrl) {
  
  if(mrl && strlen(mrl)) {
    if((strncasecmp(mrl, "file:", 5)) && 
       strstr (mrl, ":/") && (strstr (mrl, ":/") < strchr(mrl, '/')))
      return 0;
  }
  
  return 1;
}

void mediamark_collect_from_directory(char *filepathname) {
  DIR           *dir;
  struct dirent *dentry;
  
  if((dir = opendir(filepathname))) {
    
    while((dentry = readdir(dir))) {
      char fullpathname[XITK_PATH_MAX + XITK_NAME_MAX + 2] = "";
      
      snprintf(fullpathname, sizeof(fullpathname) - 1, "%s/%s", filepathname, dentry->d_name);
      
      if(strlen(fullpathname)) {

	if(fullpathname[strlen(fullpathname) - 1] == '/')
	  fullpathname[strlen(fullpathname) - 1] = '\0';
	
	if(is_a_dir(fullpathname)) {
	  if(!((strlen(dentry->d_name) == 1) && (dentry->d_name[0] == '.'))
	     && !((strlen(dentry->d_name) == 2) && 
		  ((dentry->d_name[0] == '.') && dentry->d_name[1] == '.'))) {
	    mediamark_collect_from_directory(fullpathname);
	  }
	}
	else {
	  char *p, *extension;
	  char  loname[XITK_PATH_MAX + XITK_NAME_MAX + 2] = "";
	  
	  p = strncat(loname, fullpathname, strlen(fullpathname));
	  while(*p && (*p != '\0')) {
	    *p = tolower(*p);
	    p++;
	  }
	  
	  if((extension = strrchr(loname, '.')) && (strlen(extension) > 1)) {
	    char  ext[strlen(extension) + 2];
	    char *valid_endings = 
	      ".pls .m3u .sfv .tox .asx .smi .smil .xml .fxd " /* Playlists */
	      ".4xm .ac3 .aif .aiff .asf .wmv .wma .wvx .wax .aud .avi .cin .cpk .cak "
	      ".film .dv .dif .fli .flc .mjpg .mov .qt .m2p .mp4 .mp3 .mp2 .mpa .mpega .mpg .mpeg "
	      ".mpv .mve .mv8 .nsf .nsv .ogg .ogm .spx .pes .png .mng .pva .ra .rm "
	      ".ram .rmvb .roq .snd .au .str .iki .ik2 .dps .dat .xa .xa1 .xa2 .xas .xap .ts .m2t "
	      ".trp .vob .voc .vox .vqa .wav .wve .y4m ";
	    
	    snprintf(ext, sizeof(ext), "%s ", extension);
	    
	    if(strstr(valid_endings, ext)) {
	      mediamark_append_entry((const char *)fullpathname, 
				     (const char *)fullpathname, NULL, 0, -1, 0, 0);
	    }	    
	  }
	}
      }
    }
    closedir(dir);
  }
}

/*
 *  EDITOR
 */
static void mmkeditor_exit(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if(mmkeditor.running) {
    window_info_t wi;

    mmkeditor.running = 0;
    mmkeditor.visible = 0;
    
    if((xitk_get_window_info(mmkeditor.widget_key, &wi))) {
      config_update_num ("gui.mmk_editor_x", wi.x);
      config_update_num ("gui.mmk_editor_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    mmkeditor.mmk = NULL;
    
    xitk_unregister_event_handler(&mmkeditor.widget_key);
    
    xitk_destroy_widgets(mmkeditor.widget_list);
    xitk_window_destroy_window(gui->imlib_data, mmkeditor.xwin);
    
    mmkeditor.xwin = NULL;
    /* xitk_dlist_init (&mmkeditor.widget_list.list); */
    
    gui->x_lock_display (gui->display);
    XFreeGC(gui->display, (XITK_WIDGET_LIST_GC(mmkeditor.widget_list)));
    gui->x_unlock_display (gui->display);
    
    XITK_WIDGET_LIST_FREE(mmkeditor.widget_list);
    
    playlist_get_input_focus();
  }
}

static void mmkeditor_handle_event(XEvent *event, void *data) {
  switch(event->type) {
    
  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      mmkeditor_exit(NULL, NULL);
    else
      gui_handle_event (event, gGui);
  }
}

void mmk_editor_show_tips(int enabled, unsigned long timeout) {
  
  if(mmkeditor.running) {
    if(enabled)
      xitk_set_widgets_tips_timeout(mmkeditor.widget_list, timeout);
    else
      xitk_disable_widgets_tips(mmkeditor.widget_list);
  }
}

void mmk_editor_update_tips_timeout(unsigned long timeout) {
  if(mmkeditor.running)
    xitk_set_widgets_tips_timeout(mmkeditor.widget_list, timeout);
}

int mmk_editor_is_visible(void) {
  if(mmkeditor.running)
    return mmkeditor.visible;
  
  return 0;
}

int mmk_editor_is_running(void) {
  return mmkeditor.running;
}

void mmk_editor_raise_window(void) {
  if(mmkeditor.running)
    raise_window(xitk_window_get_window(mmkeditor.xwin), mmkeditor.visible, mmkeditor.running);
}

void mmk_editor_toggle_visibility(void) {
  if(mmkeditor.running)
    toggle_window(xitk_window_get_window(mmkeditor.xwin), mmkeditor.widget_list,
		  &mmkeditor.visible, mmkeditor.running);
}

void mmk_editor_end(void) {
  mmkeditor_exit(NULL, NULL);
}

void mmkeditor_set_mmk(mediamark_t **mmk) {
  
  if(mmkeditor.running) {
    mmkeditor.mmk = mmk;
    
    xitk_inputtext_change_text(mmkeditor.mrl, (*mmk)->mrl);
    xitk_inputtext_change_text(mmkeditor.ident, (*mmk)->ident);
    xitk_inputtext_change_text(mmkeditor.sub, (*mmk)->sub);
    xitk_intbox_set_value(mmkeditor.start, (*mmk)->start);
    xitk_intbox_set_value(mmkeditor.end, (*mmk)->end);
    xitk_intbox_set_value(mmkeditor.av_offset, (*mmk)->av_offset);
    xitk_intbox_set_value(mmkeditor.spu_offset, (*mmk)->spu_offset);
  }
}

static void mmkeditor_apply(xitk_widget_t *w, void *data) {
  const char *sub;
  char       *ident, *mrl;
  int         start, end, av_offset, spu_offset;

  if(mmkeditor.mmk) {
    
    mrl = atoa(xitk_inputtext_get_text(mmkeditor.mrl));
    if (!mrl[0])
      mrl = strdup((*mmkeditor.mmk)->mrl);
    else
      mrl = strdup(mrl);
    
    ident = atoa(xitk_inputtext_get_text(mmkeditor.ident));
    if (!ident[0])
      ident = strdup(mrl);
    else
      ident = strdup(ident);

    sub = xitk_inputtext_get_text(mmkeditor.sub);
    if(sub && (!strlen(sub)))
      sub = NULL;
    
    if((start = xitk_intbox_get_value(mmkeditor.start)) < 0)
      start = 0;

    if((end = xitk_intbox_get_value(mmkeditor.end)) <= -1)
      end = -1;
    else if (end < start)
      end = start + 1;
    
    av_offset  = xitk_intbox_get_value(mmkeditor.av_offset);
    spu_offset = xitk_intbox_get_value(mmkeditor.spu_offset);
    
    mediamark_replace_entry(mmkeditor.mmk, mrl, ident, sub, start, end, av_offset, spu_offset);
    if(mmkeditor.callback)
      mmkeditor.callback(mmkeditor.user_data);

    free(mrl);
    free(ident);
  }  
}

static void mmkeditor_ok(xitk_widget_t *w, void *data) {
  mmkeditor_apply(NULL, NULL);
  mmkeditor_exit(NULL, NULL);
}

static void mmk_fileselector_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
  char *file, *dir;

  if ((dir = filebrowser_get_current_dir(fb)) != NULL) {
    strlcpy(gui->curdir, dir, sizeof(gui->curdir));
    free(dir);
  }

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    if(file)
      xitk_inputtext_change_text(mmkeditor.sub, file);
    free(file);
  }
  
}
static void mmkeditor_select_sub(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  filebrowser_callback_button_t  cbb;
  char                           *path, *open_path;
  
  cbb.label = _("Select");
  cbb.callback = mmk_fileselector_callback;
  cbb.need_a_file = 1;
  
  path = (*mmkeditor.mmk)->sub ? (*mmkeditor.mmk)->sub : (*mmkeditor.mmk)->mrl;
  
  if(mrl_look_like_file(path)) {
    char *p;
    
    open_path = strdup(path);
    
    if(!strncasecmp(path, "file:", 5))
      path += 5;

    p = strrchr(open_path, '/');
    if (p && strlen(p))
      *p = '\0';
  }
  else
    open_path = strdup(gui->curdir);
  
  create_filebrowser(_("Pick a subtitle file"), open_path, hidden_file_cb, &cbb, NULL, NULL);

  free(open_path);
}

void mmk_edit_mediamark(mediamark_t **mmk, apply_callback_t callback, void *data) {
  gGui_t *gui = gGui;
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_inputtext_widget_t     inp;
  xitk_intbox_widget_t        ib;
  xitk_pixmap_t              *bg;
  xitk_widget_t              *b;
  int                         x, y, w, width, height;

  if(mmkeditor.running) {
    if(!mmkeditor.visible)
      mmkeditor.visible = !mmkeditor.visible;
    mmk_editor_raise_window();
    mmkeditor_set_mmk(mmk);
    return;
  }

  mmkeditor.callback = callback;
  mmkeditor.user_data = data;

  x = xine_config_register_num(__xineui_global_xine_instance, "gui.mmk_editor_x", 
			       80,
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  y = xine_config_register_num(__xineui_global_xine_instance, "gui.mmk_editor_y",
			       80,
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  
  /* Create window */
  mmkeditor.xwin = xitk_window_create_dialog_window(gui->imlib_data, _("Mediamark Editor"), x, y,
						     WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start((xitk_window_get_window(mmkeditor.xwin)));
  
  gui->x_lock_display (gui->display);
  gc = XCreateGC(gui->display, 
		 (xitk_window_get_window(mmkeditor.xwin)), None, None);
  gui->x_unlock_display (gui->display);
  
  mmkeditor.widget_list = xitk_widget_list_new();
  xitk_widget_list_set(mmkeditor.widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(mmkeditor.xwin)));
  xitk_widget_list_set(mmkeditor.widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gui->imlib_data);
  XITK_WIDGET_INIT(&cb, gui->imlib_data);
  XITK_WIDGET_INIT(&inp, gui->imlib_data);
  XITK_WIDGET_INIT(&ib, gui->imlib_data);

  xitk_window_get_window_size(mmkeditor.xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gui->imlib_data, width, height);
  gui->x_lock_display (gui->display);
  XCopyArea(gui->display, (xitk_window_get_background(mmkeditor.xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  gui->x_unlock_display (gui->display);

  x = 15;
  y = 34 - 6;
  w = WINDOW_WIDTH - 30;
  draw_outter_frame(gui->imlib_data, bg, _("Identifier"), btnfontname, 
		    x, y, w, 45);

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  mmkeditor.ident = xitk_noskin_inputtext_create (mmkeditor.widget_list, &inp,
    x + 10, y + 16, w - 20, 20, "Black", "Black", fontname);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.ident);
  xitk_set_widget_tips_default(mmkeditor.ident, _("Mediamark Identifier"));
  xitk_enable_and_show_widget(mmkeditor.ident);

  y += 45 + 3;
  draw_outter_frame(gui->imlib_data, bg, _("Mrl"), btnfontname, 
		    x, y, w, 45);

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  mmkeditor.mrl = xitk_noskin_inputtext_create (mmkeditor.widget_list, &inp,
    x + 10, y + 16, w - 20, 20, "Black", "Black", fontname);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.mrl);
  xitk_set_widget_tips_default(mmkeditor.mrl, _("Mediamark Mrl"));
  xitk_enable_and_show_widget(mmkeditor.mrl);

  y += 45 + 3;
  draw_outter_frame(gui->imlib_data, bg, _("Subtitle"), btnfontname, 
		    x, y, w, 45);

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 2048;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  mmkeditor.sub = xitk_noskin_inputtext_create (mmkeditor.widget_list, &inp,
    x + 10, y + 16, w - 20 - 100 - 10, 20, "Black", "Black", fontname);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.sub);
  xitk_set_widget_tips_default(mmkeditor.sub, _("Subtitle File"));
  xitk_enable_and_show_widget(mmkeditor.sub);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Sub File");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_select_sub; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  b =  xitk_noskin_labelbutton_create (mmkeditor.widget_list, &lb,
    x + 10 + w - 20 - 100, y + 16, 100, 20, "Black", "Black", "White", btnfontname);
  xitk_add_widget (mmkeditor.widget_list, b);
  xitk_set_widget_tips_default(b, _("Select a subtitle file to use together with the mrl."));
  xitk_enable_and_show_widget(b);

  y += 45 + 3;
  w = 120;
  draw_outter_frame(gui->imlib_data, bg, _("Start at"), btnfontname, 
		    x, y, w, 45);

  ib.skin_element_name = NULL;
  ib.value             = 0;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor.widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  mmkeditor.start = xitk_noskin_intbox_create (mmkeditor.widget_list, &ib,
    x + 30, y + 16, w - 60, 20, NULL, NULL, NULL);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.start);
  xitk_set_widget_tips_default(mmkeditor.start, _("Mediamark start time (secs)."));
  xitk_enable_and_show_widget(mmkeditor.start);

  x += w + 5;
  draw_outter_frame(gui->imlib_data, bg, _("End at"), btnfontname, 
		    x, y, w, 45);

  ib.skin_element_name = NULL;
  ib.value             = -1;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor.widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  mmkeditor.end = xitk_noskin_intbox_create (mmkeditor.widget_list, &ib,
    x + 30, y + 16, w - 60, 20, NULL, NULL, NULL);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.end);
  xitk_set_widget_tips_default(mmkeditor.end, _("Mediamark end time (secs)."));
  xitk_enable_and_show_widget(mmkeditor.end);

  x += w + 5;
  draw_outter_frame(gui->imlib_data, bg, _("A/V offset"), btnfontname, 
		    x, y, w, 45);

  ib.skin_element_name = NULL;
  ib.value             = 0;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor.widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  mmkeditor.av_offset = xitk_noskin_intbox_create (mmkeditor.widget_list, &ib,
    x + 30, y + 16, w - 60, 20, NULL, NULL, NULL);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.av_offset);
  xitk_set_widget_tips_default(mmkeditor.av_offset, _("Offset of Audio and Video."));
  xitk_enable_and_show_widget(mmkeditor.av_offset);

  x += w + 5;
  draw_outter_frame(gui->imlib_data, bg, _("SPU offset"), btnfontname, 
		    x, y, w, 45);

  ib.skin_element_name = NULL;
  ib.value             = 0;
  ib.step              = 1;
  ib.parent_wlist      = mmkeditor.widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  mmkeditor.spu_offset = xitk_noskin_intbox_create (mmkeditor.widget_list, &ib,
    x + 30, y + 16, w - 60, 20, NULL, NULL, NULL);
  xitk_add_widget (mmkeditor.widget_list, mmkeditor.spu_offset);
  xitk_set_widget_tips_default(mmkeditor.spu_offset, _("Subpicture offset."));
  xitk_enable_and_show_widget(mmkeditor.spu_offset);

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("OK");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_ok; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  b =  xitk_noskin_labelbutton_create (mmkeditor.widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (mmkeditor.widget_list, b);
  xitk_set_widget_tips_default(b, _("Apply the changes and close the window."));
  xitk_enable_and_show_widget(b);

  x = (WINDOW_WIDTH - 100) / 2;
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_apply; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  b =  xitk_noskin_labelbutton_create (mmkeditor.widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (mmkeditor.widget_list, b);
  xitk_set_widget_tips_default(b, _("Apply the changes to the playlist."));
  xitk_enable_and_show_widget(b);

  x = WINDOW_WIDTH - (100 + 15);
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = mmkeditor_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  b =  xitk_noskin_labelbutton_create (mmkeditor.widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (mmkeditor.widget_list, b);
  xitk_set_widget_tips_default(b, _("Discard changes and dismiss the window."));
  xitk_enable_and_show_widget(b);
  mmk_editor_show_tips (panel_get_tips_enable (gui->panel), panel_get_tips_timeout (gui->panel));

  xitk_window_change_background(gui->imlib_data, mmkeditor.xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);

  mmkeditor.widget_key = xitk_register_event_handler("mmkeditor", 
						      (xitk_window_get_window(mmkeditor.xwin)),
						      mmkeditor_handle_event,
						      NULL,
						      NULL,
						      mmkeditor.widget_list,
						      NULL);
  
  mmkeditor.visible = 1;
  mmkeditor.running = 1;

  mmkeditor_set_mmk(mmk);
  mmk_editor_raise_window();

  try_to_set_input_focus(xitk_window_get_window(mmkeditor.xwin));
}

