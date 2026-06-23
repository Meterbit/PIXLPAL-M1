/**
 * @file mtb_usb_fs.h
 * @brief Arduino `fs::FS`-compatible filesystem adapter for a USB MSC drive.
 *
 * Implements a POSIX-backed `fs::FS` drop-in that mounts the FAT filesystem
 * exposed by a USB Mass Storage Class device at `/usb`. Two implementation
 * classes live in the `usbfs_internal` namespace:
 *
 *   - **USBFileImpl** — implements `fs::FileImpl` using POSIX `FILE*` and
 *     `DIR*` for transparent file and directory access.
 *   - **USBFSImpl**   — implements `fs::FSImpl`; wraps `begin()`/`end()` mount
 *     management and delegates all file operations to `USBFileImpl` instances.
 *
 * The public API is exposed through the global `USBFS` object of type `USBFSFS`
 * (a thin `fs::FS` subclass that adds recursive mkdir/rmdir helpers).
 *
 * @note Include this header only once per translation unit. The `USBFS` global
 *       is defined in `mtb_usb_fs.cpp`.
 */
#ifndef USBFS_H
#define USBFS_H

#include <Arduino.h>
#include <FS.h>

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

extern EventGroupHandle_t usb_event_group; /**< FreeRTOS event group shared with the USB MSC host task; `USB_MOUNTED_BIT` is set when a drive is mounted. */
#define USB_MOUNTED_BIT BIT0 /**< EventGroup bit indicating a USB MSC drive is mounted (duplicate of mtb_usb_msc.h for self-contained inclusion). */

namespace usbfs_internal {

/**
 * @brief POSIX-backed implementation of `fs::FileImpl` for USB MSC files and directories.
 *
 * Wraps a `FILE*` (regular files) or `DIR*` (directories) opened on the USB
 * mount point. All `fs::FileImpl` pure virtual methods are implemented; two
 * optional extras (`truncate`, `getCreationTime`) are also provided.
 */
class USBFileImpl : public fs::FileImpl {
public:
    USBFileImpl() {}
    /**
     * @brief Construct a file or directory implementation wrapper.
     * @param fp     Open `FILE*` handle (nullptr for directories).
     * @param path   Absolute path of the file or directory on the USB volume.
     * @param isDir  true if this handle represents a directory.
     * @param dirp   Open `DIR*` handle (nullptr for regular files).
     */
    USBFileImpl(FILE* fp, const String& path, bool isDir, DIR* dirp)
    : _fp(fp), _path(path), _isDir(isDir), _dirp(dirp) {}

    ~USBFileImpl() override { close(); }

    /** @brief Write @p size bytes from @p buf to the file. @return Bytes written. */
    size_t write(const uint8_t* buf, size_t size) override;
    /** @brief Read up to @p size bytes from the file into @p buf. @return Bytes read. */
    size_t read(uint8_t* buf, size_t size) override;
    /** @brief Flush pending writes to the underlying FAT filesystem. */
    void   flush() override;

    /**
     * @brief Seek to a byte position in the file.
     * @param pos   Target position.
     * @param mode  SeekSet, SeekCur, or SeekEnd.
     * @return true on success.
     */
    bool   seek(uint32_t pos, fs::SeekMode mode) override;
    /** @brief Return the current read/write position in bytes. */
    size_t position() const override;
    /** @brief Return the file size in bytes. */
    size_t size() const override;

    /** @brief Set the internal read/write buffer size. @return true on success. */
    bool   setBufferSize(size_t size) override;

    /** @brief Close the file or directory handle and release resources. */
    void        close() override;
    /** @brief Return the file's last-modified timestamp as a POSIX `time_t`. */
    time_t      getLastWrite() override;
    const char* path() const override { return _path.c_str(); } /**< @return Absolute path string. */
    const char* name() const override { return _path.c_str(); } /**< @return File/dir name (same as path for this implementation). */
    boolean     isDirectory(void) override { return _isDir; }   /**< @return true if this is a directory handle. */

    /** @brief Open the next entry in a directory. @param mode Open mode string (e.g., "r"). */
    fs::FileImplPtr openNextFile(const char* mode) override;
    /** @brief Seek to a position in the directory entry list. */
    boolean         seekDir(long position) override;
    /** @brief Return the name of the next directory entry. */
    String          getNextFileName(void) override;
    /**
     * @brief Return the name of the next directory entry and whether it is a directory.
     * @param isDir  Set to true if the next entry is a sub-directory.
     */
    String          getNextFileName(bool *isDir) override;
    /** @brief Rewind the directory iterator to the first entry. */
    void            rewindDirectory(void) override;

    /** @return true if a valid file or directory handle is open. */
    operator bool() override { return (_fp != nullptr) || (_dirp != nullptr); }

    /** @brief Truncate the file to @p size bytes. @return true on success. */
    bool   truncate(uint32_t size);
    /** @brief Return the file's creation timestamp as a POSIX `time_t`. */
    time_t getCreationTime();

private:
    FILE*  _fp = nullptr;  /**< POSIX file pointer (regular files). */
    String _path;           /**< Absolute path of the entry. */
    bool   _isDir = false;  /**< true if this handle is for a directory. */
    DIR*   _dirp = nullptr; /**< POSIX directory pointer (directories). */
};

/**
 * @brief POSIX-backed implementation of `fs::FSImpl` for a USB MSC volume.
 *
 * Manages mount/unmount of the USB FAT volume and implements all `fs::FSImpl`
 * virtual methods by delegating to POSIX file I/O on the mount point path.
 */
class USBFSImpl : public fs::FSImpl {
public:
    USBFSImpl() {}
    ~USBFSImpl() override {}

    /**
     * @brief Mount the USB FAT filesystem at @p mountPath.
     * @param mountPath  VFS mount point (default "/usb").
     * @return true if mounted successfully.
     */
    bool begin(const char* mountPath = "/usb");

    /** @brief Unmount the USB FAT filesystem. */
    void end();

    /** @return true if the filesystem is currently mounted. */
    bool   mounted() const { return _mounted; }
    /** @return The current VFS mount path string. */
    const char* mountPath() const { return _mountPath.c_str(); }

    /**
     * @brief Open a file or directory.
     * @param path    Path relative to the mount point.
     * @param mode    POSIX mode string ("r", "w", "a", "r+", etc.).
     * @param create  true to create the file if it does not exist.
     */
    fs::FileImplPtr open(const char* path, const char* mode, const bool create) override;
    /** @brief Check whether a file or directory exists at @p path. */
    bool exists(const char* path) override;
    /** @brief Rename a file or directory. */
    bool rename(const char* pathFrom, const char* pathTo) override;
    /** @brief Remove a file. */
    bool remove(const char* path) override;
    /** @brief Create a directory (non-recursive). */
    bool mkdir(const char* path) override;
    /** @brief Remove a directory (must be empty). */
    bool rmdir(const char* path) override;

    /**
     * @brief Prepend the mount point to a relative path.
     * @return Full absolute path string.
     */
    String makeFullPath(const char* path) const;

    /** @brief Create a directory and all missing parent directories. */
    bool mkdirRecursive(const char* path);
    /** @brief Recursively remove a directory and all its contents. */
    bool rmdirRecursive(const char* path);

private:
    String _mountPath = "/usb"; /**< VFS mount point. */
    bool   _mounted = false;    /**< true when the FAT volume is mounted. */
};

} // namespace usbfs_internal

/**
 * @brief Arduino `fs::FS` drop-in for the USB MSC volume.
 *
 * Wraps `usbfs_internal::USBFSImpl` as a proper `fs::FS` subclass.
 * Use the global `USBFS` instance; call `USBFS.begin()` after a USB drive
 * is detected (i.e., after `USB_MOUNTED_BIT` is set) and `USBFS.end()`
 * when the drive is removed.
 */
class USBFSFS : public fs::FS {
public:
    USBFSFS();

    /**
     * @brief Mount the USB FAT volume.
     * @param mountPath  VFS mount point (default "/usb").
     * @return true on success.
     */
    bool begin(const char* mountPath = "/usb");

    /** @brief Unmount the USB FAT volume. */
    void end();

    /** @return true if the volume is currently mounted. */
    bool   mounted() const;
    /** @return The current VFS mount path. */
    const char* mountPath() const;

    /**
     * @brief Create a directory and all missing parents.
     * @param path       Target directory path.
     * @param recursive  true to create intermediate directories.
     */
    bool mkdir(const char* path, bool recursive);

    /**
     * @brief Remove a directory, optionally deleting all contents first.
     * @param path       Target directory path.
     * @param recursive  true to delete contents before removing the directory.
     */
    bool rmdir(const char* path, bool recursive);

private:
    usbfs_internal::USBFSImpl* impl() { return static_cast<usbfs_internal::USBFSImpl*>(_impl.get()); }
    const usbfs_internal::USBFSImpl* impl() const { return static_cast<const usbfs_internal::USBFSImpl*>(_impl.get()); }
};

extern USBFSFS USBFS; /**< Global USB filesystem instance. Call USBFS.begin() after USB_MOUNTED_BIT is set. */

#endif // USBFS_H
