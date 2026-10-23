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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <getopt.h>
#include <unistd.h>

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

#include "ioformat.h"
#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


// ###### Check columns for proper quotation ################################
static bool checkColumns(std::string_view values)
{
   bool         inQuotation    = false;
   unsigned int quotationMarks = 0;
   for(size_t i = 0; i < values.size(); i++) {
      switch(values[i]) {
         case '\\':
            i++;
            break;
         case '"':
            inQuotation = !inQuotation;
            quotationMarks++;
            break;
      }
   }
   return ((quotationMarks & 1) == 0);
}


// ###### Read and process data file ########################################
static void addDataFile(OutputFile&         outputFile,
                        const bool          withLineNumbers,
                        unsigned long long& outputLineNumber,
                        const std::string&  varNames,
                        const std::string&  varValues,
                        const std::string&  inputFileName,
                        const char*         separator = "\t")
{
   // ====== Open input file ================================================
   InputFile inputFile;
   try {
      inputFile.initialize(inputFileName, detectFormat(inputFileName));
   }
   catch(const InputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      std::exit(1);
   }

   char buffer[4096];
   try {
      for(;;) {
         // ====== Read line from input file ================================
         bool eof = false;
         const ssize_t bytesRead = inputFile.readLine(buffer, sizeof(buffer), eof);
         if((bytesRead < 0) || eof) {
            break;
         }

         // Strip any trailing \r from DOS line endings
         size_t len = std::strlen(buffer);
         if(len > 0 && buffer[len - 1] == '\r') {
            buffer[--len] = '\0';
         }

         // ====== Process line =============================================
         if(withLineNumbers) {
            if(inputFile.getLine() == 1) {
               if(outputLineNumber == 0) {
                  outputFile.printf("%s%sSubLineNo%s%s\n",
                                    varNames.c_str(), separator, separator, buffer);
                  outputLineNumber++;
               }
            }
            else {
               outputFile.printf("%llu%s%s%s%s\n",
                                 outputLineNumber, separator,
                                 varValues.c_str(), separator,
                                 buffer);
               outputLineNumber++;
            }
         }
         else {
            if(inputFile.getLine() == 1) {
               if(outputLineNumber == 0) {
                  outputFile.printf("%s%s%s\n",
                                    varNames.c_str(), separator, buffer);
                  outputLineNumber++;
               }
            }
            else {
               outputFile.printf("%s%s%s\n",
                                 varValues.c_str(), separator, buffer);
               outputLineNumber++;
            }
         }
      }

      inputFile.finish();
   }
   catch(const InputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      std::exit(1);
   }
}


// ###### Version ###########################################################
[[noreturn]] static void version()
{
   std::cout << "CombineSummaries " << COMBINESUMMARIES_VERSION << "\n";
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

   std::cerr << HEADER("Usage:") << "\n"
             << CMD(program) << " "
                ARG("output_file") "\n"
                   ENTRY(OPT("-c") " " ARG("level") PIPE OPT("--compress") " " ARG("level"))
                   ENTRY(OPT("-s") " " ARG("separator") PIPE OPT("--separator") " " ARG("separator"))
                   ENTRY(OPT("-l") PIPE OPT("--line-numbers") PIPE OPT("-n") PIPE OPT("--no-line-numbers"))
                   ENTRY(OPT("-q") PIPE OPT("--quiet"))
                   ENTRY(ARG("variable_names"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-h") PIPE OPT("--help"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-v") PIPE OPT("--version"))
             << "\n"
             << formatText("For further information, see the man page for {}, read the documentation at [{}], examine the source code at [{}], or consult your favourite LLM.",
                           manpage, homepage, repository) << "\n";
   exit(exitCode);
}
// ------ END-OF-USAGE-BLOCK ------


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   bool        quietMode       = false;
   bool        withLineNumbers = false;
   int         explicitLevel   = -1;
   const char* separator       = "\t";

   // ====== Handle command-line arguments ==================================
   static const struct option long_options[] = {
      { "compress",        required_argument, nullptr, 'c' },
      { "separator",       required_argument, nullptr, 's' },
      { "line-numbers",    no_argument,       nullptr, 'l' },
      { "no-line-numbers", no_argument,       nullptr, 'n' },
      { "quiet",           no_argument,       nullptr, 'q' },

      { "help",            no_argument,       nullptr, 'h' },
      { "version",         no_argument,       nullptr, 'v' },
      { nullptr,           0,                 nullptr, 0   }
   };

   int option = 0;
   int longIndex = 0;
   while((option = getopt_long_only(argc, argv, "c:s:lnqhv", long_options, &longIndex)) != -1) {
      switch(option) {
         case 'c':
            explicitLevel = std::atoi(optarg);
            break;
         case 's':
            separator = optarg;
            break;
         case 'l':
            withLineNumbers = true;
            break;
         case 'n':
            withLineNumbers = false;
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

   if(optind >= argc) {
      usage(argv[0], 1);
   }
   const std::string outputFileName(argv[optind++]);
   std::string       varNames;
   if(optind < argc) {
      varNames = argv[optind++];
      if(!checkColumns(varNames)) {
         std::cerr << "ERROR: Invalid quotation in variable names string!\n";
         std::exit(1);
      }
   }
   if(optind < argc) {
      std::cerr << "ERROR: Invalid option " << argv[optind] << "!\n";
      usage(argv[0], 1);
   }

   // ====== Detect output format & validate compression level ==============
   const OutputFileFormat outputFileFormat = detectFormat(outputFileName);
   unsigned int compressionLevel = 0;
   if(explicitLevel >= 0) {
      if(!isValidCompressionLevel(outputFileFormat, explicitLevel)) {
         std::cerr << "ERROR: Compression level " << explicitLevel
                   << " is invalid for format " << getFormatName(outputFileFormat) << "!\n";
         if(outputFileFormat == FileFormat::Plain) {
            std::cerr << "Compression level for Plain format must be 0.\n";
         }
         else {
            std::cerr << "Allowed range: " << getMinCompressionLevel(outputFileFormat)
                      << " to " << getMaxCompressionLevel(outputFileFormat) << "\n";
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
      std::cout << "CombineSummaries " << COMBINESUMMARIES_VERSION << "\n"
                << "* Output Format:     " << getFormatName(outputFileFormat) << "\n"
                << "* Compression Level: " << compressionLevel << "\n"
                << "* Separator:         \"" << separator << "\"\n"
                << "* Line Numbers:      " << (withLineNumbers ? "yes" : "no") << "\n"
                << "\n";
   }

   // ====== Open output file and process input =============================
   try {
      OutputFile outputFile;
      outputFile.initialize(outputFileName, outputFileFormat, compressionLevel);

      unsigned long long outputLineNumber = 0;
      std::string        varValues;
      std::string        simulationsDirectory = ".";
      std::string        line;

      if(!quietMode) {
         std::cout << "Ready> " << std::flush;
      }

      while(std::getline(std::cin, line)) {
         if(line.ends_with('\r')) {
            line.pop_back();
         }

         if(line.empty()) {
            if(!quietMode) {
               std::cout << "Ready> " << std::flush;
            }
            continue;
         }

         if(line.starts_with("--values=")) {
            varValues = line.substr(9);
            if(varValues.size() >= 2 && varValues.front() == '"' && varValues.back() == '"') {
               varValues = varValues.substr(1, varValues.size() - 2);
            }
            if(!checkColumns(varValues)) {
               std::cerr << "ERROR: Invalid quotation in variable values string!\n";
               std::exit(1);
            }
         }
         else if(line.starts_with("--input=")) {
            if(varValues.empty()) {
               std::cerr << "ERROR: No values given (parameter --values=...)!\n";
               std::exit(1);
            }
            const std::string inputFilePath = simulationsDirectory + "/" + std::string(line.substr(8));
            addDataFile(outputFile, withLineNumbers, outputLineNumber,
                        varNames, varValues,
                        inputFilePath,
                        separator);
            varValues.clear();
         }
         else if(line.starts_with("--varnames=")) {
            varNames = line.substr(11);
            if(varNames.size() >= 2 && varNames.front() == '"' && varNames.back() == '"') {
               varNames = varNames.substr(1, varNames.size() - 2);
            }
            if(!checkColumns(varNames)) {
               std::cerr << "ERROR: Invalid quotation in variable names string!\n";
               std::exit(1);
            }
         }
         else if(line.starts_with("--simulationsdirectory=")) {
            simulationsDirectory = line.substr(23);
         }
         else {
            std::cerr << "ERROR: Invalid command!\n";
            std::exit(1);
         }

         if(!quietMode) {
            std::cout << "Ready> " << std::flush;
         }
      }

      // ====== Close output file ===========================================
      unsigned long long in = 0;
      unsigned long long out = 0;
      outputFile.finish(true, &in, &out);

      if(!quietMode) {
         std::cout << "\nWrote " << outputLineNumber << " lines";
         if(in > 0) {
            std::cout << " (" << in << " -> " << out << " - "
                      << (static_cast<double>(out) * 100.0 / static_cast<double>(in)) << "%)";
         }
         std::cout << "\n";
      }
   }
   catch(const OutputFileException& ex) {
      std::cerr << "ERROR: " << ex.what() << "\n";
      std::exit(1);
   }
   catch(const std::exception& ex) {
      std::cerr << "ERROR: " << ex.what() << "\n";
      std::exit(1);
   }

   return 0;
}
