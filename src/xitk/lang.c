/* 
 * Copyright (C) 2000-2001 the xine project
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lang.h"
#include "xitk.h"

/*
 * Our LC_MESSAGES handling.
 */
/*
  locale -a tell us (* mean we already handle):
  -----------------
  *  C
  *  POSIX
     af_ZA
     ar_AE
     ar_BH
     ar_DZ
     ar_EG
     ar_IN
     ar_IQ
     ar_JO
     ar_KW
     ar_LB
     ar_LY
     ar_MA
     ar_OM
     ar_QA
     ar_SA
     ar_SD
     ar_SY
     ar_TN
     ar_YE
     be_BY
     bg_BG
     bokmal
     bokmål
     ca_ES
     ca_ES@euro
     catalan
     croatian
     cs_CZ
     czech
     da_DK
     danish
     dansk
     de_AT
     de_AT@euro
     de_BE
     de_BE@euro
     de_CH
  *  de_DE
  *  de_DE@euro
     de_LU
     de_LU@euro
  *  deutsch
  *  dutch
     eesti
     el_GR
     en_AU
     en_BW
     en_CA
     en_DK
  *  en_GB
     en_HK
     en_IE
     en_IE@euro
     en_IN
     en_NZ
     en_PH
     en_SG
     en_US
     en_ZA
     en_ZW
     es_AR
     es_BO
     es_CL
     es_CO
     es_CR
     es_DO
     es_EC
  *  es_ES
  *  es_ES@euro
     es_GT
     es_HN
     es_MX
     es_NI
     es_PA
     es_PE
     es_PR
     es_PY
     es_SV
     es_US
     es_UY
     es_VE
     estonian
     et_EE
     eu_ES
     eu_ES@euro
     fa_IR
     fi_FI
     fi_FI@euro
     finnish
     fo_FO
     fr_BE
     fr_BE@euro
     fr_CA
     fr_CH
  *  fr_FR
  *  fr_FR@euro
     fr_LU
     fr_LU@euro
  *  français
  *  french
     ga_IE
     ga_IE@euro
     galego
     galician
  *  german
     gl_ES
     gl_ES@euro
     greek
     gv_GB
     he_IL
     hebrew
     hi_IN
     hr_HR
     hrvatski
     hu_HU
     hungarian
     icelandic
     id_ID
     is_IS
     it_CH
     it_IT
     it_IT@euro
     italian
     iw_IL
     ja_JP
     ja_JP.eucjp
     ja_JP.ujis
     japanese
     japanese.euc
     japanese.sjis
     kl_GL
     ko_KR
     ko_KR.euckr
     ko_KR.utf8
     korean
     korean.euc
     kw_GB
     lithuanian
     lt_LT
     lv_LV
     mk_MK
     mr_IN
     ms_MY
     mt_MT
     nb_NO
     nb_NO.ISO-8859-1
     nl_BE
     nl_BE@euro
     nl_NL
     nl_NL@euro
     nn_NO
     no_NO
     norwegian
     nynorsk
     pl_PL
     polish
     portuguese
     pt_BR
     pt_PT
     pt_PT@euro
     ro_RO
     romanian
     ru_RU
     ru_RU.koi8r
     ru_UA
     russian
     sk_SK
     sl_SI
     slovak
     slovene
     slovenian
  *  spanish
     sq_AL
     sr_YU
     sr_YU@cyrillic
     sv_FI
     sv_FI@euro
     sv_SE
     swedish
     ta_IN
     te_IN
     th_TH
     thai
     tr_TR
     turkish
     uk_UA
     vi_VN
     zh_CN
     zh_CN.gb18030
     zh_CN.gbk
     zh_HK
     zh_TW
     zh_TW.euctw

*/

/* Global definition */
static langs_t _langs[] = {
  /* English section */
  { "C",               ENGLISH, "_en" }, /* WARNING: This should be the first entry */
  { "POSIX",           ENGLISH, "_en" },
  { "en_GB",           ENGLISH, "_en" },

  /* French section */
  { "fr_FR",           FRENCH,  "_fr" },
  { "fr_FR@euro",      FRENCH,  "_fr" },
  { "français",        FRENCH,  "_fr" },
  { "french",          FRENCH,  "_fr" },

  /* German section */
  { "de_DE",           GERMAN,  "_de" },
  { "de_DE@euro",      GERMAN,  "_de" },
  { "deutsch",         GERMAN,  "_de" },
  { "dutch",           GERMAN,  "_de" },
  { "german",          GERMAN,  "_de" },

  /* Spanish section */
  { "es_ES",           SPANISH, "_es" },
  { "es_ES@euro",      SPANISH, "_es" },
  { "spanish",         SPANISH, "_es" },

  /* The ultimate solution */
  { NULL,              ENGLISH, "_en" }
};
  

/*
 * Return a langs_t pointer on static struct langs[];
 */
const langs_t *get_lang(void) {
  langs_t *l;
  char    *lcmsg = setlocale(LC_MESSAGES, NULL);
  
  if(lcmsg) {
    for(l = _langs; l->lang != NULL; l++) {
      if(!strncasecmp(l->lang, lcmsg, strlen(lcmsg))) {
	return l;
      }
    }
  }
  
  return &_langs[0];
}

const langs_t *get_default_lang(void) {
  return &_langs[0];
}
