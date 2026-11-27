#pragma once
// Read-only file system over byte arrays compiled into the firmware. The two
// built-in sounds play from here, so they no longer need a second copy in the
// photo storage.
#include <FS.h>
#include <FSImpl.h>
#include <memory>
#include <string.h>

namespace bgrom {

struct Entry {
  const char* path;
  const uint8_t* data;
  size_t size;
};

class RomFile final : public fs::FileImpl {
 public:
  explicit RomFile(const Entry& entry) : entry_(entry) {
    const char* slash = strrchr(entry.path, '/');
    name_ = slash ? slash + 1 : entry.path;
  }
  size_t write(const uint8_t*, size_t) override { return 0; }
  size_t read(uint8_t* buf, size_t size) override {
    if (!open_ || !buf || pos_ >= entry_.size) return 0;
    const size_t left = entry_.size - pos_;
    const size_t count = size < left ? size : left;
    memcpy(buf, entry_.data + pos_, count);
    pos_ += count;
    return count;
  }
  void flush() override {}
  bool seek(uint32_t pos, fs::SeekMode mode) override {
    int64_t target = (int64_t)pos;
    if (mode == fs::SeekCur) target = (int64_t)pos_ + (int32_t)pos;
    else if (mode == fs::SeekEnd) target = (int64_t)entry_.size + (int32_t)pos;
    if (!open_ || target < 0 || target > (int64_t)entry_.size) return false;
    pos_ = (size_t)target;
    return true;
  }
  size_t position() const override { return pos_; }
  size_t size() const override { return entry_.size; }
  bool setBufferSize(size_t) override { return true; }
  void close() override { open_ = false; }
  time_t getLastWrite() override { return 0; }
  const char* path() const override { return entry_.path; }
  const char* name() const override { return name_; }
  boolean isDirectory(void) override { return false; }
  fs::FileImplPtr openNextFile(const char*) override { return fs::FileImplPtr(); }
  boolean seekDir(long) override { return false; }
  String getNextFileName(void) override { return String(); }
  String getNextFileName(bool* isDir) override {
    if (isDir) *isDir = false;
    return String();
  }
  void rewindDirectory(void) override {}
  operator bool() override { return open_; }

 private:
  Entry entry_;
  const char* name_;
  size_t pos_ = 0;
  bool open_ = true;
};

class RomFSImpl final : public fs::FSImpl {
 public:
  RomFSImpl(const Entry* entries, size_t count) : entries_(entries), count_(count) {}
  fs::FileImplPtr open(const char* path, const char* mode, const bool create) override {
    const Entry* entry = find(path);
    if (!entry || create || (mode && mode[0] != 'r')) return fs::FileImplPtr();
    return std::make_shared<RomFile>(*entry);
  }
  bool exists(const char* path) override { return find(path) != nullptr; }
  bool rename(const char*, const char*) override { return false; }
  bool remove(const char*) override { return false; }
  bool mkdir(const char*) override { return false; }
  bool rmdir(const char*) override { return false; }

 private:
  const Entry* find(const char* path) const {
    if (!path) return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(entries_[i].path, path) == 0) return &entries_[i];
    return nullptr;
  }
  const Entry* entries_;
  size_t count_;
};

}  // namespace bgrom
