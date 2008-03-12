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

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include "_xitk.h"
#include "recode.h"

typedef struct {
  char                    *language;
  char                    *encoding;
  char                    *modifier;
} lang_locale_t;

static const lang_locale_t lang_locales[] = {
  { "af_ZA",    "iso88591",   NULL       },
  { "ar_AE",    "iso88596",   NULL       },
  { "ar_BH",    "iso88596",   NULL       },
  { "ar_DZ",    "iso88596",   NULL       },
  { "ar_EG",    "iso88596",   NULL       },
  { "ar_IN",    "utf8",       NULL       },
  { "ar_IQ",    "iso88596",   NULL       },
  { "ar_JO",    "iso88596",   NULL       },
  { "ar_KW",    "iso88596",   NULL       },
  { "ar_LB",    "iso88596",   NULL       },
  { "ar_LY",    "iso88596",   NULL       },
  { "ar_MA",    "iso88596",   NULL       },
  { "ar_OM",    "iso88596",   NULL       },
  { "ar_QA",    "iso88596",   NULL       },
  { "ar_SA",    "iso88596",   NULL       },
  { "ar_SD",    "iso88596",   NULL       },
  { "ar_SY",    "iso88596",   NULL       },
  { "ar_TN",    "iso88596",   NULL       },
  { "ar_YE",    "iso88596",   NULL       },
  { "be_BY",    "cp1251",     NULL       },
  { "bg_BG",    "cp1251",     NULL       },
  { "br_FR",    "iso88591",   NULL       },
  { "bs_BA",    "iso88592",   NULL       },
  { "ca_ES",    "iso88591",   NULL       },
  { "ca_ES",    "iso885915",  "euro"     },
  { "cs_CZ",    "iso88592",   NULL       },
  { "cy_GB",    "iso885914",  NULL       },
  { "da_DK",    "iso88591",   NULL       },
  { "de_AT",    "iso88591",   NULL       },
  { "de_AT",    "iso885915",  "euro"     },
  { "de_BE",    "iso88591",   NULL       },
  { "de_BE",    "iso885915",  "euro"     },
  { "de_CH",    "iso88591",   NULL       },
  { "de_DE",    "iso88591",   NULL       },
  { "de_DE",    "iso885915",  "euro"     },
  { "de_LU",    "iso88591",   NULL       },
  { "de_LU",    "iso885915",  "euro"     },
  { "el_GR",    "iso88597",   NULL       },
  { "en_AU",    "iso88591",   NULL       },
  { "en_BW",    "iso88591",   NULL       },
  { "en_CA",    "iso88591",   NULL       },
  { "en_DK",    "iso88591",   NULL       },
  { "en_GB",    "iso88591",   NULL       },
  { "en_HK",    "iso88591",   NULL       },
  { "en_IE",    "iso88591",   NULL       },
  { "en_IE",    "iso885915",  "euro"     },
  { "en_IN",    "utf8",       NULL       },
  { "en_NZ",    "iso88591",   NULL       },
  { "en_PH",    "iso88591",   NULL       },
  { "en_SG",    "iso88591",   NULL       },
  { "en_US",    "iso88591",   NULL       },
  { "en_ZA",    "iso88591",   NULL       },
  { "en_ZW",    "iso88591",   NULL       },
  { "es_AR",    "iso88591",   NULL       },
  { "es_BO",    "iso88591",   NULL       },
  { "es_CL",    "iso88591",   NULL       },
  { "es_CO",    "iso88591",   NULL       },
  { "es_CR",    "iso88591",   NULL       },
  { "es_DO",    "iso88591",   NULL       },
  { "es_EC",    "iso88591",   NULL       },
  { "es_ES",    "iso88591",   NULL       },
  { "es_ES",    "iso885915",  "euro"     },
  { "es_GT",    "iso88591",   NULL       },
  { "es_HN",    "iso88591",   NULL       },
  { "es_MX",    "iso88591",   NULL       },
  { "es_NI",    "iso88591",   NULL       },
  { "es_PA",    "iso88591",   NULL       },
  { "es_PE",    "iso88591",   NULL       },
  { "es_PR",    "iso88591",   NULL       },
  { "es_PY",    "iso88591",   NULL       },
  { "es_SV",    "iso88591",   NULL       },
  { "es_US",    "iso88591",   NULL       },
  { "es_UY",    "iso88591",   NULL       },
  { "es_VE",    "iso88591",   NULL       },
  { "et_EE",    "iso88591",   NULL       },
  { "eu_ES",    "iso88591",   NULL       },
  { "eu_ES",    "iso885915",  "euro"     },
  { "fa_IR",    "utf8",       NULL       },
  { "fi_FI",    "iso88591",   NULL       },
  { "fi_FI",    "iso885915",  "euro"     },
  { "fo_FO",    "iso88591",   NULL       },
  { "fr_BE",    "iso88591",   NULL       },
  { "fr_BE",    "iso885915",  "euro"     },
  { "fr_CA",    "iso88591",   NULL       },
  { "fr_CH",    "iso88591",   NULL       },
  { "fr_FR",    "iso88591",   NULL       },
  { "fr_FR",    "iso885915",  "euro"     },
  { "fr_LU",    "iso88591",   NULL       },
  { "fr_LU",    "iso885915",  "euro"     },
  { "ga_IE",    "iso88591",   NULL       },
  { "ga_IE",    "iso885915",  "euro"     },
  { "gl_ES",    "iso88591",   NULL       },
  { "gl_ES",    "iso885915",  "euro"     },
  { "gv_GB",    "iso88591",   NULL       },
  { "he_IL",    "iso88598",   NULL       },
  { "hi_IN",    "utf8",       NULL       },
  { "hr_HR",    "iso88592",   NULL       },
  { "hu_HU",    "iso88592",   NULL       },
  { "id_ID",    "iso88591",   NULL       },
  { "is_IS",    "iso88591",   NULL       },
  { "it_CH",    "iso88591",   NULL       },
  { "it_IT",    "iso88591",   NULL       },
  { "it_IT",    "iso885915",  "euro"     },
  { "iw_IL",    "iso88598",   NULL       },
  { "ja_JP",    "utf8",       NULL       },
  { "ja_JP",    "eucjp",      NULL       },
  { "ja_JP",    "ujis",       NULL       },
  { "japanese", "euc",        NULL       },
  { "ka_GE",    "georgianps", NULL       },
  { "kl_GL",    "iso88591",   NULL       },
  { "ko_KR",    "euckr",      NULL       },
  { "ko_KR",    "utf8",       NULL       },
  { "korean",   "euc",        NULL       },
  { "kw_GB",    "iso88591",   NULL       },
  { "lt_LT",    "iso885913",  NULL       },
  { "lv_LV",    "iso885913",  NULL       },
  { "mi_NZ",    "iso885913",  NULL       },
  { "mk_MK",    "iso88595",   NULL       },
  { "mr_IN",    "utf8",       NULL       },
  { "ms_MY",    "iso88591",   NULL       },
  { "mt_MT",    "iso88593",   NULL       },
  { "nb_NO",    "ISO-8859-1", NULL       },
  { "nl_BE",    "iso88591",   NULL       },
  { "nl_BE",    "iso885915",  "euro"     },
  { "nl_NL",    "iso88591",   NULL       },
  { "nl_NL",    "iso885915",  "euro"     },
  { "nn_NO",    "iso88591",   NULL       },
  { "no_NO",    "iso88591",   NULL       },
  { "oc_FR",    "iso88591",   NULL       },
  { "pl_PL",    "iso88592",   NULL       },
  { "pt_BR",    "iso88591",   NULL       },
  { "pt_PT",    "iso88591",   NULL       },
  { "pt_PT",    "iso885915",  "euro"     },
  { "ro_RO",    "iso88592",   NULL       },
  { "ru_RU",    "iso88595",   NULL       },
  { "ru_RU",    "koi8r",      NULL       },
  { "ru_UA",    "koi8u",      NULL       },
  { "se_NO",    "utf8",       NULL       },
  { "sk_SK",    "iso88592",   NULL       },
  { "sl_SI",    "iso88592",   NULL       },
  { "sq_AL",    "iso88591",   NULL       },
  { "sr_YU",    "iso88592",   NULL       },
  { "sr_YU",    "iso88595",   "cyrillic" },
  { "sv_FI",    "iso88591",   NULL       },
  { "sv_FI",    "iso885915",  "euro"     },
  { "sv_SE",    "iso88591",   NULL       },
  { "ta_IN",    "utf8",       NULL       },
  { "te_IN",    "utf8",       NULL       },
  { "tg_TJ",    "koi8t",      NULL       },
  { "th_TH",    "tis620",     NULL       },
  { "tl_PH",    "iso88591",   NULL       },
  { "tr_TR",    "iso88599",   NULL       },
  { "uk_UA",    "koi8u",      NULL       },
  { "ur_PK",    "utf8",       NULL       },
  { "uz_UZ",    "iso88591",   NULL       },
  { "vi_VN",    "tcvn",       NULL       },
  { "vi_VN",    "utf8",       NULL       },
  { "wa_BE",    "iso88591",   NULL       },
  { "wa_BE",    "iso885915",  "euro"     },
  { "yi_US",    "cp1255",     NULL       },
  { "zh_CN",    "gb18030",    NULL       },
  { "zh_CN",    "gb2312",     NULL       },
  { "zh_CN",    "gbk",        NULL       },
  { "zh_HK",    "big5hkscs",  NULL       },
  { "zh_TW",    "big5",       NULL       },
  { "zh_TW",    "euctw",      NULL       },
  { NULL,       NULL,         NULL       }
};

#if 0
static const lang_locale_t *_get_next_lang_locale(char *lcal, const lang_locale_t *plocale) {
  if(lcal && strlen(lcal) && plocale) {
    const lang_locale_t *llocale = plocale;

    llocale++;

    while(llocale && llocale->language) {
      if(!strncmp(lcal, llocale->language, strlen(lcal)))
	return llocale;
      
      llocale++;
    }
  }
  return NULL;
}
#endif

static const lang_locale_t *_get_first_lang_locale(char *lcal) {
  const lang_locale_t *llocale;

  if(lcal && strlen(lcal)) {
    llocale = &*lang_locales;
    
    while(llocale->language) {
      if(!strncmp(lcal, llocale->language, strlen(lcal)))
	return llocale;
      
      llocale++;
    }
  }
  return NULL;
}

static char *xitk_get_system_encoding(void) {
  char *lang, *codeset = NULL;
  
#ifdef HAVE_LANGINFO_CODESET
  codeset = nl_langinfo(CODESET);
#endif
  /*
   * guess locale codeset according to shell variables
   * when nl_langinfo(CODESET) isn't available or working
   */
  if (!codeset || strstr(codeset, "ANSI") != 0) {
    if(!(lang = getenv("LC_ALL")))
      if(!(lang = getenv("LC_MESSAGES")))
        lang = getenv("LANG");

    codeset = NULL;

    if(lang) {
      char *lg, *enc, *mod;

      lg = strdup(lang);

      if((enc = strchr(lg, '.')) && (strlen(enc) > 1)) {
        enc++;

        if((mod = strchr(enc, '@')))
          *mod = '\0';

        codeset = strdup(enc);
      }
      else {
        const lang_locale_t *llocale = _get_first_lang_locale(lg);

        if(llocale && llocale->encoding)
          codeset = strdup(llocale->encoding);
      }

      free(lg);
    }
  } else
    codeset = strdup(codeset);

  return codeset;
}

xitk_recode_t *xitk_recode_init(const char *src_encoding, const char *dst_encoding) {
  xitk_recode_t  *xr = NULL;
#ifdef HAVE_ICONV
  iconv_t         id;
  char           *src_enc = NULL, *dst_enc = NULL;
  
  if (src_encoding)
    src_enc = xitk_get_system_encoding();
  else
    src_enc = strdup(src_encoding);

  if (!src_enc)
    goto end;
  
  if (dst_encoding)
    dst_enc = xitk_get_system_encoding();
  else
    dst_enc = strdup(dst_encoding);

  if (!dst_enc)
    goto end;
  
  if ((id = iconv_open(dst_enc, src_enc)) == (iconv_t)-1)
    goto end;
  
  xr = (xitk_recode_t *) xitk_xmalloc(sizeof(xitk_recode_t));
  xr->id = id;
  
 end:
  free(dst_enc);
  free(src_enc);
#endif
  return xr;
}

void xitk_recode_done(xitk_recode_t *xr) {
  if (xr) {
#ifdef HAVE_ICONV
    if (xr->id != (iconv_t)-1)
      iconv_close(xr->id);
#endif
    free(xr);
  }
}

char *xitk_recode(xitk_recode_t *xr, const char *src) {
#ifdef HAVE_ICONV
  char    *inbuf, *outbuf, *buffer = NULL;
  size_t   inbytes, outbytes;

  if (xr && xr->id != (iconv_t)-1) {
    inbytes  = strlen(src);
    outbytes = 2 * inbytes;
    buffer   = xitk_xmalloc(outbytes + 1);
    inbuf    = (char *)src;
    outbuf   = buffer;

    while (inbytes) {
      if (iconv(xr->id, &inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
	free(buffer);
	buffer = NULL;
	break;
      }
    }
  }

  return buffer ? buffer : strdup(src);
#else
  return strdup(src);
#endif
}
