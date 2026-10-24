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
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <getopt.h>

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
static inline std::string formatText(std::string_view formatString, const Args&... arguments)
{
   try {
      return std::vformat(formatString, std::make_format_args(arguments...));
   }
   catch(const std::format_error&) {
      return std::string(formatString);
   }
}
#else
template<typename... Args>
static inline std::string formatText(std::string_view formatString, const Args&...)
{
   return std::string(formatString);
}
#endif

#include "fileio.h"
#include "inputfile.h"
#include "outputfile.h"
#include "package-version.h"


// ====== Statistics Key and Storage ========================================
struct ScalarKey
{
   std::string scalarName;
   std::string splitName;
   std::string aggValues;
   std::string varValues;
   size_t      run { 0 };

   auto operator<=>(const ScalarKey&) const = default;
};

struct ScalarData
{
   std::string         aggNames;
   size_t              entries { 0 };
   std::vector<double> valueSet;
};

static std::map<ScalarKey, ScalarData> StatisticsStorage;
static std::vector<std::string>        SkipList;


// ###### String modification helpers =======================================
static inline void replaceSlashes(std::string& str)
{
   std::ranges::replace(str, '/', ':');
}

static inline void removeSpaces(std::string& str)
{
   std::erase(str, ' ');
}

static inline void removeBrackets(std::string& str)
{
   std::erase_if(str, [](char c) { return c == '[' || c == ']'; });
}

static inline void removeScenarioName(std::string& str)
{
   const auto dotPos = str.find('.');
   if(dotPos != std::string::npos) {
      str.erase(0, dotPos + 1);
   }
}


// ###### Check columns for proper quotation ================================
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


// ###### Get aggregate data ================================================
static unsigned int getAggregate(std::string_view objectName,
                                 std::string_view statName,
                                 std::string&     scalarName,
                                 std::string&     aggNames,
                                 std::string&     aggValues,
                                 const bool       topLevel = true)
{
   unsigned int levels = 0;
   std::string_view segment;
   const auto lastDot = objectName.rfind('.');

   if(lastDot == std::string_view::npos) {
      segment = objectName;
      scalarName.clear();
      aggNames.clear();
      aggValues.clear();
      levels = 0;
   }
   else {
      segment = objectName.substr(lastDot + 1);
      levels  = getAggregate(objectName.substr(0, lastDot), statName,
                             scalarName, aggNames, aggValues, false);
   }

   const auto lastNonDigit = segment.find_last_not_of("0123456789");
   std::string_view aggregate;
   std::string_view numPart;
   bool hasDigits = false;

   if(lastNonDigit == std::string_view::npos) {
      aggregate = "";
      numPart   = segment;
      hasDigits = !segment.empty();
   }
   else {
      aggregate = segment.substr(0, lastNonDigit + 1);
      if(lastNonDigit + 1 < segment.size()) {
         numPart   = segment.substr(lastNonDigit + 1);
         hasDigits = true;
      }
   }

   if(hasDigits) {
      if(!aggNames.empty()) {
         aggNames += ' ';
      }
      if(!scalarName.empty()) {
         aggNames += scalarName;
         aggNames += '.';
      }
      aggNames += aggregate;

      if(!aggValues.empty()) {
         aggValues += ' ';
      }
      unsigned long val = 0;
      std::from_chars(numPart.data(), numPart.data() + numPart.size(), val);
      aggValues += std::to_string(val);
   }

   if(!scalarName.empty()) {
      scalarName += '.';
   }
   scalarName += aggregate;

   if(topLevel) {
      const auto hashPos = statName.find('#');
      if(hashPos != std::string_view::npos) {
         const std::string_view statBase = statName.substr(0, hashPos);
         const std::string_view idPart   = statName.substr(hashPos + 1);
         const bool allDigits            = !idPart.empty() && std::ranges::all_of(idPart, [](char c) {
            return std::isdigit(static_cast<unsigned char>(c));
         });

         if(allDigits) {
            if(!aggNames.empty()) {
               aggNames += ' ';
            }
            aggNames += statBase;

            if(!aggValues.empty()) {
               aggValues += ' ';
            }
            aggValues += idPart;

            scalarName += '-';
            scalarName += statBase;
            return levels + 1;
         }
      }

      scalarName += '-';
      scalarName += statName;
   }

   return levels + 1;
}


// ###### Extract word from text (quoted or unquoted) =======================
static bool getWord(std::string_view& input, std::string& word)
{
   word.clear();
   while(!input.empty() && (input.front() == ' ' || input.front() == '\t')) {
      input.remove_prefix(1);
   }
   if(input.empty()) {
      return false;
   }

   bool quoted = false;
   if(input.front() == '"') {
      quoted = true;
      input.remove_prefix(1);
   }

   size_t i = 0;
   while(i < input.size()) {
      if(quoted) {
         if(input[i] == '"') {
            word = input.substr(0, i);
            input.remove_prefix(i + 1);
            return true;
         }
      }
      else {
         if(input[i] == ' ' || input[i] == '\t') {
            word = input.substr(0, i);
            input.remove_prefix(i + 1);
            return true;
         }
      }
      i++;
   }

   if(quoted) {
      return false;
   }
   word = input;
   input = {};
   return true;
}


// ###### Add scalar to storage =============================================
static void addScalar(std::string_view scalarName,
                      std::string_view splitName,
                      std::string_view aggNames,
                      std::string_view aggValues,
                      std::string_view varValues,
                      const size_t     runNumber,
                      const double     value)
{
   ScalarKey key {
      .scalarName = std::string(scalarName),
      .splitName  = std::string(splitName),
      .aggValues  = std::string(aggValues),
      .varValues  = std::string(varValues),
      .run        = runNumber
   };

   auto& entry = StatisticsStorage[std::move(key)];
   if(entry.aggNames.empty()) {
      entry.aggNames = std::string(aggNames);
   }
   entry.valueSet.push_back(value);
   entry.entries++;
}


// ###### Handle scalar =====================================================
static void handleScalar(const std::string& varValues,
                         const unsigned int run,
                         const bool         interactiveMode,
                         const bool         scalarSplittingMode,
                         std::string&       objectName,
                         std::string&       statName,
                         const double       value)
{
   std::string splitName;
   if(scalarSplittingMode) {
      const auto spacePos = statName.rfind(' ');
      if(spacePos != std::string::npos && spacePos + 1 < statName.size()) {
         if(std::isdigit(static_cast<unsigned char>(statName[spacePos + 1]))) {
            splitName = statName.substr(spacePos + 1);
            statName.erase(spacePos);
         }
      }
   }

   removeSpaces(statName);
   replaceSlashes(statName);
   removeBrackets(objectName);

   std::string scalarName;
   std::string aggNames;
   std::string aggValues;
   getAggregate(objectName, statName, scalarName, aggNames, aggValues);

   const bool isSkipped = std::ranges::any_of(SkipList, [&](std::string_view prefix) {
      return std::string_view(scalarName).starts_with(prefix);
   });

   if(!isSkipped) {
      addScalar(scalarName, splitName, aggNames, aggValues,
                varValues, run, value);
   }
   else if(interactiveMode) {
      std::cout << "Skipping entry " << scalarName << "\n";
   }
}


// ###### Read and process scalar file ======================================
static bool handleScalarFile(const std::string& varValues,
                             const std::string& fileName,
                             const bool         interactiveMode,
                             const bool         scalarSplitting)
{
   InputFile inputFile;
   try {
      inputFile.initialize(fileName, detectFormat(fileName));
   }
   catch(const InputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      return false;
   }

   char         buffer[4096];
   std::string  statisticObjectName;
   std::string  statisticBlockName;
   bool         hasStatistic = false;
   unsigned int run          = 0;
   bool         success      = true;

   try {
      for(;;) {
         bool eof = false;
         const ssize_t bytesRead = inputFile.readLine(buffer, sizeof(buffer), eof);
         if((bytesRead < 0) || eof) {
            break;
         }

         std::string_view line(buffer);
         if(line.ends_with('\n')) {
            line.remove_suffix(1);
         }
         if(line.ends_with('\r')) {
            line.remove_suffix(1);
         }

         if(line.starts_with("scalar ") || line.starts_with("scalar\t")) {
            std::string_view rem = line.substr(7);
            std::string objectName;
            std::string statName;
            if(!getWord(rem, objectName)) {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Object name expected for \"scalar\"!\n";
               success = false;
               break;
            }
            if(!getWord(rem, statName)) {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Statistics name expected for \"scalar\"!\n";
               success = false;
               break;
            }

            while(!rem.empty() && (rem.front() == ' ' || rem.front() == '\t')) {
               rem.remove_prefix(1);
            }

            double value = 0.0;
            auto [ptr, ec] = std::from_chars(rem.data(), rem.data() + rem.size(), value, std::chars_format::general);
            if(ec != std::errc{}) {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Value expected!\n";
               success = false;
               break;
            }

            removeScenarioName(objectName);
            handleScalar(varValues, run, interactiveMode, scalarSplitting,
                         objectName, statName, value);
         }
         else if(line.starts_with('#')) {
            // Comment line
         }
         else if(line.starts_with("bin")  || line.starts_with("par") ||
                 line.starts_with("attr") || line.starts_with("config")) {
            // Skip configuration/attribute items
         }
         else if(line.starts_with("run")) {
            run++;
         }
         else if(line.starts_with("field ")) {
            if(hasStatistic) {
               std::string_view rem = line.substr(6);
               std::string fieldName;
               if(getWord(rem, fieldName)) {
                  while(!rem.empty() && (rem.front() == ' ' || rem.front() == '\t')) {
                     rem.remove_prefix(1);
                  }
                  double value = 0.0;
                  auto [ptr, ec] = std::from_chars(rem.data(), rem.data() + rem.size(), value, std::chars_format::general);
                  if(ec != std::errc{}) {
                     std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                               << " - Value expected!\n";
                     success = false;
                     break;
                  }

                  std::string newStatName = fieldName;
                  if(newStatName == "stddev") {
                     newStatName = "StdDev";
                  }
                  else if(newStatName == "sqrsum") {
                     newStatName = "SqrSum";
                  }
                  else if(!newStatName.empty()) {
                     newStatName[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(newStatName[0])));
                  }
                  newStatName += statisticBlockName;

                  std::string objCopy = statisticObjectName;
                  handleScalar(varValues, run, interactiveMode, scalarSplitting,
                               objCopy, newStatName, value);
               }
            }
            else {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - \"field\" without \"statistic\"!\n";
               success = false;
               break;
            }
         }
         else if(line.starts_with("statistic ")) {
            std::string_view rem = line.substr(10);
            if(getWord(rem, statisticObjectName)) {
               if(!getWord(rem, statisticBlockName)) {
                  std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                            << " - Statistics name expected for \"statistic\"!\n";
                  success = false;
                  break;
               }
            }
            else {
               std::cerr << "ERROR: File \"" << fileName << "\", line " << inputFile.getLine()
                         << " - Object name expected for \"statistic\"!\n";
               success = false;
               break;
            }
            removeScenarioName(statisticObjectName);
            hasStatistic = true;
         }
         else if(line.empty()) {
            // Empty line
         }
         else if(line.starts_with("version")) {
            // Version header line
         }
         else {
            std::cerr << "NOTE: " << fileName << ":" << inputFile.getLine()
                      << " - Ignoring line \"" << line << "\"\n";
         }
      }

      inputFile.finish();
   }
   catch(const InputFileException& exception) {
      std::cerr << "ERROR: " << exception.what() << "\n";
      return false;
   }

   return success;
}


// ###### Close output file =================================================
static void closeOutputFile(OutputFile&                         outputFile,
                            [[maybe_unused]] const std::string& outputFileName,
                            const unsigned long long            lineNumber,
                            unsigned long long&                 totalIn,
                            unsigned long long&                 totalOut,
                            unsigned long long&                 totalLines,
                            unsigned int&                       totalFiles,
                            const bool                          interactiveMode)
{
   if(outputFile.exists()) {
      unsigned long long in  = 0;
      unsigned long long out = 0;
      outputFile.finish(true, &in, &out);
      totalIn    += in;
      totalOut   += out;
      totalLines += lineNumber;
      totalFiles++;
      if(interactiveMode) {
         std::cout << " (" << lineNumber << " lines";
         if(in > 0) {
            std::cout << ", " << in << " -> " << out << " - "
                      << (static_cast<double>(out) * 100.0 / static_cast<double>(in)) << "%";
         }
         std::cout << ")\n";
      }
   }
}


// ###### Dump scalars to output files ======================================
static void dumpScalars(const std::string& resultsDirectory,
                        const std::string& varNames,
                        const FileFormat   outputFormat,
                        const unsigned int compressionLevel,
                        const bool         interactiveMode,
                        const char*        separator,
                        const bool         addLineNumbers)
{
   std::string        fileName;
   std::string        lastStatisticsName;
   unsigned long long totalIn    = 0;
   unsigned long long totalOut   = 0;
   unsigned long long totalLines = 0;
   unsigned int       totalFiles = 0;
   unsigned long long lineNumber = 0;
   OutputFile         outputFile;

   for(const auto& [key, data] : StatisticsStorage) {
      if(lastStatisticsName != key.scalarName) {
         closeOutputFile(outputFile, fileName, lineNumber,
                         totalIn, totalOut, totalLines, totalFiles,
                         interactiveMode);

         const std::string baseName = resultsDirectory + "/" + key.scalarName + ".data";
         fileName = baseName + std::string(getFormatExtension(outputFormat));

         std::error_code ec;
         std::filesystem::remove(fileName, ec);

         if(interactiveMode) {
            std::cout << "Statistics \"" << key.scalarName << "\" ...";
         }
         std::cout.flush();

         outputFile.initialize(fileName, outputFormat, compressionLevel);
         lineNumber = 1;

         outputFile.printf("RunNo%sValueNo%sSplit%s%s%s%s%s%s\n",
                           separator, separator, separator,
                           data.aggNames.c_str(), separator,
                           varNames.c_str(),      separator,
                           key.scalarName.c_str());
         lastStatisticsName = key.scalarName;
      }

      size_t valueNumber = 1;
      for(const double val : data.valueSet) {
         if(addLineNumbers) {
            outputFile.printf("%llu%s", lineNumber, separator);
         }
         outputFile.printf("%u%s%u%s\"%s\"%s%s%s%s%s%lf\n",
                           static_cast<unsigned int>(key.run), separator,
                           static_cast<unsigned int>(valueNumber), separator,
                           key.splitName.c_str(), separator,
                           key.aggValues.c_str(), separator,
                           key.varValues.c_str(), separator,
                           val);
         valueNumber++;
         lineNumber++;
      }
   }

   closeOutputFile(outputFile, fileName, lineNumber,
                   totalIn, totalOut, totalLines, totalFiles,
                   interactiveMode);

   std::cout << "Wrote " << totalLines << " lines into "
             << totalFiles << " files";
   if(totalIn > 0) {
      std::cout << ", " << totalIn << " -> " << totalOut << " - "
                << (static_cast<double>(totalOut) * 100.0 / static_cast<double>(totalIn)) << "%";
   }
   std::cout << "\n";
}


// ###### Version ===========================================================
[[noreturn]] static void version()
{
   std::cout << "CreateSummary " << CREATESUMMARY_VERSION << "\n";
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
             << CMD(program) << "\n"
                   ENTRY(OPT("-b") PIPE OPT("--batch") PIPE OPT("-i") PIPE OPT("--interactive"))
                   ENTRY(OPT("-F") " " ARG("format") PIPE OPT("--format") " " ARG("format"))
                   ENTRY(OPT("-c") " " ARG("level") PIPE OPT("--compress") " " ARG("level"))
                   ENTRY(OPT("-s") " " ARG("separator") PIPE OPT("--separator") " " ARG("separator"))
                   ENTRY(OPT("-l") PIPE OPT("--line-numbers") PIPE OPT("-n") PIPE OPT("--no-line-numbers"))
                   ENTRY(OPT("-p") PIPE OPT("--split") PIPE OPT("-a") PIPE OPT("--no-split"))
                   ENTRY(OPT("-r") PIPE OPT("--ignore-scalar-file-errors"))
                   ENTRY(OPT("-q") PIPE OPT("--quiet"))
                   ENTRY(ARG("variable_names"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-h") PIPE OPT("--help"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-v") PIPE OPT("--version"))
             << "\n"
             << formatText("For further information, see the man page for {}, read the documentation at [{}], examine the source code at [{}], or consult your favourite LLM.",
                           manpage, homepage, repository) << "\n";
   std::exit(exitCode);
}
// ------ END-OF-USAGE-BLOCK ------


// ###### Main program ======================================================
int main(int argc, char** argv)
{
   FileFormat   outputFormat           = FileFormat::BZip2;
   int          explicitLevel          = -1;
   const char*  separator              = "\t";
   bool         interactiveMode        = true;
   bool         addLineNumbers         = false;
   bool         scalarSplittingMode    = false;
   bool         quietMode              = false;
   bool         ignoreScalarFileErrors = false;
   std::string  varNames               = "_NoVarNamesGiven_";
   std::string  varValues;
   std::string  simulationsDirectory   = ".";
   std::string  resultsDirectory       = ".";
   std::string  logFileName;
   std::string  statusFileName;

   // ====== Handle command-line arguments ==================================
   static const struct option long_options[] = {
      { "interactive",               no_argument,       nullptr, 'i' },
      { "batch",                     no_argument,       nullptr, 'b' },

      { "format",                    required_argument, nullptr, 'F' },
      { "compress",                  required_argument, nullptr, 'c' },
      { "separator",                 required_argument, nullptr, 's' },
      { "line-numbers",              no_argument,       nullptr, 'l' },
      { "no-line-numbers",           no_argument,       nullptr, 'n' },
      { "split",                     no_argument,       nullptr, 'p' },
      { "no-split",                  no_argument,       nullptr, 'a' },
      { "ignore-scalar-file-errors", no_argument,       nullptr, 'r' },
      { "quiet",                     no_argument,       nullptr, 'q' },

      { "help",                      no_argument,       nullptr, 'h' },
      { "version",                   no_argument,       nullptr, 'v' },
      { nullptr,                     0,                 nullptr, 0   }
   };

   int option = 0;
   int longIndex = 0;
   while((option = getopt_long_only(argc, argv, "ibF:c:s:lnparqhv", long_options, &longIndex)) != -1) {
      switch(option) {
         case 'i':
            interactiveMode = true;
            break;
         case 'b':
            interactiveMode = false;
            break;
         case 'F': {
            outputFormat = parseFormat(optarg);
            if(outputFormat == FileFormat::None) {
               std::cerr << "ERROR: Invalid output format \"" << optarg << "\"!\n"
                         << "Allowed formats: plain, bz2, gzip, xz, lzma, zstd\n";
               std::exit(1);
            }
            break;
         }
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
            scalarSplittingMode = true;
            break;
         case 'a':
            scalarSplittingMode = false;
            break;
         case 'r':
            ignoreScalarFileErrors = true;
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

   // ====== Validate compression level for format ==========================
   unsigned int compressionLevel = 0;
   if(explicitLevel >= 0) {
      if(!isValidCompressionLevel(outputFormat, explicitLevel)) {
         std::cerr << "ERROR: Compression level " << explicitLevel
                   << " is invalid for format " << getFormatName(outputFormat) << "!\n";
         if(outputFormat == FileFormat::Plain) {
            std::cerr << "Compression level for Plain format must be 0.\n";
         }
         else {
            std::cerr << "Allowed range: " << getMinCompressionLevel(outputFormat)
                      << " to " << getMaxCompressionLevel(outputFormat) << "\n";
         }
         std::exit(1);
      }
      compressionLevel = static_cast<unsigned int>(explicitLevel);
   }
   else {
      compressionLevel = getDefaultCompressionLevel(outputFormat);
   }

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

   // ====== Print information ==============================================
   if(!quietMode) {
      std::cout << "CreateSummary " << CREATESUMMARY_VERSION << "\n"
                << "* Interactive Mode:  " << (interactiveMode      ? "on" : "off") << "\n"
                << "* Output Format:     " << getFormatName(outputFormat) << "\n"
                << "* Compression Level: " << compressionLevel << "\n"
                << "* Separator:         \"" << separator << "\"\n"
                << "* Line Numbers:      " << (addLineNumbers       ? "on" : "off") << "\n"
                << "* Scalar Splitting:  " << (scalarSplittingMode  ? "on" : "off") << "\n"
                << "\n";
   }

   if(interactiveMode) {
      std::cout << "Ready> " << std::flush;
   }
   else {
      std::cout << "Processing input ...\n";
   }

   bool scalarFileError = false;
   std::string line;

   while(std::getline(std::cin, line)) {
      if(line.ends_with('\r')) {
         line.pop_back();
      }

      if(line.empty()) {
         if(interactiveMode) {
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
         if(!handleScalarFile(varValues, inputFilePath,
                              interactiveMode, scalarSplittingMode)) {
            scalarFileError = true;
            if(!logFileName.empty()) {
               std::cerr << " => see logfile " << logFileName << "\n";
            }
            if(!statusFileName.empty()) {
               std::cerr << " Removing status file; restart simulation to re-create this run!\n";
               std::error_code ec;
               std::filesystem::remove(statusFileName, ec);
            }
         }
         varValues.clear();
         logFileName.clear();
         statusFileName.clear();
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
      else if(line.starts_with("--logfile=")) {
         logFileName = line.substr(10);
      }
      else if(line.starts_with("--statusfile=")) {
         statusFileName = line.substr(13);
      }
      else if(line.starts_with("--skip=")) {
         SkipList.emplace_back(line.substr(7));
      }
      else if(line.starts_with("--simulationsdirectory=")) {
         simulationsDirectory = line.substr(23);
      }
      else if(line.starts_with("--resultsdirectory=")) {
         resultsDirectory = line.substr(19);
      }
      else if(line == "--splitall") {
         scalarSplittingMode = true;
      }
      else if(line == "--ignore-scalar-file-errors") {
         ignoreScalarFileErrors = true;
      }
      else if(line.starts_with("--level=")) {
         // Deprecated, ignore
      }
      else {
         std::cerr << "ERROR: Invalid command \"" << line << "\"!\n"
                   << "Examples:\n"
                   << "   --skip=calcAppServer\n"
                   << "   --simulationsdirectory=MySimulationsDirectory\n"
                   << "   --resultsdirectory=MySimulationsDirectory/Results\n"
                   << "   --varnames=Alpha Beta Gamma\n"
                   << "   --values=alpha100 beta200 gamma300\n"
                   << "   --input=simulations/scalar-file.sca.bz2\n"
                   << "   (Ctrl-D, EOF)\n";
         if(!interactiveMode) {
            std::exit(1);
         }
      }

      if(interactiveMode) {
         std::cout << "Ready> " << std::flush;
      }
   }

   // ====== Write results ==================================================
   if(interactiveMode) {
      std::cout << "\n\n";
   }
   if(scalarFileError) {
      if(!ignoreScalarFileErrors) {
         std::cerr << "ERROR: Not all scalar files have been read -> aborting!\n";
         std::exit(1);
      }
      std::cerr << "WARNING: Not all scalar files have been read -> continuing!\n";
   }

   try {
      std::cout << "Writing scalar files...\n";
      dumpScalars(resultsDirectory, varNames, outputFormat,
                  compressionLevel, interactiveMode, separator, addLineNumbers);
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
