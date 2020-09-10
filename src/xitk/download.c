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

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include "common.h"
#include "download.h"
#include "panel.h"

#ifdef HAVE_CURL

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static int progress_callback(void *userdata, 
			     double dltotal, double dlnow, double ultotal, double ulnow) {
  download_t  *download = (download_t *) userdata;
  char         buffer[1024];
  int          percent = (dltotal > 0.0) ? (int) (dlnow * 100.0 / dltotal) : 0;
  
  osd_draw_bar(_("Download in progress"), 0, 100, percent, OSD_BAR_POS);
  /* TRANSLATORS: only ASCII characters (skin) */
  snprintf(buffer, sizeof(buffer), pgettext("skin", "Download progress: %d%%."), percent);
  gGui->mrl_overrided = 3;
  panel_set_title (gGui->panel, buffer);
  
  /* return non 0 abort transfert */
  return download->status;
}

static size_t store_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
  download_t  *download = (download_t *) userdata;
  int         rsize = size * nmemb;

  if(download->size == 0)
    download->buf = (char *) calloc((rsize + 1), sizeof(char));
  else
    download->buf = (char *) realloc(download->buf, sizeof(char) * (download->size + rsize + 1));
  
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

  pthread_mutex_lock(&gGui->download_mutex);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  
  if((curl = curl_easy_init()) != NULL) {
    char error_buffer[CURL_ERROR_SIZE];
    
    memset(&error_buffer, 0, sizeof(error_buffer));
    
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 
#ifdef DEBUG
		     TRUE
#else
		     FALSE
#endif
		     );
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "xine/" VERSION);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, TRUE);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, TRUE); 
    curl_easy_setopt(curl, CURLOPT_NETRC, TRUE);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)download);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, (void *)download);
    
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    
    if(curl_easy_perform(curl)) {
      error_buffer[sizeof(error_buffer) - 1] = 0;
      download->error = strdup((strlen(error_buffer)) ? error_buffer : "Unknown error");
      download->status = 1;
    }
    
    curl_easy_cleanup(curl);
  }
  else {
    download->error = strdup("Cannot initialize");
    download->status = 1;
  }
  
  curl_global_cleanup();
  
  pthread_mutex_unlock(&gGui->download_mutex);

  return (download->status == 0);
#else
  return 0;
#endif
}
