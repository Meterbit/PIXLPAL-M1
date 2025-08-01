#ifndef USBFS_H
#define USBFS_H

#include <Arduino.h>
#include <FS.h>

// Use the user's FSImpl.h (ESP8266-style interface)
#if __has_include(<FSImpl.h>)
  #include <FSImpl.h>
#else
  #error "FSImpl.h not found. Please include the same FSImpl.h used by your core."
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <memory>
#include <cstdio>
#include <cstring>

// --- Optional: Event group bit for your app ---
#if __has_include(<freertos/FreeRTOS.h>)
  #include <freertos/FreeRTOS.h>
  #include <freertos/event_groups.h>
#else
  struct _EventGroup;
  typedef _EventGroup* EventGroupHandle_t;
#endif

#ifndef BIT0
  #define BIT0 0x00000001u
#endif

extern EventGroupHandle_t usb_event_group;
#define USB_MOUNTED_BIT BIT0

namespace usbfs_internal {

// ---- File implementation (fs::FileImpl) ----
class USBFileImpl : public fs::FileImpl {
public:
    USBFileImpl() {}
    USBFileImpl(FILE* fp, const String& path, bool isDir, DIR* dirp)
    : _fp(fp), _path(path), _isDir(isDir), _dirp(dirp) {}

    ~USBFileImpl() override { close(); }

    // Stream / Print
    size_t write(const uint8_t* buf, size_t size) override;
    size_t read(uint8_t* buf, size_t size) override;
    void   flush() override;

    // Positioning
    bool   seek(uint32_t pos, fs::SeekMode mode) override;
    size_t position() const override;
    size_t size() const override;

    // Buffers
    bool   setBufferSize(size_t size) override;

    // Lifecycle / info
    void        close() override;
    time_t      getLastWrite() override;
    const char* path() const override { return _path.c_str(); }
    const char* name() const override { return _path.c_str(); }
    boolean     isDirectory(void) override { return _isDir; }

    // Directory APIs
    fs::FileImplPtr openNextFile(const char* mode) override;
    boolean         seekDir(long position) override;
    String          getNextFileName(void) override;
    String          getNextFileName(bool *isDir) override;
    void            rewindDirectory(void) override;

    // Conversion
    operator bool() override { return (_fp != nullptr) || (_dirp != nullptr); }

    // Optional extras (not pure-virtual in your header)
    bool   truncate(uint32_t size);
    time_t getCreationTime();

private:
    FILE*  _fp = nullptr;
    String _path;
    bool   _isDir = false;
    DIR*   _dirp = nullptr;
};

// ---- FS implementation (fs::FSImpl) ----
class USBFSImpl : public fs::FSImpl {
public:
    USBFSImpl() {}
    ~USBFSImpl() override {}

    // Mount mgmt (helper APIs)
    bool begin(const char* mountPath = "/usb");
    void end();
    bool   mounted() const { return _mounted; }
    const char* mountPath() const { return _mountPath.c_str(); }

    // FSImpl API (matches user's FSImpl.h exactly)
    fs::FileImplPtr open(const char* path, const char* mode, const bool create) override;
    bool exists(const char* path) override;
    bool rename(const char* pathFrom, const char* pathTo) override;
    bool remove(const char* path) override;
    bool mkdir(const char* path) override;
    bool rmdir(const char* path) override;

    // Helpers
    String makeFullPath(const char* path) const;

    // Extras
    bool mkdirRecursive(const char* path);
    bool rmdirRecursive(const char* path);

private:
    String _mountPath = "/usb";
    bool   _mounted = false;
};

} // namespace usbfs_internal

// ---- Public FS wrapper: true fs::FS drop-in ----
class USBFSFS : public fs::FS {
public:
    USBFSFS();

    // Helpers
    bool begin(const char* mountPath = "/usb");
    void end();
    bool   mounted() const;
    const char* mountPath() const;

    bool mkdir(const char* path, bool recursive);
    bool rmdir(const char* path, bool recursive);

private:
    usbfs_internal::USBFSImpl* impl() { return static_cast<usbfs_internal::USBFSImpl*>(_impl.get()); }
    const usbfs_internal::USBFSImpl* impl() const { return static_cast<const usbfs_internal::USBFSImpl*>(_impl.get()); }
};

extern USBFSFS USBFS;

#endif // USBFS_H
