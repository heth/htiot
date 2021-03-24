AC_INIT([htiot], [2021.3.22], [heth@mercantec.dk])
AM_INIT_AUTOMAKE([-Wall -Werror])
# we specify AM_MAINTAINER_MODE to avoid problems with rebuilding
# the configure and makefiles.  Without it users doing things that
# change the timestamps on the code, like checking it into a cvs
# tree, could trigger a rebuild of the infrastructure files which
# might fail if they don't have the correct tools.
AM_MAINTAINER_MODE
# We want to turn on warnings if we are using gcc and the user did
# not specify CFLAGS. The autoconf check for the C compiler sets the
# CFLAGS if gcc is used, so we will save it before we run that check.
SAVE_CFLAGS="$CFLAGS"
# If we have gcc, and AC_PROG_CC changed the flags, then we know the
# user did not specify any flags. Add warnings in this case.
if test "$GCC" = "yes"; then
    if test "$CFLAGS" != "$SAVE_CFLAGS"; then
        STD_CWARNINGS="$STD_CWARNINGS -Wall -Werror -fno-strict-aliasing"
    fi
fi


AC_CONFIG_FILES([Makefile src/Makefile])
AC_PROG_CC
AC_OUTPUT
