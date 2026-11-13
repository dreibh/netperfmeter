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

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <getopt.h>
#include <unistd.h>

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

#if __has_include(<format>)
#include <format>
template<typename... Args>
static inline std::string formatText(const std::string_view fmt, const Args&... args)
{
   try {
      return std::vformat(fmt, std::make_format_args(args...));
   }
   catch(const std::format_error&) {
      return std::string(fmt);
   }
}
#else
template<typename... Args>
static inline std::string formatText(const std::string_view fmt, const Args&...)
{
   return std::string(fmt);
}
#endif

#include "fileio.h"
#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


struct VectorInfo
{
   std::string vectorPrefix;
   bool        splitMode { false };
   bool        found     { false };
};

struct VectorMetadata
{
   std::string name;
   std::string split;
   std::string object;
};


// ###### Read and process data file ########################################
static unsigned long long extractVectors(InputFile&               inputFile,
                                         OutputFile&              outputFile,
                                         const bool               defaultVectorSplittingMode,
                                         std::vector<VectorInfo>& vectorsToExtract,
                                         const bool               addLineNumbers,
                                         const char*              separator)
{
   std::map<unsigned int, VectorMetadata> vectorMap;
   unsigned long long                     outputLine  = 0;
   char                                   inBuffer[4096];
   bool                                   versionOkay = false;

   for(;;) {
      // ====== Read line from input file ===================================
      bool          eof = false;
      const ssize_t bytesRead = inputFile.readLine(inBuffer, sizeof(inBuffer), eof);
      if((bytesRead < 0) || eof) {
         break;
      }

      // Strip any trailing \r from DOS line endings
      size_t len = std::strlen(inBuffer);
      if(len > 0 && inBuffer[len - 1] == '\r') {
         inBuffer[--len] = '\0';
      }

      // ====== Process line ================================================
      if(!versionOkay) {
         // ====== Handle version number ====================================
         unsigned int version = 0;
         if((inputFile.getLine() == 1) &&
            (std::sscanf(inBuffer, "version %u", &version) == 1)) {
            if(version != 2) {
               // TRANSLATORS: Error message when input file has an unsupported version number. {} is the found version number.
               std::cerr << formatText(gettext("ERROR: Got unknown version number {} (expected 2)"), version) << "\n";
               std::exit(1);
            }
            versionOkay = true;
            outputFile.printf("Time%sEvent%sObject%sVector%sSplit%sValue\n",
                              separator, separator, separator, separator, separator);
            outputLine++;
         }
         else {
            // TRANSLATORS: Error message when the first line of the input file is not a "version" header.
            std::cerr << gettext("ERROR: Missing \"version\" entry in input file!") << "\n";
            std::exit(1);
         }
      }
      else {
         // ====== Handle data line =========================================
         unsigned int vectorID = 0;
         unsigned int event    = 0;
         double       simTime  = 0.0;
         double       value    = 0.0;
         int          n        = 0;
         if(std::sscanf(inBuffer, "%u %u %lf %lf%n",
                        &vectorID, &event, &simTime, &value, &n) == 4) {
            const auto found = vectorMap.find(vectorID);
            if(found != vectorMap.end()) {
               if(addLineNumbers) {
                  outputFile.printf("%llu%s%lf%s%u%s\"%s\"%s\"%s\"%s\"%s\"%s%lf\n",
                                    outputLine,                   separator,
                                    simTime,                      separator,
                                    event,                        separator,
                                    found->second.object.c_str(), separator,
                                    found->second.name.c_str(),   separator,
                                    found->second.split.c_str(),  separator,
                                    value);
               }
               else {
                  outputFile.printf("%lf%s%u%s\"%s\"%s\"%s\"%s\"%s\"%s%lf\n",
                                    simTime,                      separator,
                                    event,                        separator,
                                    found->second.object.c_str(), separator,
                                    found->second.name.c_str(),   separator,
                                    found->second.split.c_str(),  separator,
                                    value);
               }
               outputLine++;
            }
         }

         // ====== Handle vector definition =================================
         else if(std::strncmp(inBuffer, "vector ", 7) == 0) {
            char objectName[1024];
            char vectorName[1024];
            if((std::sscanf(inBuffer, "vector %u %1022s \"%1022[^\\\"]\" ETV",
                            &vectorID, objectName, vectorName) == 3) ||
               (std::sscanf(inBuffer, "vector %u %1022s %1022[^\\\"] ETV",
                            &vectorID, objectName, vectorName) == 3)) {
               bool includeVector = vectorsToExtract.empty();
               bool splitMode     = defaultVectorSplittingMode;

               if(!includeVector) {
                  for(auto& vectorInfo : vectorsToExtract) {
                     if(std::string_view(vectorName).starts_with(vectorInfo.vectorPrefix)) {
                        includeVector    = true;
                        vectorInfo.found = true;
                        if(vectorInfo.splitMode) {
                           splitMode = true;
                        }
                     }
                  }
               }

               if(includeVector) {
                  std::string vName(vectorName);
                  std::string splitName;
                  if(splitMode) {
                     const auto lastSpace = vName.rfind(' ');
                     if(lastSpace != std::string::npos) {
                        splitName = vName.substr(lastSpace + 1);
                        vName.erase(lastSpace);
                     }
                     // TRANSLATORS: Status message when adding a vector with split parameter. Placeholders: {} 1: vector name, 2: split parameter, 3: object name.
                     std::cout << formatText(gettext("Adding vector \"{}\", split \"{}\" of object {} ..."),
                                             vName, splitName, objectName) << "\n";
                  }
                  else {
                     // TRANSLATORS: Status message when adding a vector without split parameter. Placeholders: {} 1: vector name, 2: object name.
                     std::cout << formatText(gettext("Adding vector \"{}\" of object {} ..."),
                                             vName, objectName) << "\n";
                  }

                  vectorMap.emplace(vectorID, VectorMetadata {
                     .name   = std::move(vName),
                     .split  = std::move(splitName),
                     .object = std::string(objectName)
                  });
               }
            }
            else {
               // TRANSLATORS: Error message when a vector definition line cannot be parsed. {} is the line number.
               std::cerr << formatText(gettext("ERROR: Unexpected vector definition on input file {}!"),
                                       inputFile.getLine()) << "\n";
               std::exit(1);
            }
         }
      }
   }

   // ====== Warn, if no vector had been extracted ==========================
   const size_t uniqueFoundPrefixes = static_cast<size_t>(
      std::ranges::count_if(vectorsToExtract, [](const VectorInfo& v) { return v.found; })
   );
   if((uniqueFoundPrefixes < vectorsToExtract.size()) && !vectorsToExtract.empty()) {
      // TRANSLATORS: Warning message when fewer vector prefixes matched than requested. Placeholders: {} 1: matched count, 2: requested count.
      std::cerr << formatText(gettext("WARNING: Found only {} of {} specified!"),
                              uniqueFoundPrefixes, vectorsToExtract.size()) << "\n";
   }

   return outputLine;
}


// ###### Version ###########################################################
[[noreturn]] static void version()
{
   std::cout << "ExtractVectors " << EXTRACTVECTORS_VERSION << "\n";
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
   const std::string manpage(std::string("\x1b[1m") +
                             std::filesystem::path(program).stem().string() +
                             std::string("\x1b[0m"));
   const std::string homepage(std::string("\x1b[1m\x1b]8;;") +
                              std::string(PROJECT_HOMEPAGE_URL) +
                              "\a" + std::string(PROJECT_HOMEPAGE_URL) + "\a" +
                              std::string("\x1b]8;;\a\x1b[0m"));
   const std::string repository(std::string("\x1b[1m\x1b]8;;") +
                                std::string(PROJECT_SOURCE_URL) +
                                "\a" + std::string(PROJECT_SOURCE_URL) + "\a" +
                                std::string("\x1b]8;;\a\x1b[0m"));

   // TRANSLATORS: Command-line syntax message prefix:
   // "Usage: <program> [options]".
   std::cerr << HEADER(gettext("Usage:")) << "\n"
             << CMD(program) << " "
                ARG("input_file") " " ARG("output_file") "\n"
                   ENTRY(OPT("-c") " " ARG("level") PIPE OPT("--compress") " " ARG("level"))
                   ENTRY(OPT("-s") " " ARG("separator") PIPE OPT("--separator") " " ARG("separator"))
                   ENTRY(OPT("-l") PIPE OPT("--line-numbers") PIPE OPT("-n") PIPE OPT("--no-line-numbers"))
                   ENTRY(OPT("-p") PIPE OPT("--split") PIPE OPT("-a") PIPE OPT("--no-split"))
                   ENTRY(OPT("-q") PIPE OPT("--quiet"))
                   ENTRY(ARG("[!]vector_name_prefix") " " SYN("[") "..." SYN("]"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-h") PIPE OPT("--help"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-v") PIPE OPT("--version"))
             << "\n"
             // TRANSLATORS: Informational message at the end of usage:
             // "For further information, see the man page for <manpage>, read the documentation at [<url>], examine the source code at [<url>], or consult your favourite LLM.".
             // Placeholders: {} 1: manpage, 2: homepage URL, 3: repository URL.
             << formatText(gettext("For further information, see the man page for {}, read the documentation at [{}], examine the source code at [{}], or consult your favourite LLM."),
                           manpage, homepage, repository) << "\n";
   std::exit(exitCode);
}
// ------ END-OF-USAGE-BLOCK ------


// ###### Main program ======================================================
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
   bindtextdomain("netperfmeter",
                  ( ((textdomaindir) && (textdomaindir[0] != 0x00)) ?
                       textdomaindir : PROJECT_LOCALEDIR ));
#if defined(_WIN32)
   if(textdomaindir) {
       free(textdomaindir);
   }
#endif
   bind_textdomain_codeset("netperfmeter", "UTF-8");
   textdomain("netperfmeter");
#endif

   bool                    addLineNumbers      = false;
   int                     explicitLevel       = -1;
   const char*             separator           = "\t";
   bool                    vectorSplittingMode = false;
   bool                    quietMode           = false;
   std::vector<VectorInfo> vectorsToExtract;

   // ====== Handle command-line arguments ==================================
   static const struct option long_options[] = {
      { "compress",        required_argument, nullptr, 'c' },
      { "separator",       required_argument, nullptr, 's' },
      { "line-numbers",    no_argument,       nullptr, 'l' },
      { "no-line-numbers", no_argument,       nullptr, 'n' },
      { "split",           no_argument,       nullptr, 'p' },
      { "no-split",        no_argument,       nullptr, 'a' },
      { "quiet",           no_argument,       nullptr, 'q' },

      { "help",            no_argument,       nullptr, 'h' },
      { "version",         no_argument,       nullptr, 'v' },
      { nullptr,           0,                 nullptr, 0   }
   };

   int option    = 0;
   int longIndex = 0;
   while((option = getopt_long_only(argc, argv, "c:s:lnpaqhv", long_options, &longIndex)) != -1) {
      switch(option) {
         case 'c':
            explicitLevel = std::atoi(optarg);
            break;
         case 's':
            separator = optarg;
            break;
         case 'l':
            addLineNumbers = true;
            break;
         case 'n':
            addLineNumbers = false;
            break;
         case 'p':
            vectorSplittingMode = true;
            break;
         case 'a':
            vectorSplittingMode = false;
            break;
         case 'q':
            quietMode = true;
            break;
         case 'v':
            version();
            break;
         case 'h':
         case '?':
            usage(argv[0], (option == 'h') ? 0 : 1);
            break;
         case '-':
            break;
         default:
            std::cerr << "INTERNAL ERROR: Unhandled option code=" << option << "!\n";
            return 1;
      }
   }

   if(optind + 1 >= argc) {
      usage(argv[0], 1);
   }
   const std::string inputFileName(argv[optind++]);
   const std::string outputFileName(argv[optind++]);

   // ====== Detect formats & validate compression level ====================
   const FileFormat inputFileFormat  = detectFormat(inputFileName);
   const FileFormat outputFileFormat = detectFormat(outputFileName);

   unsigned int compressionLevel = 0;
   if(explicitLevel >= 0) {
      if(!isValidCompressionLevel(outputFileFormat, explicitLevel)) {
         // TRANSLATORS: Error message when requested compression level is invalid for the output format. Placeholders: {} 1: compression level, 2: format name.
         std::cerr << formatText(gettext("ERROR: Compression level {} is invalid for format {}!"),
                                 explicitLevel, getFormatName(outputFileFormat)) << "\n";
         if(outputFileFormat == FileFormat::Plain) {
            // TRANSLATORS: Hint that Plain (uncompressed) output only accepts compression level 0.
            std::cerr << gettext("Compression level for Plain format must be 0.") << "\n";
         }
         else {
            // TRANSLATORS: Allowed compression level range hint. Placeholders: {} 1: minimum level, 2: maximum level.
            std::cerr << formatText(gettext("Allowed range: {} to {}"),
                                    getMinCompressionLevel(outputFileFormat),
                                    getMaxCompressionLevel(outputFileFormat)) << "\n";
         }
         std::exit(1);
      }
      compressionLevel = static_cast<unsigned int>(explicitLevel);
   }
   else {
      compressionLevel = getDefaultCompressionLevel(outputFileFormat);
   }

   // ====== Print information ==============================================
   if(!quietMode) {
      std::cout << "ExtractVectors " << EXTRACTVECTORS_VERSION << "\n"
                // TRANSLATORS: Status label for vector name splitting mode ("on" or "off"). {} is "on" or "off".
                << formatText(gettext("* Vector Splitting:  {}"),
                              // TRANSLATORS: Boolean state label when an option is enabled.
                              // TRANSLATORS: Boolean state label when an option is disabled.
                              vectorSplittingMode ? gettext("on") : gettext("off")) << "\n"
                // TRANSLATORS: Status label for detected input file compression format. {} is format name.
                << formatText(gettext("* Input Format:      {}"), getFormatName(inputFileFormat)) << "\n"
                // TRANSLATORS: Status label for detected output file compression format. {} is format name.
                << formatText(gettext("* Output Format:     {}"), getFormatName(outputFileFormat)) << "\n"
                // TRANSLATORS: Status label for selected output compression level. {} is level number.
                << formatText(gettext("* Compression Level: {}"), compressionLevel) << "\n"
                << "\n";
   }

   // ====== Parse vector name prefixes =====================================
   for(int i = optind; i < argc; i++) {
      const std::string_view arg(argv[i]);
      if(!arg.empty() && arg.front() == '!') {
         vectorsToExtract.push_back(VectorInfo {
            .vectorPrefix = std::string(arg.substr(1)),
            .splitMode    = true,
            .found        = false
         });
      }
      else {
         vectorsToExtract.push_back(VectorInfo {
            .vectorPrefix = std::string(arg),
            .splitMode    = vectorSplittingMode,
            .found        = false
         });
      }
   }

   // ====== Process files ==================================================
   try {
      InputFile inputFile;
      inputFile.initialize(inputFileName, inputFileFormat);

      OutputFile outputFile;
      outputFile.initialize(outputFileName, outputFileFormat, compressionLevel);

      const unsigned long long lines = extractVectors(inputFile, outputFile,
                                                      vectorSplittingMode, vectorsToExtract,
                                                      addLineNumbers, separator);

      inputFile.finish();

      unsigned long long in  = 0;
      unsigned long long out = 0;
      outputFile.finish(true, &in, &out);

      if(!quietMode) {
         // TRANSLATORS: Summary message showing number of written output lines. {} is the line count.
         std::cout << formatText(gettext("Wrote {} lines"), lines);
         if(in > 0) {
            std::cout << " (" << in << " -> " << out << " - "
                      << (static_cast<double>(out) * 100.0 / static_cast<double>(in)) << "%)";
         }
         std::cout << "\n";
      }
   }
   catch(const InputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      std::exit(1);
   }
   catch(const OutputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      std::exit(1);
   }
   catch(const std::exception& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      std::exit(1);
   }

   return 0;
}
