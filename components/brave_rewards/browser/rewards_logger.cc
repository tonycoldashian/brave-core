/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_rewards/browser/rewards_logger.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace brave_rewards {

namespace {

constexpr size_t kChunkSize = 1024;
constexpr size_t kDividerLength = 80;
constexpr int32_t kLinesAfterTrim = 20000;
constexpr int64_t kMaxFileSize = 10 * 1024 * 1024;

class Logger : public mojom::RewardsLogger {
 public:
  explicit Logger(const base::FilePath& file_path) : file_path_(file_path) {}

  ~Logger() override {}

  Logger& operator=(const Logger&) = delete;
  Logger(const Logger&) = delete;

  void ReadTail(int32_t lines, ReadTailCallback callback) override {
    std::move(callback).Run(ReadInternal(lines));
  }

  void ReadFile(ReadFileCallback callback) override {
    std::move(callback).Run(ReadInternal(-1));
  }

  void WriteMessage(const std::string& message,
                    const std::string& location,
                    int32_t line,
                    int32_t level,
                    WriteMessageCallback callback) override {
    bool success = WriteInternal(message, location, line, level);
    if (success) {
      first_write_ = false;
    }
    std::move(callback).Run(success);
  }

  void DeleteFile(DeleteFileCallback callback) override {
    bool success = base::DeleteFile(file_path_);
    std::move(callback).Run(success);
  }

  static void CreateForReceiver(
      const base::FilePath& file_path,
      mojo::PendingReceiver<mojom::RewardsLogger> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<Logger>(file_path),
                                std::move(receiver));
  }

 private:
  std::string ReadInternal(int32_t lines) {
    auto file = OpenFile(false);
    if (!file) {
      return "";
    }

    if (file->GetLength() == 0) {
      return "";
    }

    int64_t offset;

    if (lines < 0) {
      offset = 0;
    } else {
      offset = SeekFromEnd(file.get(), lines);
      if (offset == -1) {
        return "";
      }
    }

    if (file->Seek(base::File::FROM_BEGIN, offset) == -1) {
      return "";
    }

    const int64_t size = file->GetLength() - offset;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size + 1);

    if (file->ReadAtCurrentPos(buffer.get(), size) == -1) {
      return "";
    }

    return std::string(buffer.get());
  }

  bool WriteInternal(const std::string& message,
                     const std::string& location,
                     int32_t line,
                     int32_t level) {
    auto file = OpenFile(true);
    if (!file) {
      return false;
    }

    if (file->Seek(base::File::FROM_END, 0) == -1) {
      return false;
    }

    if (first_write_) {
      std::string divider = std::string(kDividerLength, '-') + "\n";
      file->WriteAtCurrentPos(divider.c_str(), divider.length());
    }

    std::string log_entry = FormatMessage(message, location, line, level);
    if (file->WriteAtCurrentPos(log_entry.c_str(), log_entry.length()) == -1) {
      return false;
    }

    return MaybeTrimBeginningOfFile(file.get());
  }

  std::unique_ptr<base::File> OpenFile(bool create) {
    auto file = std::make_unique<base::File>();

    file->Initialize(
        file_path_,
        (create ? base::File::FLAG_OPEN_ALWAYS : base::File::FLAG_OPEN) |
            base::File::FLAG_READ | base::File::FLAG_WRITE);

    return file->IsValid() ? std::move(file) : nullptr;
  }

  int64_t SeekFromEnd(base::File* file, int32_t lines) {
    DCHECK(file);

    if (!file->IsValid()) {
      return 0;
    }

    if (lines <= 0) {
      return 0;
    }

    int64_t length = file->GetLength();
    if (length == -1) {
      return -1;
    }

    if (length == 0) {
      return 0;
    }

    int line_count = 0;

    char chunk[kChunkSize];
    int64_t chunk_size = kChunkSize;
    int64_t last_chunk_size = 0;

    if (file->Seek(base::File::FROM_END, 0) == -1) {
      return -1;
    }

    do {
      if (chunk_size > length) {
        chunk_size = length;
      }

      int64_t seek_offset = chunk_size + last_chunk_size;
      if (file->Seek(base::File::FROM_CURRENT, -seek_offset) == -1) {
        return -1;
      }

      if (file->ReadAtCurrentPos(chunk, chunk_size) == -1) {
        return -1;
      }

      for (int i = chunk_size - 1; i >= 0; i--) {
        if (chunk[i] == '\n') {
          line_count++;
          if (line_count == lines + 1) {
            return length;
          }
        }

        length--;
      }

      last_chunk_size = chunk_size;
    } while (length > 0);

    return length;
  }

  bool TrimBeginningOfFile(base::File* file) {
    DCHECK(file);

    if (file->GetLength() == 0) {
      return true;
    }

    int64_t offset = SeekFromEnd(file, kLinesAfterTrim);
    if (offset == -1) {
      return false;
    }

    if (offset == 0) {
      return true;
    }

    if (file->Seek(base::File::FROM_BEGIN, offset) == -1) {
      return false;
    }

    int64_t size = file->GetLength() - offset;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size + 1);

    if (file->ReadAtCurrentPos(buffer.get(), size) == -1) {
      return false;
    }

    if (file->Seek(base::File::FROM_BEGIN, 0) == -1) {
      return false;
    }

    std::string data = std::string(buffer.get());
    int64_t new_size = data.size();

    if (file->WriteAtCurrentPos(data.c_str(), new_size) == -1) {
      return false;
    }

    if (!file->SetLength(new_size)) {
      return false;
    }

    return true;
  }

  bool MaybeTrimBeginningOfFile(base::File* file) {
    DCHECK(file);

    int64_t length = file->GetLength();
    if (length == -1) {
      return false;
    }

    // We do not trim the log on first run so that if the browser crashes and we
    // investigate the log with the user they are able to re-run the browser
    // without losing past logs.
    if (first_write_ || length <= kMaxFileSize) {
      return true;
    }

    return TrimBeginningOfFile(file);
  }

  std::string GetLogLevelName(int32_t level) {
    switch (level) {
      case 0:
        return "ERROR";
      case 1:
        return "INFO";
      default:
        return "VERBOSE" + base::NumberToString(level);
    }
  }

  std::string FormatTime(base::Time time) {
    return base::UTF16ToUTF8(
        base::TimeFormatWithPattern(time, "MMM dd, YYYY h::mm::ss.S a"));
  }

  std::string FormatMessage(const std::string& message,
                            const std::string& location,
                            int32_t line,
                            int32_t level) {
    std::ostringstream stream;
    stream << "[" << FormatTime(base::Time::Now()) << ":"
           << GetLogLevelName(level) << ":"
           << base::FilePath(location).BaseName().MaybeAsASCII() << "(" << line
           << ")] " << message << "\n";
    return stream.str();
  }

  base::FilePath file_path_;
  bool first_write_ = true;
};

}  // namespace

void CreateRewardsLoggerOnTaskRunner(
    const base::FilePath& file_path,
    mojo::PendingReceiver<mojom::RewardsLogger> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(Logger::CreateForReceiver, file_path,
                                       std::move(receiver)));
}

}  // namespace brave_rewards
