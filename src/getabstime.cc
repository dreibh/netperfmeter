/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
 * ==========================================================================
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include <cctype>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <getopt.h>
#include <iostream>

#if defined(ENABLE_NLS)
#include <libintl.h>
#else
#define gettext(s) (s)
#endif

#if defined(HAVE_LIBIBERTY)
#include <libiberty.h>
extern "C" {
int getopt_long_only(int argc, char* const* argv, const char* optstring,
                     const struct option* longopts, int* longindex);
}
#endif

#include "package-version.h"


// ###### Modern C++20 string formatting ###################################
template<typename... Args>
inline std::string formatText(const std::string_view fmt, const Args&... args)
{
   try {
      return std::vformat(fmt, std::make_format_args(args...));
   }
   catch(const std::format_error&) {
      return std::string(fmt);
   }
}


// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "GetAbsTime" << " " << GETABSTIME_VERSION << "\n";
   std::exit(0);
}


// ------ BEGIN-OF-USAGE-BLOCK ------
// ###### Usage #############################################################
#define HEADER(s)    "\x1b[4m" << (s) << "\x1b[0m"
#define CMD(s)       "\x1b[1m" << (s) << "\x1b[0m"
#define OPT(s)       "\x1b[1m" s "\x1b[0m"
#define ARG(s)       "\x1b[4m" s "\x1b[0m"
#define SYN(s)       "\x1b[1m" s "\x1b[0m"
#define PIPE         SYN(" | ")
#define ENTRY(s)     "\t" SYN("[") s SYN("]\n")

[[ noreturn ]]
static void usage(const char* program, const int exitCode)
{
   // TRANSLATORS: Text direction: LTR (Left-to-Right) or RTL (Right-to-Left)
   const bool rtl = (std::string(gettext("LTR")) == "RTL");
   const std::string manpage(std::string("\x1b[1m") +
                             std::filesystem::path(program).stem().string() +
                             std::string("\x1b[0m"));
   const std::string homepage((rtl ? std::string("\xe2\x80\x8e") : std::string("")) +
                              std::string("[\x1b[1m\x1b]8;;") +
                              std::string(PROJECT_HOMEPAGE_URL) +
                              "\a" + std::string(PROJECT_HOMEPAGE_URL) + "\a" +
                              std::string("\x1b]8;;\a\x1b[0m]") +
                              (rtl ? std::string("\xe2\x80\x8f") : std::string("")) );
   const std::string repository((rtl ? std::string("\xe2\x80\x8e") : std::string("")) +
                                std::string("[\x1b[1m\x1b]8;;") +
                                std::string(PROJECT_SOURCE_URL) +
                                "\a" + std::string(PROJECT_SOURCE_URL) + "\a" +
                                std::string("\x1b]8;;\a\x1b[0m]") +
                                (rtl ? std::string("\xe2\x80\x8f") : std::string("")) );

   // TRANSLATORS: Command-line syntax message prefix:
   // "Usage: <program> [options]".
   std::cerr << HEADER(gettext("Usage:")) << "\n"
             << CMD(program) << "\n"
             << CMD(program) << "\n"
                   ENTRY(OPT("-h") PIPE OPT("--help"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-v") PIPE OPT("--version"))
             << "\n"
             // TRANSLATORS: Informational message at the end of usage:
             // "For further information, see the man page for <manpage>, read the documentation at <url>, examine the source code at <url>, or consult your favourite LLM.".
             << formatText(gettext("For further information, see the man page for {}, read the documentation at {}, examine the source code at {}, or consult your favourite LLM."),
                           manpage, homepage, repository) << "\n";
   std::exit(exitCode);
}
// ------ END-OF-USAGE-BLOCK ------


// ###### Get current time stamp ############################################
static unsigned long long getMicroTime()
{
   using MicrosecondsU64 = std::chrono::duration<unsigned long long, std::micro>;
   return std::chrono::duration_cast<MicrosecondsU64>(
      std::chrono::system_clock::now().time_since_epoch()
   ).count();
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialise locale support ======================================
   if(std::setlocale(LC_ALL, "") == nullptr) {
      std::setlocale(LC_ALL, "C.UTF-8");
   }
   std::setlocale(LC_NUMERIC, "C");   // Use "." for fractional numbers!

   // ====== Initialise i18n support ========================================
#if defined(ENABLE_NLS)
#if !defined(_WIN32)
   const char* textdomaindir     = std::getenv("TEXTDOMAINDIR");
#else
   char*       textdomaindir     = nullptr;
   size_t      textdomaindirsize = 0;
   _dupenv_s(&textdomaindir, &textdomaindirsize, "TEXTDOMAINDIR");
#endif
   bindtextdomain("getabstime",
                  ( ((textdomaindir) && (textdomaindir[0] != 0x00)) ?
                       textdomaindir : PROJECT_LOCALEDIR ));
#if defined(_WIN32)
   if(textdomaindir) {
       free(textdomaindir);
   }
#endif
   bind_textdomain_codeset("getabstime", "UTF-8");
   textdomain("getabstime");
#endif

   // ====== Handle command-line arguments ==================================
   const static struct option long_options[] = {
      { "help",    no_argument, 0, 'h' },
      { "version", no_argument, 0, 'v' },
      {  nullptr,  0,           0, 0   }
   };

   int option;
   int longIndex;
   while( (option = getopt_long_only(argc, argv, "hv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 'v':
            version();
          break;
         case 'h':
         case '?':
            // Exit with 0 on h/help, exit with 1 on '?' (unknown option):
            usage(argv[0], (option == 'h') ? 0 : 1);
          break;
         case '-':
          break;
         default:
            // This should not happen: wrong getopt parameters, or missing case?
            fprintf(stderr, "INTERNAL ERROR: Unhandled option c=%c code=%x!\n",
                    (isprint(option) ? (char)option : ' '), option);
            return 1;
          break;
      }
   }
   if(optind < argc) {
      // TRANSLATORS: Error message when an unexpected positional argument or invalid option is given. {} is the argument.
      std::cerr << formatText(gettext("ERROR: Invalid option {}!"), argv[optind]) << "\n";
      usage(argv[0], 1);
   }

   // ====== Print time stamp ===============================================
   std::cout << getMicroTime() << "\n";
   return 0;
}
