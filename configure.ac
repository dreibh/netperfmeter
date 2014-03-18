AC_INIT([NetPerfMeter], [1.4.0~rc2.1], [dreib@iem.uni-due.de])

AM_INIT_AUTOMAKE
AC_PREREQ(2.60)

AC_CONFIG_HEADERS([config.h:config.in])
AC_CONFIG_SRCDIR([src/netperfmeter.cc])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# ###### colorgcc ###########################################################
AC_ARG_ENABLE([colorgcc],
              [  --enable-colorgcc       enable colorgcc support ][default=yes]],
              enable_colorgcc=$enableval, enable_colorgcc=yes)
if test "${enable_colorgcc}" = "yes"; then
   AC_CHECK_PROG([COLORGCC], [colorgcc], [yes], [no])
   if test "$COLORGCC" == "yes"
   then
      AC_MSG_NOTICE([Compiling using colorgcc])
      CC="colorgcc"
      CXX="colorgcc -lstdc++"
   fi
fi

# ###### Maintainer mode ####################################################
AC_ARG_ENABLE([maintainer-mode],
              AC_HELP_STRING( [--enable-maintainer-mode],
                              [enable maintainer mode @<:@default=no@:>@]),
              enable_maintainer_mode=$enableval,enable_maintainer_mode=no)
if test x$enable_maintainer_mode = xyes; then
   CFLAGS="$CFLAGS -Wall -Wunreachable-code -g3 -O0"
   CXXFLAGS="$CXXFLAGS -Wall -Wunreachable-code -g3 -O0"
else
   CFLAGS="$CFLAGS -Wall -Wunreachable-code"
   CXXFLAGS="$CXXFLAGS -Wall -Wunreachable-code"
fi

AC_ARG_ENABLE([test-programs],
              AC_HELP_STRING( [--enable-test-programs],
                              [build with batch test programs @<:@default=no@:>@]),
              enable_test_programs=$enableval,enable_test_programs=no)
AM_CONDITIONAL(BUILD_TEST_PROGRAMS, test x$enable_test_programs = xyes)

AC_ARG_ENABLE([plotting-programs],
              AC_HELP_STRING( [--enable-plotting-programs],
                              [build with batch plotting programs @<:@default=no@:>@]),
              enable_plotting_programs=$enableval,enable_plotting_programs=no)
AM_CONDITIONAL(BUILD_PLOTTING_PROGRAMS, test x$enable_plotting_programs = xyes)

AC_ARG_ENABLE([per-cpu-stats],
              AC_HELP_STRING( [--enable-per-cpu-stats],
                              [Use per CPU stats (applies only to Mac OS X) @<:@default=no@:>@]),
              enable_per_cpu_stats=$enableval,enable_per_cpu_stats=no)
if test x$enable_per_cpu_stats = xyes; then
  AC_DEFINE(USE_PER_CPU_STATISTICS, 1, [Use per CPU statistics])
fi

# ###### Networking declarations ############################################
AC_MSG_CHECKING(if stderr is a variable of type FILE* and not a macro)
AC_TRY_COMPILE([#include <stdio.h>],
               [FILE** stdlog = &stderr],
               [AC_DEFINE(HAVE_STDERR_FILEPTR, 1, "Define to 1 if stderr is a pointer.")
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])

AC_CHECK_HEADERS(sys/types.h sys/socket.h netinet/in.h netinet/sctp.h)

AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <sys/socket.h>],
               [socklen_t x = 1; return 0;],
               [AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(int)
                AC_DEFINE(socklen_t, int, [Define a type for socklen_t.])])

AC_MSG_CHECKING([whether 'struct sockaddr' has sa_len])
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <sys/socket.h>],
               [static struct sockaddr sa; int i = sizeof(sa.sa_len);],
               [AC_MSG_RESULT(yes)
                AC_DEFINE(HAVE_SA_LEN,1,"Define this if you have sa_len in sockaddr struct.")],
                AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether 'struct sockaddr_in' has sin_len])
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <netinet/in.h>],
               [static struct sockaddr_in in; int i = sizeof(in.sin_len);],
               [AC_MSG_RESULT(yes)
                AC_DEFINE(HAVE_SIN_LEN,1,"Define this if your IPv4 has sin_len in sockaddr_in struct.")],
                AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether 'struct sockaddr_in6' has sin6_len])
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <netinet/in.h>],
               [static struct sockaddr_in6 in; int i = sizeof(in.sin6_len);],
               [AC_MSG_RESULT(yes)
                AC_DEFINE(HAVE_SIN6_LEN,1,"Define this if your IPv6 has sin6_len in sockaddr_in6 struct.")],
                AC_MSG_RESULT(no))

AC_MSG_CHECKING(for DCCP support)
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <netinet/in.h>],
               [socklen_t x = IPPROTO_DCCP; return 0;],
               [AC_MSG_RESULT(yes)
                AC_DEFINE(HAVE_DCCP, 1, "Define to 1 if DCCP is supported.")],
               [AC_MSG_RESULT(no)])

# !!! Since only Linux supports MPTCP at the moment, the following code
# !!! is Linux-specific!
AC_MSG_CHECKING(for MPTCP support)
AC_TRY_COMPILE([#include <linux/tcp.h>],
               [int x = 1; return 0;],
               [AC_MSG_RESULT(yes)
                AC_DEFINE(HAVE_MPTCP, 1, "Define to 1 if MPTCP is supported.")],
               [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for IPV6_BINDV6ONLY)
AC_TRY_COMPILE([#include <netinet/in.h>],
               [int x = IPV6_BINDV6ONLY],
               [AC_DEFINE(HAVE_IPV6_BINDV6ONLY, 1, "Define to 1 if IPV6_BINDV6ONLY socket option is available.")
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])

AC_CHECK_FUNCS(socket, , AC_CHECK_LIB(socket, socket))


# ###### Check for IEEE-754 complient float-point handling ##################
AC_MSG_CHECKING([for floating point type])
AC_TRY_RUN([
#include <stdio.h>

union DoubleIntUnion
{
   double             Double;
   unsigned long long Integer;
};


/*
void hexdump(const void* ptr, const size_t length)
{
   const unsigned char* cptr = (const unsigned char*)ptr;
   size_t               i;

   for(i = 0;i < length;i++) {
      printf("%02x ", cptr[i]);
   }
   puts("");
}
*/

int main ()
{
   union DoubleIntUnion value;

   value.Double = 0.0;

   if(value.Integer != 0ULL) {
      /* puts("Math is non-IEEE (check 1)!\n"); */
      return(1);
   }

   value.Double = 1000.5;
   if(value.Integer != 0x408f440000000000ULL) {
      /* puts("Math is non-IEEE (check 2)!\n"); */
      return(1);
   }

   value.Double = -1000.5;
   if(value.Integer != 0xc08f440000000000ULL) {
      /* puts("Math is non-IEEE (check 3)!\n"); */
      return(1);
   }

   /* puts("Math is IEEE\n"); */
   return(0);
}
], IEEEFP=ok, IEEEFP=broken, IEEEFP=cross)


case $IEEEFP in
ok)
    AC_MSG_RESULT(IEEE)
    AC_DEFINE(HAVE_IEEE_FP, 1, [IEEE compliant float-point handling])
    ;;
cross)
    AC_MSG_RESULT([cross-compiling, assuming IEEE])
    AC_DEFINE(HAVE_IEEE_FP)
    ;;
*|broken)
    AC_MSG_RESULT([non IEEE])
    ;;
esac


# ###### Check for BZip2 library ############################################
oldLIBS=$LIBS
LIBS=
AC_CHECK_HEADERS([bzlib.h])
AC_SEARCH_LIBS([BZ2_bzReadOpen], [bz2])
bz2_LIBS=$LIBS
AC_SUBST(bz2_LIBS)
LIBS=$oldLIBS


# ###### Check if kernel SCTP should be used ################################
AC_ARG_ENABLE([kernel-sctp],
              AC_HELP_STRING( [--enable-kernel-sctp],
                              [Use kernel SCTP instead of sctplib @<:@default=yes@:>@]),
              enable_kernel_sctp=$enableval,enable_kernel_sctp=yes)


# ###### Check for SCTP implementation ######################################
if test "$enable_kernel_sctp" = "no"; then
   # ====== sctplib =========================================================
   ac_sctplib_includes=no ac_sctplib_libraries=no ac_sctplib_bindir=no
   sctplib_libraries=""
   sctplib_includes=""
   AC_ARG_WITH(sctplib,
      [  --with-sctplib=DIR       where the root of sctplib is installed ],
      [  ac_sctplib_includes="$withval"/include
         ac_sctplib_libraries="$withval"/lib
         ac_sctplib_bindir="$withval"/bin
      ])

   if test "$ac_sctplib_libraries" = "no"; then
      ac_sctplib_defaultdir="/usr/local"
      ac_sctplib_includes="$ac_sctplib_defaultdir"/include
      ac_sctplib_libraries="$ac_sctplib_defaultdir"/lib
      ac_sctplib_bindir="$ac_sctplib_defaultdir"/bin
   fi

   if test ! -e "$ac_sctplib_includes/sctp.h" ; then
      AC_MSG_ERROR([No sctplib installation found ($ac_sctplib_includes/sctp.h)!])
   fi
   if test ! -e $ac_sctplib_libraries/libsctplib.a ; then
      AC_MSG_ERROR([No sctplib installation found ($ac_sctplib_libraries/libsctplib.a)!])
   fi

   # ====== socketapi =======================================================
   ac_socketapi_includes=no ac_socketapi_libraries=no ac_socketapi_bindir=no
   socketapi_libraries=""
   socketapi_includes=""
   AC_ARG_WITH(socketapi,
      [  --with-socketapi=DIR       where the root of socketapi is installed ],
      [  ac_socketapi_includes="$withval"/include
         ac_socketapi_libraries="$withval"/lib
         ac_socketapi_bindir="$withval"/bin
      ])

   if test "$ac_socketapi_libraries" = "no"; then
      ac_socketapi_defaultdir="/usr/local"
      ac_socketapi_includes="$ac_socketapi_defaultdir"/include
      ac_socketapi_libraries="$ac_socketapi_defaultdir"/lib
      ac_socketapi_bindir="$ac_socketapi_defaultdir"/bin
   fi

   if test ! -e $ac_socketapi_libraries/libsctpsocket.a ; then
      AC_MSG_ERROR([No socketapi installation found ($ac_socketapi_libraries/libsctpsocket.a)!])
   fi

   # In maintainer mode, use static linking for sctplib/socketapi!
   if test "$enable_maintainer_mode" = "yes"; then
      sctplib_LIBS="$ac_sctplib_libraries/libsctplib.a"
      socketapi_LIBS="$ac_socketapi_libraries/libsctpsocket.a -lstdc++"
   else
      sctplib_LIBS="-L$ac_sctplib_libraries -lsctplib"
      socketapi_LIBS="-L$ac_socketapi_libraries -lsctpsocket -lstdc++"
   fi

   AC_DEFINE(HAVE_SPP_FLAGS, 1, [Define this if you support version 10 if the API ID.])

   # ====== Check for glib ==================================================
   AM_PATH_GLIB_2_0(2.0.0, [ glib_is_okay=yes ], [ glib_is_okay=no ])
   if test "x$glib_is_okay" = "xno"; then
      AC_MSG_ERROR(GLib distribution not found.)
   fi

   CFLAGS="$CFLAGS $GLIB_CFLAGS"
   CPPFLAGS="$CPPFLAGS $GLIB_CFLAGS"
   glib_LIBS="$GLIB_LIBS"

   # ====== Version compatibility test ======================================
   oldLIBS=$LIBS
   LIBS="$LIBS $socketapi_LIBS $sctplib_LIBS $glib_LIBS $thread_LIBS"
   AC_MSG_CHECKING([socketapi library version])
   AC_TRY_RUN([
#include <stdio.h>
#include "ext_socket.h"

int main(int argc, char** argv)
{
   unsigned int version = socketAPIGetVersion();
   printf("Version is %u.%04x\n", (version >> 16), version & 0xffff);
   if(version >= 0x21100) {
      return(0);
   }
   return(1);
}
], socketapi_version=good, socketapi_version=bad, socketapi_version=cross)

   case $socketapi_version in
   good)
      ;;
   bad|*)
      AC_MSG_ERROR([You need at least version 2.110 of the socketapi library!])
      ;;
   esac

   AC_DEFINE(HAVE_SCTP_SENDMSG, 1, [Define to 1 if you have the `sctp_sendmsg' function.])
   AC_DEFINE(HAVE_SCTP_CONNECTX, 1, [Define to 1 if you have the `sctp_connectx' function.])
   AC_DEFINE(HAVE_SCTP_SEND, 1, [Define to 1 if you have the `sctp_send' function.])
   AC_DEFINE(HAVE_SCTP_SENDX, 1, [Define to 1 if you have the `sctp_sendx' function.])
   AC_DEFINE(HAVE_SPP_FLAGS, 1, [Define this if you support version 10 if the API ID.])
   AC_DEFINE(HAVE_CONNECTX_WITH_ID, 1, [Define to 1 if sctp_connectx has associd parameter.])
   AC_DEFINE(HAVE_SCTP_DELAYED_SACK, 1, [Define to 1 if SCTP_DELAYED_SACK with sctp_sack_info is available.])

   LIBS=$oldLIBS
   # ========================================================================

else
   # ====== Kernel SCTP =====================================================
   oldLIBS=$LIBS
   AC_CHECK_FUNCS(sctp_sendmsg, ,  AC_CHECK_LIB(sctp, sctp_sendmsg))
   AC_CHECK_FUNCS(sctp_connectx, , AC_CHECK_LIB(sctp, sctp_connectx))
   AC_CHECK_FUNCS(sctp_send, ,     AC_CHECK_LIB(sctp, sctp_send))
   AC_CHECK_FUNCS(sctp_sendx, ,    AC_CHECK_LIB(sctp, sctp_sendx))
   AC_CHECK_MEMBER(struct sctp_paddrparams.spp_flags,
                   AC_DEFINE(HAVE_SPP_FLAGS, 1, [Define this if sctp_paddrparams has the spp_flags field.]),,
                   [#ifdef HAVE_SYS_TYPES_H
                   #include <sys/types.h>
                   #endif
                   #ifdef HAVE_NETINET_SCTP_H
                   #include <netinet/sctp.h>
                   #endif])
   AC_MSG_CHECKING(whether sctp_connectx has an associd parameter)
   AC_TRY_COMPILE([#include <sys/types.h>
                   #include <netinet/sctp.h>],
                  [sctp_assoc_t id;
                   int result = sctp_connectx(0, NULL, 0, &id);],
                  [AC_MSG_RESULT(yes)
                   AC_DEFINE(HAVE_CONNECTX_WITH_ID, 1, [Define to 1 if sctp_connectx has the associd parameter.])],
                  AC_MSG_RESULT(no))
   AC_MSG_CHECKING(whether SCTP_DELAYED_SACK with sctp_sack_info is available)
   AC_TRY_COMPILE([#include <sys/types.h>
                   #include <netinet/sctp.h>],
                  [struct sctp_sack_info sctpSACKInfo;
                   int opt = SCTP_DELAYED_SACK;],
                  [AC_MSG_RESULT(yes)
                   AC_DEFINE(HAVE_SCTP_DELAYED_SACK, 1, [Define to 1 if SCTP_DELAYED_SACK with sctp_sack_info is available.])],
                  AC_MSG_RESULT(no))
   LIBS=$oldLIBS

   # ====== Check for Valgrind ==============================================
   # Needed to avoid false positives for sctp_getladdrs()/sctp_getpaddrs()
   AC_CHECK_HEADERS([valgrind/valgrind.h])
   AC_CHECK_HEADERS([valgrind/memcheck.h])

   # ====== Check for SCTP library linker parameter =========================
   sctplib_LIBS=""
   socketapi_LIBS=""
   sctp_okay=0
   AC_CHECK_LIB(sctp, sctp_sendmsg, [
      socketapi_LIBS="-lsctp"
      sctp_okay=1])
   if test $sctp_okay -ne 1 ; then
      AC_CHECK_LIB(c, sctp_sendmsg, [
         socketapi_LIBS=""
         sctp_okay=1])
   fi

   AC_MSG_CHECKING([whether library parameter for SCTP functions])
   if test $sctp_okay -ne 1 ; then
      AC_MSG_ERROR([Do not know where to find SCTP functions! Missing libsctp?])
   else
      AC_MSG_RESULT([appending "$socketapi_LIBS"])
   fi

   glib_LIBS=""
   CFLAGS="$CFLAGS -DHAVE_KERNEL_SCTP"
   CXXFLAGS="$CXXFLAGS -DHAVE_KERNEL_SCTP"
fi
AC_SUBST(sctplib_LIBS)
AC_SUBST(socketapi_LIBS)
AC_SUBST(glib_LIBS)


# ###### Makefiles ##########################################################
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT


# ###### Configuration results ##############################################
echo ""
echo "The NetPerfMeter package has been configured with the following options:"
echo ""
echo "   Build in Maintainer Mode        : $enable_maintainer_mode"
echo "   Build with Kernel SCTP          : $enable_kernel_sctp"
echo "   SCTP Socket API Libraries       : $socketapi_LIBS"
if test "$enable_kernel_sctp" = "no"; then
   echo "   SCTPLIB Libraries               : $sctplib_LIBS"
fi
echo "   BZip2 Libraries                 : $bz2_LIBS"
