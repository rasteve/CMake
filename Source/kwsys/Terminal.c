/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing#kwsys for details.  */
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__OpenBSD__)
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#  define _XOPEN_SOURCE 600
#endif
#include "kwsysPrivate.h"
#include KWSYS_HEADER(Terminal.h)

/* Work-around CMake dependency scanning limitation.  This must
   duplicate the above list of headers.  */
#if 0
#  include "Terminal.h.in"
#endif

/* Configure support for this platform.  */
#if defined(_WIN32)
#  define KWSYS_TERMINAL_SUPPORT_CONSOLE
#endif
#if !defined(_WIN32)
#  define KWSYS_TERMINAL_ISATTY_WORKS
#endif

/* Include needed system APIs.  */

#include <stdarg.h> /* va_list */
#include <stdlib.h> /* getenv */
#include <string.h> /* strcmp */

#if defined(KWSYS_TERMINAL_SUPPORT_CONSOLE)
#  include <io.h>      /* _get_osfhandle */
#  include <windows.h> /* SetConsoleTextAttribute */
#endif

#if defined(KWSYS_TERMINAL_ISATTY_WORKS)
#  include <unistd.h> /* isatty */
#else
#  include <sys/stat.h> /* fstat */
#endif

static int kwsysTerminalStreamIsVT100(FILE* stream, int default_vt100,
                                      int default_tty);
static void kwsysTerminalSetVT100Color(FILE* stream, int color);
#if defined(KWSYS_TERMINAL_SUPPORT_CONSOLE)
static HANDLE kwsysTerminalGetStreamHandle(FILE* stream);
static void kwsysTerminalSetConsoleColor(HANDLE hOut,
                                         CONSOLE_SCREEN_BUFFER_INFO* hOutInfo,
                                         FILE* stream, int color);
#endif

void kwsysTerminal_cfprintf(int color, FILE* stream, char const* format, ...)
{
  /* Setup the stream with the given color if possible.  */
  int pipeIsConsole = 0;
  int pipeIsVT100 = 0;
  int default_vt100 = color & kwsysTerminal_Color_AssumeVT100;
  int default_tty = color & kwsysTerminal_Color_AssumeTTY;
#if defined(KWSYS_TERMINAL_SUPPORT_CONSOLE)
  CONSOLE_SCREEN_BUFFER_INFO hOutInfo;
  HANDLE hOut = kwsysTerminalGetStreamHandle(stream);
  if (GetConsoleScreenBufferInfo(hOut, &hOutInfo)) {
    pipeIsConsole = 1;
    kwsysTerminalSetConsoleColor(hOut, &hOutInfo, stream, color);
  }
#endif
  if (!pipeIsConsole &&
      kwsysTerminalStreamIsVT100(stream, default_vt100, default_tty)) {
    pipeIsVT100 = 1;
    kwsysTerminalSetVT100Color(stream, color);
  }

  /* Format the text into the stream.  */
  {
    va_list var_args;
    va_start(var_args, format);
    vfprintf(stream, format, var_args);
    va_end(var_args);
  }

/* Restore the normal color state for the stream.  */
#if defined(KWSYS_TERMINAL_SUPPORT_CONSOLE)
  if (pipeIsConsole) {
    kwsysTerminalSetConsoleColor(hOut, &hOutInfo, stream,
                                 kwsysTerminal_Color_Normal);
  }
#endif
  if (pipeIsVT100) {
    kwsysTerminalSetVT100Color(stream, kwsysTerminal_Color_Normal);
  }
}

/* Detect cases when a stream is definitely not interactive.  */
#if !defined(KWSYS_TERMINAL_ISATTY_WORKS)
static int kwsysTerminalStreamIsNotInteractive(FILE* stream)
{
  /* The given stream is definitely not interactive if it is a regular
     file.  */
  struct stat stream_stat;
  if (fstat(fileno(stream), &stream_stat) == 0) {
    if (stream_stat.st_mode & S_IFREG) {
      return 1;
    }
  }
  return 0;
}
#endif

/* List of terminal names known to support VT100 color escape sequences.  */
static char const* kwsysTerminalVT100Names[] = { "Eterm",
                                                 "alacritty",
                                                 "alacritty-direct",
                                                 "ansi",
                                                 "color-xterm",
                                                 "con132x25",
                                                 "con132x30",
                                                 "con132x43",
                                                 "con132x60",
                                                 "con80x25",
                                                 "con80x28",
                                                 "con80x30",
                                                 "con80x43",
                                                 "con80x50",
                                                 "con80x60",
                                                 "cons25",
                                                 "console",
                                                 "cygwin",
                                                 "dtterm",
                                                 "eterm-color",
                                                 "gnome",
                                                 "gnome-256color",
                                                 "konsole",
                                                 "konsole-256color",
                                                 "kterm",
                                                 "linux",
                                                 "msys",
                                                 "linux-c",
                                                 "mach-color",
                                                 "mlterm",
                                                 "putty",
                                                 "putty-256color",
                                                 "rxvt",
                                                 "rxvt-256color",
                                                 "rxvt-cygwin",
                                                 "rxvt-cygwin-native",
                                                 "rxvt-unicode",
                                                 "rxvt-unicode-256color",
                                                 "screen",
                                                 "screen-256color",
                                                 "screen-256color-bce",
                                                 "screen-bce",
                                                 "screen-w",
                                                 "screen.linux",
                                                 "st-256color",
                                                 "tmux",
                                                 "tmux-256color",
                                                 "vt100",
                                                 "xterm",
                                                 "xterm-16color",
                                                 "xterm-256color",
                                                 "xterm-88color",
                                                 "xterm-color",
                                                 "xterm-debian",
                                                 "xterm-kitty",
                                                 "xterm-termite",
                                                 0 };

/* Detect whether a stream is displayed in a VT100-compatible terminal.  */
static int kwsysTerminalStreamIsVT100(FILE* stream, int default_vt100,
                                      int default_tty)
{
  /* Force color according to https://bixense.com/clicolors/ convention.  */
  {
    char const* clicolor_force = getenv("CLICOLOR_FORCE");
    if (clicolor_force && *clicolor_force &&
        strcmp(clicolor_force, "0") != 0) {
      return 1;
    }
  }

  /* Disable color according to https://bixense.com/clicolors/ convention. */
  {
    char const* clicolor = getenv("CLICOLOR");
    if (clicolor && strcmp(clicolor, "0") == 0) {
      return 0;
    }
  }

  /* GNU make 4.1+ may tell us that its output is destined for a TTY. */
  {
    char const* termout = getenv("MAKE_TERMOUT");
    if (termout && *termout != '\0') {
      return 1;
    }
  }

  /* If running inside emacs the terminal is not VT100.  Some emacs
     seem to claim the TERM is xterm even though they do not support
     VT100 escapes.  */
  {
    char const* emacs = getenv("EMACS");
    if (emacs && *emacs == 't') {
      return 0;
    }
  }

  /* Check for a valid terminal.  */
  if (!default_vt100) {
    char const** t = 0;
    char const* term = getenv("TERM");
    if (term) {
      for (t = kwsysTerminalVT100Names; *t && strcmp(term, *t) != 0; ++t) {
      }
    }
    if (!(t && *t)) {
      return 0;
    }
  }

#if defined(KWSYS_TERMINAL_ISATTY_WORKS)
  /* Make sure the stream is a tty. */
  (void)default_tty;
  return isatty(fileno(stream)) ? 1 : 0;
#else
  /* Check for cases in which the stream is definitely not a tty.  */
  if (kwsysTerminalStreamIsNotInteractive(stream)) {
    return 0;
  }

  /* Use the provided default for whether this is a tty.  */
  return default_tty;
#endif
}

/* VT100 escape sequence strings.  */
#if defined(__MVS__)
/* if building on z/OS (aka MVS), assume we are using EBCDIC */
#  define ESCAPE_CHAR "\47"
#else
#  define ESCAPE_CHAR "\33"
#endif

#define KWSYS_TERMINAL_VT100_NORMAL ESCAPE_CHAR "[0m"
#define KWSYS_TERMINAL_VT100_BOLD ESCAPE_CHAR "[1m"
#define KWSYS_TERMINAL_VT100_UNDERLINE ESCAPE_CHAR "[4m"
#define KWSYS_TERMINAL_VT100_BLINK ESCAPE_CHAR "[5m"
#define KWSYS_TERMINAL_VT100_INVERSE ESCAPE_CHAR "[7m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_BLACK ESCAPE_CHAR "[30m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_RED ESCAPE_CHAR "[31m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_GREEN ESCAPE_CHAR "[32m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_YELLOW ESCAPE_CHAR "[33m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_BLUE ESCAPE_CHAR "[34m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_MAGENTA ESCAPE_CHAR "[35m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_CYAN ESCAPE_CHAR "[36m"
#define KWSYS_TERMINAL_VT100_FOREGROUND_WHITE ESCAPE_CHAR "[37m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_BLACK ESCAPE_CHAR "[40m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_RED ESCAPE_CHAR "[41m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_GREEN ESCAPE_CHAR "[42m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_YELLOW ESCAPE_CHAR "[43m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_BLUE ESCAPE_CHAR "[44m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_MAGENTA ESCAPE_CHAR "[45m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_CYAN ESCAPE_CHAR "[46m"
#define KWSYS_TERMINAL_VT100_BACKGROUND_WHITE ESCAPE_CHAR "[47m"

/* Write VT100 escape sequences to the stream for the given color.  */
static void kwsysTerminalSetVT100Color(FILE* stream, int color)
{
  if (color == kwsysTerminal_Color_Normal) {
    fprintf(stream, KWSYS_TERMINAL_VT100_NORMAL);
    return;
  }

  switch (color & kwsysTerminal_Color_ForegroundMask) {
    case kwsysTerminal_Color_Normal:
      fprintf(stream, KWSYS_TERMINAL_VT100_NORMAL);
      break;
    case kwsysTerminal_Color_ForegroundBlack:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_BLACK);
      break;
    case kwsysTerminal_Color_ForegroundRed:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_RED);
      break;
    case kwsysTerminal_Color_ForegroundGreen:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_GREEN);
      break;
    case kwsysTerminal_Color_ForegroundYellow:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_YELLOW);
      break;
    case kwsysTerminal_Color_ForegroundBlue:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_BLUE);
      break;
    case kwsysTerminal_Color_ForegroundMagenta:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_MAGENTA);
      break;
    case kwsysTerminal_Color_ForegroundCyan:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_CYAN);
      break;
    case kwsysTerminal_Color_ForegroundWhite:
      fprintf(stream, KWSYS_TERMINAL_VT100_FOREGROUND_WHITE);
      break;
    default:
      break;
  }
  switch (color & kwsysTerminal_Color_BackgroundMask) {
    case kwsysTerminal_Color_BackgroundBlack:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_BLACK);
      break;
    case kwsysTerminal_Color_BackgroundRed:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_RED);
      break;
    case kwsysTerminal_Color_BackgroundGreen:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_GREEN);
      break;
    case kwsysTerminal_Color_BackgroundYellow:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_YELLOW);
      break;
    case kwsysTerminal_Color_BackgroundBlue:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_BLUE);
      break;
    case kwsysTerminal_Color_BackgroundMagenta:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_MAGENTA);
      break;
    case kwsysTerminal_Color_BackgroundCyan:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_CYAN);
      break;
    case kwsysTerminal_Color_BackgroundWhite:
      fprintf(stream, KWSYS_TERMINAL_VT100_BACKGROUND_WHITE);
      break;
    default:
      break;
  }
  if (color & kwsysTerminal_Color_ForegroundBold) {
    fprintf(stream, KWSYS_TERMINAL_VT100_BOLD);
  }
}

#if defined(KWSYS_TERMINAL_SUPPORT_CONSOLE)

#  define KWSYS_TERMINAL_MASK_FOREGROUND                                      \
    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED |                    \
     FOREGROUND_INTENSITY)
#  define KWSYS_TERMINAL_MASK_BACKGROUND                                      \
    (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED |                    \
     BACKGROUND_INTENSITY)

/* Get the Windows handle for a FILE stream.  */
static HANDLE kwsysTerminalGetStreamHandle(FILE* stream)
{
  /* Get the C-library file descriptor from the stream.  */
  int fd = fileno(stream);

#  if defined(__CYGWIN__)
  /* Cygwin seems to have an extra pipe level.  If the file descriptor
     corresponds to stdout or stderr then obtain the matching windows
     handle directly.  */
  if (fd == fileno(stdout)) {
    return GetStdHandle(STD_OUTPUT_HANDLE);
  } else if (fd == fileno(stderr)) {
    return GetStdHandle(STD_ERROR_HANDLE);
  }
#  endif

  /* Get the underlying Windows handle for the descriptor.  */
  return (HANDLE)_get_osfhandle(fd);
}

/* Set color attributes in a Windows console.  */
static void kwsysTerminalSetConsoleColor(HANDLE hOut,
                                         CONSOLE_SCREEN_BUFFER_INFO* hOutInfo,
                                         FILE* stream, int color)
{
  WORD attributes = 0;
  switch (color & kwsysTerminal_Color_ForegroundMask) {
    case kwsysTerminal_Color_Normal:
      attributes |= hOutInfo->wAttributes & KWSYS_TERMINAL_MASK_FOREGROUND;
      break;
    case kwsysTerminal_Color_ForegroundBlack:
      attributes |= 0;
      break;
    case kwsysTerminal_Color_ForegroundRed:
      attributes |= FOREGROUND_RED;
      break;
    case kwsysTerminal_Color_ForegroundGreen:
      attributes |= FOREGROUND_GREEN;
      break;
    case kwsysTerminal_Color_ForegroundYellow:
      attributes |= FOREGROUND_RED | FOREGROUND_GREEN;
      break;
    case kwsysTerminal_Color_ForegroundBlue:
      attributes |= FOREGROUND_BLUE;
      break;
    case kwsysTerminal_Color_ForegroundMagenta:
      attributes |= FOREGROUND_RED | FOREGROUND_BLUE;
      break;
    case kwsysTerminal_Color_ForegroundCyan:
      attributes |= FOREGROUND_BLUE | FOREGROUND_GREEN;
      break;
    case kwsysTerminal_Color_ForegroundWhite:
      attributes |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
      break;
  }
  switch (color & kwsysTerminal_Color_BackgroundMask) {
    case kwsysTerminal_Color_Normal:
      attributes |= hOutInfo->wAttributes & KWSYS_TERMINAL_MASK_BACKGROUND;
      break;
    case kwsysTerminal_Color_BackgroundBlack:
      attributes |= 0;
      break;
    case kwsysTerminal_Color_BackgroundRed:
      attributes |= BACKGROUND_RED;
      break;
    case kwsysTerminal_Color_BackgroundGreen:
      attributes |= BACKGROUND_GREEN;
      break;
    case kwsysTerminal_Color_BackgroundYellow:
      attributes |= BACKGROUND_RED | BACKGROUND_GREEN;
      break;
    case kwsysTerminal_Color_BackgroundBlue:
      attributes |= BACKGROUND_BLUE;
      break;
    case kwsysTerminal_Color_BackgroundMagenta:
      attributes |= BACKGROUND_RED | BACKGROUND_BLUE;
      break;
    case kwsysTerminal_Color_BackgroundCyan:
      attributes |= BACKGROUND_BLUE | BACKGROUND_GREEN;
      break;
    case kwsysTerminal_Color_BackgroundWhite:
      attributes |= BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
      break;
  }
  if (color & kwsysTerminal_Color_ForegroundBold) {
    attributes |= FOREGROUND_INTENSITY;
  }
  if (color & kwsysTerminal_Color_BackgroundBold) {
    attributes |= BACKGROUND_INTENSITY;
  }
  fflush(stream);
  SetConsoleTextAttribute(hOut, attributes);
}
#endif
