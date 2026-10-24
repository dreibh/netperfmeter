#ifndef INPUTFILE_H
#define INPUTFILE_H

#include "fileio.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>

class InputFileException : public std::runtime_error
{
   public:
   InputFileException(std::string_view message,
                      std::string_view filename  = {},
                      int              errorCode = 0,
                      std::string_view reason    = {})
      : std::runtime_error(formatErrorMessage(message, filename, errorCode, reason)),
        Filename(filename),
        Reason(!reason.empty() ? std::string(reason) : (errorCode != 0 ? std::string(std::strerror(errorCode)) : std::string())),
        ErrorCode(errorCode)
   {
   }

   [[nodiscard]] const std::string& getFileName() const noexcept { return Filename; }
   [[nodiscard]] const std::string& getReason() const noexcept   { return Reason; }
   [[nodiscard]] int getErrorCode() const noexcept               { return ErrorCode; }

   private:
   static std::string formatErrorMessage(std::string_view message,
                                         std::string_view filename,
                                         int              errorCode,
                                         std::string_view reason)
   {
      std::string result(message);
      if(!filename.empty()) {
         result += " <";
         result += filename;
         result += ">";
      }
      if(!reason.empty()) {
         result += ": ";
         result += reason;
      }
      else if(errorCode != 0) {
         result += ": ";
         result += std::strerror(errorCode);
      }
      return result;
   }

   std::string Filename;
   std::string Reason;
   int         ErrorCode { 0 };
};

class InputFile
{
   public:
   InputFile();
   ~InputFile();

   InputFile(const InputFile&) = delete;
   InputFile& operator=(const InputFile&) = delete;

   InputFile(InputFile&& other) noexcept;
   InputFile& operator=(InputFile&& other) noexcept;

   void initialize(const std::filesystem::path& name,
                   InputFileFormat              format = InputFileFormat::Plain);

   void initialize(const char*     name,
                   InputFileFormat format = InputFileFormat::Plain);

   void finish(bool closeFile = true);

   size_t read(char* buffer, size_t bufferSize);
   inline size_t read(void* buffer, size_t bufferSize) {
      return read(static_cast<char*>(buffer), bufferSize);
   }

   ssize_t readLine(char* buffer, size_t bufferSize, bool& eof);
   bool readLine(std::string& line);

   [[nodiscard]] bool exists() const noexcept;
   [[nodiscard]] InputFileFormat getFormat() const noexcept  { return Format; }
   [[nodiscard]] FILE* getFile() const noexcept;
   [[nodiscard]] const std::string& getName() const noexcept { return Name;   }
   [[nodiscard]] unsigned long long getLine() const noexcept { return Line;   }

   private:
   struct Impl;
   std::unique_ptr<Impl> Pimpl;

   InputFileFormat    Format { InputFileFormat::None };
   std::string        Name;
   unsigned long long Line   { 0 };
};

#endif // INPUTFILE_H
