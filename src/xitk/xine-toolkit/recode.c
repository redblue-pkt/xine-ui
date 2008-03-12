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

#include <assert.h>

#include "_xitk.h"
#include "recode.h"

typedef struct {
  const char               language[9];
  const char               encoding[9];
} lang_locale_t;

static const lang_locale_t lang_locales[] = {
  { "af_ZA",    "iso88591"   },
  { "ar_AE",    "iso88596"   },
  { "ar_BH",    "iso88596"   },
  { "ar_DZ",    "iso88596"   },
  { "ar_EG",    "iso88596"   },
  { "ar_IN",    "utf8"       },
  { "ar_IQ",    "iso88596"   },
  { "ar_JO",    "iso88596"   },
  { "ar_KW",    "iso88596"   },
  { "ar_LB",    "iso88596"   },
  { "ar_LY",    "iso88596"   },
  { "ar_MA",    "iso88596"   },
  { "ar_OM",    "iso88596"   },
  { "ar_QA",    "iso88596"   },
  { "ar_SA",    "iso88596"   },
  { "ar_SD",    "iso88596"   },
  { "ar_SY",    "iso88596"   },
  { "ar_TN",    "iso88596"   },
  { "ar_YE",    "iso88596"   },
  { "be_BY",    "cp1251"     },
  { "bg_BG",    "cp1251"     },
  { "br_FR",    "iso88591"   },
  { "bs_BA",    "iso88592"   },
  { "ca_ES",    "iso88591"   },
  { "ca_ES",    "iso885915"  },
  { "cs_CZ",    "iso88592"   },
  { "cy_GB",    "iso885914"  },
  { "da_DK",    "iso88591"   },
  { "de_AT",    "iso88591"   },
  { "de_AT",    "iso885915"  },
  { "de_BE",    "iso88591"   },
  { "de_BE",    "iso885915"  },
  { "de_CH",    "iso88591"   },
  { "de_DE",    "iso88591"   },
  { "de_DE",    "iso885915"  },
  { "de_LU",    "iso88591"   },
  { "de_LU",    "iso885915"  },
  { "el_GR",    "iso88597"   },
  { "en_AU",    "iso88591"   },
  { "en_BW",    "iso88591"   },
  { "en_CA",    "iso88591"   },
  { "en_DK",    "iso88591"   },
  { "en_GB",    "iso88591"   },
  { "en_HK",    "iso88591"   },
  { "en_IE",    "iso88591"   },
  { "en_IE",    "iso885915"  },
  { "en_IN",    "utf8"       },
  { "en_NZ",    "iso88591"   },
  { "en_PH",    "iso88591"   },
  { "en_SG",    "iso88591"   },
  { "en_US",    "iso88591"   },
  { "en_ZA",    "iso88591"   },
  { "en_ZW",    "iso88591"   },
  { "es_AR",    "iso88591"   },
  { "es_BO",    "iso88591"   },
  { "es_CL",    "iso88591"   },
  { "es_CO",    "iso88591"   },
  { "es_CR",    "iso88591"   },
  { "es_DO",    "iso88591"   },
  { "es_EC",    "iso88591"   },
  { "es_ES",    "iso88591"   },
  { "es_ES",    "iso885915"  },
  { "es_GT",    "iso88591"   },
  { "es_HN",    "iso88591"   },
  { "es_MX",    "iso88591"   },
  { "es_NI",    "iso88591"   },
  { "es_PA",    "iso88591"   },
  { "es_PE",    "iso88591"   },
  { "es_PR",    "iso88591"   },
  { "es_PY",    "iso88591"   },
  { "es_SV",    "iso88591"   },
  { "es_US",    "iso88591"   },
  { "es_UY",    "iso88591"   },
  { "es_VE",    "iso88591"   },
  { "et_EE",    "iso88591"   },
  { "eu_ES",    "iso88591"   },
  { "eu_ES",    "iso885915"  },
  { "fa_IR",    "utf8"       },
  { "fi_FI",    "iso88591"   },
  { "fi_FI",    "iso885915"  },
  { "fo_FO",    "iso88591"   },
  { "fr_BE",    "iso88591"   },
  { "fr_BE",    "iso885915"  },
  { "fr_CA",    "iso88591"   },
  { "fr_CH",    "iso88591"   },
  { "fr_FR",    "iso88591"   },
  { "fr_FR",    "iso885915"  },
  { "fr_LU",    "iso88591"   },
  { "fr_LU",    "iso885915"  },
  { "ga_IE",    "iso88591"   },
  { "ga_IE",    "iso885915"  },
  { "gl_ES",    "iso88591"   },
  { "gl_ES",    "iso885915"  },
  { "gv_GB",    "iso88591"   },
  { "he_IL",    "iso88598"   },
  { "hi_IN",    "utf8"       },
  { "hr_HR",    "iso88592"   },
  { "hu_HU",    "iso88592"   },
  { "id_ID",    "iso88591"   },
  { "is_IS",    "iso88591"   },
  { "it_CH",    "iso88591"   },
  { "it_IT",    "iso88591"   },
  { "it_IT",    "iso885915"  },
  { "iw_IL",    "iso88598"   },
  { "ja_JP",    "utf8"       },
  { "ja_JP",    "eucjp"      },
  { "ja_JP",    "ujis"       },
  { "japanese", "euc"        },
  { "ka_GE",    "georgianps" },
  { "kl_GL",    "iso88591"   },
  { "ko_KR",    "euckr"      },
  { "ko_KR",    "utf8"       },
  { "korean",   "euc"        },
  { "kw_GB",    "iso88591"   },
  { "lt_LT",    "iso885913"  },
  { "lv_LV",    "iso885913"  },
  { "mi_NZ",    "iso885913"  },
  { "mk_MK",    "iso88595"   },
  { "mr_IN",    "utf8"       },
  { "ms_MY",    "iso88591"   },
  { "mt_MT",    "iso88593"   },
  { "nb_NO",    "ISO-8859-1" },
  { "nl_BE",    "iso88591"   },
  { "nl_BE",    "iso885915"  },
  { "nl_NL",    "iso88591"   },
  { "nl_NL",    "iso885915"  },
  { "nn_NO",    "iso88591"   },
  { "no_NO",    "iso88591"   },
  { "oc_FR",    "iso88591"   },
  { "pl_PL",    "iso88592"   },
  { "pt_BR",    "iso88591"   },
  { "pt_PT",    "iso88591"   },
  { "pt_PT",    "iso885915"  },
  { "ro_RO",    "iso88592"   },
  { "ru_RU",    "iso88595"   },
  { "ru_RU",    "koi8r"      },
  { "ru_UA",    "koi8u"      },
  { "se_NO",    "utf8"       },
  { "sk_SK",    "iso88592"   },
  { "sl_SI",    "iso88592"   },
  { "sq_AL",    "iso88591"   },
  { "sr_YU",    "iso88592"   },
  { "sr_YU",    "iso88595"   },
  { "sv_FI",    "iso88591"   },
  { "sv_FI",    "iso885915"  },
  { "sv_SE",    "iso88591"   },
  { "ta_IN",    "utf8"       },
  { "te_IN",    "utf8"       },
  { "tg_TJ",    "koi8t"      },
  { "th_TH",    "tis620"     },
  { "tl_PH",    "iso88591"   },
  { "tr_TR",    "iso88599"   },
  { "uk_UA",    "koi8u"      },
  { "ur_PK",    "utf8"       },
  { "uz_UZ",    "iso88591"   },
  { "vi_VN",    "tcvn"       },
  { "vi_VN",    "utf8"       },
  { "wa_BE",    "iso88591"   },
  { "wa_BE",    "iso885915"  },
  { "yi_US",    "cp1255"     },
  { "zh_CN",    "gb18030"    },
  { "zh_CN",    "gb2312"     },
  { "zh_CN",    "gbk"        },
  { "zh_HK",    "big5hkscs"  },
  { "zh_TW",    "big5"       },
  { "zh_TW",    "euctw"      }
};

static const lang_locale_t *_get_first_lang_locale(char *lcal) {
  if ( ! (lcal && lcal[0]) )
    return NULL;

  const size_t lcal_len = strlen(lcal);
  const lang_locale_t *locale = lang_locales;
  size_t i;

  for(i = 0; i < sizeof(lang_locales)/sizeof(lang_locales[0]); i++, locale++) {
    if ( ! strncmp(lcal, locale->language, lcal_len) )
      return locale;
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

        if(llocale != NULL)
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
  return xr;
}

void xitk_recode_done(xitk_recode_t *xr) {
  if ( ! xr ) return;

  iconv_close(xr->id);

  free(xr);
}

char *xitk_recode(xitk_recode_t *xr, const char *src) {
  char *buffer = NULL;

  if ( xr ) {
    size_t inbytes  = strlen(src);
    size_t outbytes = 2 * inbytes;
    char *buffer    = calloc(outbytes + 1, sizeof(char));
    ICONV_CONST char *inbuf     = (ICONV_CONST char *)src;
    char *outbuf    = buffer;

    while (inbytes) {
      if (iconv(xr->id, &inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
	free(buffer);
	buffer = NULL;
	break;
      }
    }
  }

  return buffer ? buffer : strdup(src);
}
