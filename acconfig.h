#ifndef _CONFIG_H_
#define _CONFIG_H_
@TOP@

/* Define this if you're running x86 architecture */
#undef __i386__

/* Define this if you're running x86 architecture */
#undef ARCH_X86

/* Define this if you're running Alpha architecture */
#undef __alpha__

/* Define this if you're running PowerPC architecture */
#undef __ppc__

/* Define this if you're running Sparc architecture */
#undef __sparc__ 

/* Define this if you have X11R6 installed */
#undef HAVE_X11

#undef HAVE_IPC_H
#undef HAVE_SHM_H
#undef HAVE_XSHM_H
#undef HAVE_SHM
#undef IPC_RMID_DEFERRED_RELEASE

/* Define this if you have libXtst installed */
#undef HAVE_XTESTEXTENSION

/* Define this if you have GNU getopt_long() implemented */
#undef HAVE_GETOPT_LONG

/* Define this if you have getpwuid_t() implemented */
#undef HAVE_GETPWUID_R

/* Define this is you have stdarg.h header file installed */
#undef HAVE_STDARGS

/* Define this if you have LIRC (liblir_client) installed */
#undef HAVE_LIRC

/* Define this if you have ORBit (libORBit) installed */
#undef HAVE_ORBIT

#undef HAVE_AA

/* Define this if you have libXv installed */
#undef HAVE_XV

/* Define this if you have libXinerama installed */
#undef HAVE_XINERAMA

/* Define this if you have libXxf86vm installed */
#undef HAVE_XF86VIDMODE

/* Define this to add mrl browser into xitk lib */
#undef NEED_MRLBROWSER

/* Where skins are */
#undef XINE_SKINDIR

/* Where scripts are */
#undef XINE_SCRIPTDIR

/* Path where catalog files will be. */
#undef XITK_LOCALE

/* Path where docs files will be. */
#undef XINE_DOCDIR

/* Readline section */
/* Definitions pulled in from aclocal.m4. */
#undef VOID_SIGHANDLER

#undef TIOCGWINSZ_IN_SYS_IOCTL

#undef GWINSZ_IN_SYS_IOCTL

#undef TIOCSTAT_IN_SYS_IOCTL

#undef HAVE_GETPW_DECLS

#undef FIONREAD_IN_SYS_IOCTL

#undef SPEED_T_IN_SYS_TYPES

#undef HAVE_BSD_SIGNALS

#undef HAVE_POSIX_SIGNALS

#undef HAVE_USG_SIGHOLD

#undef MUST_REINSTALL_SIGHANDLERS

#undef HAVE_POSIX_SIGSETJMP

#undef STRCOLL_BROKEN

#undef STRUCT_DIRENT_HAS_D_INO

#undef STRUCT_DIRENT_HAS_D_FILENO

#undef STRUCT_WINSIZE_IN_SYS_IOCTL

#undef STRUCT_WINSIZE_IN_TERMIOS

/* Define if you have the lstat function. */
#undef HAVE_LSTAT
/* End of readline section */

@BOTTOM@
#endif
