/*
 * Copyright (C) 2000-2002 the xine project
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

#ifdef HAVE_CURL
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#endif

#include "common.h"

extern gGui_t   *gGui;

#ifdef HAVE_CURL
static int progress_callback(void *userdata, 
			     double dltotal, double dlnow, double ultotal, double ulnow) {
  download_t  *download = (download_t *) userdata;
  char         buffer[1024];
  int          percent = (dltotal > 0.0) ? (int) (dlnow * 100.0 / dltotal) : 0;

  osd_draw_bar(_("Download in progress"), 0, 100, percent, OSD_BAR_POS);
  memset(&buffer, 0, sizeof(buffer));
  sprintf(buffer, _("Download progress: %d%%."), percent);
  gGui->mrl_overrided = 3;
  panel_set_title(buffer);
  
  /* return non 0 abort transfert */
  return download->status;
}

static int store_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
  download_t  *download = (download_t *) userdata;
  int         rsize = size * nmemb;
  
  if(download->size == 0)
    download->buf = (char *) malloc(rsize);
  else
    download->buf = (char *) realloc(download->buf, download->size + rsize + 1);
  
  if(download->buf) {
    memcpy(&(download->buf[download->size]), ptr, rsize);
    download->size += rsize;
    download->buf[download->size] = 0;
  }
  else
    download->status = 1; /* error */
  
  return rsize;
}
#endif

int network_download(const char *url, download_t *download) {
#ifdef HAVE_CURL
  CURL        *curl;
  CURLcode     res;
  
  curl_global_init(CURL_GLOBAL_DEFAULT);
  
  if((curl = curl_easy_init()) != NULL) {
    char error_buffer[CURL_ERROR_SIZE + 1];
    char user_agent[256];
    
    memset(&error_buffer, 0, sizeof(error_buffer));
    
    memset(&user_agent, 0, sizeof(user_agent));
    sprintf(user_agent, "User-Agent: xine/%s", VERSION);
    
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 
#ifdef DEBUG
		     TRUE
#else
		     FALSE
#endif
		     );
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, TRUE);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, TRUE); 
    curl_easy_setopt(curl, CURLOPT_NETRC, TRUE);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)download);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, (void *)download);
    
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buffer);
    
    if((res = curl_easy_perform(curl)) != 0) {
      download->error = strdup((strlen(error_buffer)) ? error_buffer : "Unknown error");
      download->status = 1;
    }
    
    curl_easy_cleanup(curl);
  }
  
  curl_global_cleanup();
  
  return (download->status == 0);
#else
  return 0;
#endif
}
