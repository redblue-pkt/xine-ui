/* 
 * Copyright (C) 2000-2010 the xine project
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"

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
  *  cs_CZ
  *  cs_CZ.UTF8
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
     dutch
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
     français
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
 *   nb_NO
 *   nb_NO.ISO-8859-1
     nl_BE
     nl_BE@euro
     nl_NL
     nl_NL@euro
     nn_NO
     no_NO
     norwegian
     nynorsk
 *   pl_PL
 *   pl_PL.UTF-8
     polish
     portuguese
  *  pt_BR
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
  *  uk_UA
     vi_VN
     zh_CN
     zh_CN.gb18030
     zh_CN.gbk
     zh_HK
     zh_TW
     zh_TW.euctw

*/

/* Global definition */
static const langs_t _langs[] = {
  /* English section */
  { "C",                ENGLISH,            "en",    ""           }, /* WARNING: This should be the first entry */
  { "POSIX",            ENGLISH,            "en",    ""           },
  { "en_GB",            ENGLISH,            "en",    "UTF-8" },

  /* French section */
  { "fr_FR",            FRENCH,             "fr",    "UTF-8" },
  { "fr_FR@euro",       FRENCH,             "fr",    "UTF-8" },
  { "french",           FRENCH,             "fr",    "UTF-8" },

  /* German section */
  { "de_DE",            GERMAN,             "de",    "UTF-8" },
  { "de_DE@euro",       GERMAN,             "de",    "UTF-8" },
  { "deutsch",          GERMAN,             "de",    "UTF-8" },
  { "german",           GERMAN,             "de",    "UTF-8" },

  /* Portuguese section */
  { "pt_BR",            PORTUGUESE,         "pt_BR", "UTF-8" },

  /* Spanish section */
  { "es_ES",            SPANISH,            "es",    "UTF-8" },
  { "es_ES@euro",       SPANISH,            "es",    "UTF-8" },
  { "spanish",          SPANISH,            "es",    "UTF-8" },

  /* Polish */
  { "pl_PL",            POLISH,             "pl",    "UTF-8" },
  { "pl_PL.UTF-8",      POLISH,             "pl",    "UTF-8" },

  /* Ukrainian */
  { "uk_UA",            UKRAINIAN,          "uk",    "UTF-8" },
  { "uk_UA.UTF-8",      UKRAINIAN,          "uk",    "UTF-8" },

  /* Czech */
  { "cs_CZ",            CZECH,              "cs",    "UTF-8" },
  { "cs_CZ.UTF-8",      CZECH,              "cs",    "UTF-8" },

  /* NORWEGIAN */
  { "nb_NO",            NORWEGIAN_BOKMAL,   "nb",    "UTF-8" },
  { "nb_NO.ISO-8859-1", NORWEGIAN_BOKMAL,   "nb",    "UTF-8" },

  /* The ultimate solution */
  { "",                 ENGLISH,            "en",    "UTF-8" }
};
  
/* ISO 639-1 */
static const struct {
  char   two_letters[4];
  char   language[28];
} iso639_1[] = {
  { "ab", "Abkhazian"                },
  { "aa", "Afar"                     },
  { "af", "Afrikaans"                },
  { "ak", "Akan"                     },
  { "sq", "Albanian"		     },
  { "am", "Amharic"		     },
  { "ar", "Arabic"		     },
  { "an", "Aragonese"		     },
  { "hy", "Armenian"		     },
  { "as", "Assamese"		     },
  { "av", "Avaric"		     },
  { "ae", "Avestan"		     },
  { "ay", "Aymara"		     },
  { "az", "Azerbaijani"		     },
  { "bm", "Bambara"		     },
  { "ba", "Bashkir"		     },
  { "eu", "Basque"		     },
  { "be", "Belarusian"		     },
  { "bn", "Bengali"		     },
  { "bh", "Bihari"		     },
  { "bi", "Bislama"		     },
  { "nb", "Bokmål, Norwegian"	     },
  { "bs", "Bosnian"		     },
  { "br", "Breton"		     },
  { "bg", "Bulgarian"		     },
  { "my", "Burmese"		     },
  { "ca", "Catalan"		     },
  { "ch", "Chamorro"		     },
  { "ce", "Chechen"		     },
  { "zh", "Chinese"		     },
  { "cv", "Chuvash"		     },
  { "kw", "Cornish"		     },
  { "co", "Corsican"		     },
  { "cr", "Cree"		     },
  { "hr", "Croatian"		     },
  { "cs", "Czech"		     },
  { "da", "Danish"		     },
  { "dv", "Divehi"		     },
  { "nl", "Dutch, Flemish"	     },
  { "dz", "Dzongkha"		     },
  { "en", "English"		     },
  { "eo", "Esperanto"		     },
  { "et", "Estonian"		     },
  { "ee", "Ewe"			     },
  { "fo", "Faroese"		     },
  { "fj", "Fijian"		     },
  { "fi", "Finnish"		     },
  { "fr", "French"		     },
  { "fy", "Frisian"		     },
  { "ff", "Fulah"		     },
  { "gd", "Gaelic"		     },
  { "gl", "Galician"		     },
  { "lg", "Ganda"		     },
  { "ka", "Georgian"		     },
  { "de", "German"		     },
  { "el", "Greek"		     },
  { "kl", "Greenlandic"		     },
  { "gn", "Guarani"		     },
  { "gu", "Gujarati"		     },
  { "ha", "Hausa"		     },
  { "he", "Hebrew"		     },
  { "hz", "Herero"		     },
  { "hi", "Hindi"		     },
  { "ho", "Hiri Motu"		     },
  { "hu", "Hungarian"		     },
  { "is", "Icelandic"		     },
  { "io", "Ido"			     },
  { "ig", "Igbo"		     },
  { "id", "Indonesian"		     },
  { "ia", "Interlingua"		     },
  { "ie", "Interlingue"		     },
  { "iu", "Inuktitut"		     },
  { "ik", "Inupiaq"		     },
  { "iw", "Hebrew"		     },
  { "ga", "Irish"		     },
  { "it", "Italian"		     },
  { "ja", "Japanese"		     },
  { "jv", "Javanese"		     },
  { "kn", "Kannada"		     },
  { "kr", "Kanuri"		     },
  { "ks", "Kashmiri"		     },
  { "kk", "Kazakh"		     },
  { "km", "Khmer"		     },
  { "ki", "Kikuyu"		     },
  { "rw", "Kinyarwanda"		     },
  { "ky", "Kirghiz"		     },
  { "kv", "Komi"		     },
  { "kg", "Kongo"		     },
  { "ko", "Korean"		     },
  { "kj", "Kuanyama"		     },
  { "ku", "Kurdish"		     },
  { "lo", "Lao"			     },
  { "la", "Latin"		     },
  { "lv", "Latvian"		     },
  { "lb", "Letzeburgesch"	     },
  { "li", "Limburgan"		     },
  { "ln", "Lingala"		     },
  { "lt", "Lithuanian"		     },
  { "lu", "Luba-Katanga"	     },
  { "mk", "Macedonian"		     },
  { "mg", "Malagasy"		     },
  { "ms", "Malay"		     },
  { "ml", "Malayalam"		     },
  { "mt", "Maltese"		     },
  { "gv", "Manx"		     },
  { "mi", "Maori"		     },
  { "mr", "Marathi"		     },
  { "mh", "Marshallese"		     },
  { "mo", "Moldavian"		     },
  { "mn", "Mongolian"		     },
  { "na", "Nauru"		     },
  { "nv", "Navaho"		     },
  { "nd", "Ndebele North"	     },
  { "nr", "Ndebele South"	     },
  { "ng", "Ndonga"		     },
  { "ne", "Nepali"		     },
  { "se", "Northern Sami"	     },
  { "no", "Norwegian"		     },
  { "nn", "Norwegian Nynorsk"	     },
  { "ny", "Nyanja"		     },
  { "oc", "Provençal"		     },
  { "oj", "Ojibwa"		     },
  { "cu", "Old Bulgarian"	     },
  { "or", "Oriya"		     },
  { "om", "Oromo"		     },
  { "os", "Ossetian"		     },
  { "pi", "Pali"		     },
  { "pa", "Panjabi"		     },
  { "fa", "Persian"		     },
  { "pl", "Polish"		     },
  { "pt", "Portuguese"		     },
  { "ps", "Pushto"		     },
  { "qu", "Quechua"		     },
  { "rm", "Raeto-Romance"	     },
  { "ro", "Romanian"		     },
  { "rn", "Rundi"		     },
  { "ru", "Russian"		     },
  { "sm", "Samoan"		     },
  { "sg", "Sango"		     },
  { "sa", "Sanskrit"		     },
  { "sc", "Sardinian"		     },
  { "sr", "Serbian"		     },
  { "sn", "Shona"		     },
  { "ii", "Sichuan Yi"		     },
  { "sd", "Sindhi"		     },
  { "si", "Sinhalese"		     },
  { "sk", "Slovak"		     },
  { "sl", "Slovenian"		     },
  { "so", "Somali"		     },
  { "st", "Sotho"		     },
  { "es", "Spanish"		     },
  { "su", "Sundanese"		     },
  { "sw", "Swahili"		     },
  { "ss", "Swati"		     },
  { "sv", "Swedish"		     },
  { "tl", "Tagalog"		     },
  { "ty", "Tahitian"		     },
  { "tg", "Tajik"		     },
  { "ta", "Tamil"		     },
  { "tt", "Tatar"		     },
  { "te", "Telugu"		     },
  { "th", "Thai"		     },
  { "bo", "Tibetan"		     },
  { "ti", "Tigrinya"		     },
  { "to", "Tonga"		     },
  { "ts", "Tsonga"		     },
  { "tn", "Tswana"		     },
  { "tr", "Turkish"		     },
  { "tk", "Turkmen"		     },
  { "tw", "Twi"			     },
  { "ug", "Uighur"		     },
  { "uk", "Ukrainian"		     },
  { "ur", "Urdu"		     },
  { "uz", "Uzbek"		     },
  { "ve", "Venda"		     },
  { "vi", "Vietnamese"		     },
  { "vo", "Volapük"		     },
  { "wa", "Walloon"		     },
  { "cy", "Welsh"		     },
  { "wo", "Wolof"		     },
  { "xh", "Xhosa"		     },
  { "yi", "Yiddish"		     },
  { "yo", "Yoruba"		     },
  { "za", "Zhuang"		     },
  { "zu", "Zulu"		     },
  { "",   ""			     }
};

/*
 * Return language from iso639-1 two letters.
 */
const char *get_language_from_iso639_1(char *two_letters) {

  if(two_letters) {
    char *tl = two_letters;
    
    while((*tl == ' ') && (*tl != '\0'))
      tl++;
    
    if(tl && (strlen(tl) == 2)) {
      int   i;
      
      for(i = 0; *(iso639_1[i].two_letters); i++) {
	if(!strcmp(iso639_1[i].two_letters, tl))
	  return iso639_1[i].language;
      }
    }
  }
  
  return two_letters ? two_letters : _("Unknown");
}

/*
 * Return a langs_t pointer on static struct langs[];
 */
const langs_t *get_lang(void) {
  char    *lcmsg = setlocale(LC_MESSAGES, NULL);
  
  if(lcmsg) {
    const langs_t *l;
    for(l = _langs; l->lang[0]; l++) {
      if(!strncasecmp(l->lang, lcmsg, strlen(lcmsg))) {
	return l;
      }
    }
  }
  
  return &_langs[0];
}
