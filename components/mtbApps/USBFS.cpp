#include "USBFS.h"
#include <unistd.h>   // fileno, ftruncate, truncate
#include <errno.h>

using namespace usbfs_internal;

// ------------- helpers -------------
static bool is_dot_or_dotdot(const char* n) {
    return (strcmp(n, ".") == 0) || (strcmp(n, "..") == 0);
}

// ------------- USBFileImpl -------------
size_t USBFileImpl::write(const uint8_t* buf, size_t size) {
    if (!_fp || !buf || size == 0) return 0;
    return fwrite(buf, 1, size, _fp);
}

size_t USBFileImpl::read(uint8_t* buf, size_t size) {
    if (!_fp || !buf || size == 0) return 0;
    return fread(buf, 1, size, _fp);
}

void USBFileImpl::flush() {
    if (_fp) fflush(_fp);
}

bool USBFileImpl::seek(uint32_t pos, fs::SeekMode mode) {
    if (!_fp) return false;
    int whence = SEEK_SET;
    switch (mode) {
        case fs::SeekSet: whence = SEEK_SET; break;
        case fs::SeekCur: whence = SEEK_CUR; break;
        case fs::SeekEnd: whence = SEEK_END; break;
        default: break;
    }
    return fseek(_fp, static_cast<long>(pos), whence) == 0;
}

size_t USBFileImpl::position() const {
    if (!_fp) return 0;
    long p = ftell(_fp);
    return (p < 0) ? 0 : static_cast<size_t>(p);
}

size_t USBFileImpl::size() const {
    struct stat st;
    if (stat(_path.c_str(), &st) == 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

bool USBFileImpl::setBufferSize(size_t size) {
    if (!_fp) return false;
    // Set fully buffered with requested size; let libc allocate the buffer.
    return setvbuf(_fp, nullptr, _IOFBF, size) == 0;
}

void USBFileImpl::close() {
    if (_fp) { fclose(_fp); _fp = nullptr; }
    if (_dirp) { closedir(_dirp); _dirp = nullptr; }
    _isDir = false;
    _path = String();
}

time_t USBFileImpl::getLastWrite() {
    struct stat st;
    if (stat(_path.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

bool USBFileImpl::truncate(uint32_t size) {
    if (_fp) {
        int fd = fileno(_fp);
        if (fd >= 0) {
            return ::ftruncate(fd, static_cast<off_t>(size)) == 0;
        }
    }
    if (_path.length()) {
        return ::truncate(_path.c_str(), static_cast<off_t>(size)) == 0;
    }
    return false;
}

time_t USBFileImpl::getCreationTime() {
#if defined(st_birthtime)
    struct stat st;
    if (stat(_path.c_str(), &st) == 0) {
        return st.st_birthtime;
    }
#endif
    // Fallback: some filesystems don't expose creation time; return last write.
    return getLastWrite();
}

fs::FileImplPtr USBFileImpl::openNextFile(const char* mode) {
    if (!_dirp) return fs::FileImplPtr();
    while (true) {
        errno = 0;
        dirent* de = readdir(_dirp);
        if (!de) return fs::FileImplPtr(); // end or error
        if (is_dot_or_dotdot(de->d_name)) continue;
        String childPath = _path;
        if (!childPath.endsWith("/")) childPath += "/";
        childPath += de->d_name;

        // Determine if entry is a directory
        bool isDir = false;
        struct stat st;
        if (stat(childPath.c_str(), &st) == 0) {
            isDir = S_ISDIR(st.st_mode);
        }
        if (isDir) {
            DIR* nd = opendir(childPath.c_str());
            return std::make_shared<USBFileImpl>(nullptr, childPath, true, nd);
        } else {
            FILE* fp = fopen(childPath.c_str(), mode ? mode : "r");
            if (!fp) return fs::FileImplPtr();
            return std::make_shared<USBFileImpl>(fp, childPath, false, nullptr);
        }
    }
}

boolean USBFileImpl::seekDir(long position) {
#if defined(seekdir)
    if (!_dirp) return false;
    seekdir(_dirp, position);
    return true;
#else
    (void)position;
    return false;
#endif
}

String USBFileImpl::getNextFileName(void) {
    bool isDir = false;
    return getNextFileName(&isDir);
}

String USBFileImpl::getNextFileName(bool *isDir) {
    if (!isDir) return String();
    if (!_dirp) return String();
    while (true) {
        errno = 0;
        dirent* de = readdir(_dirp);
        if (!de) return String();
        if (is_dot_or_dotdot(de->d_name)) continue;
        String name = String(de->d_name);
        String full = _path;
        if (!full.endsWith("/")) full += "/";
        full += name;
        struct stat st;
        *isDir = (stat(full.c_str(), &st) == 0) ? S_ISDIR(st.st_mode) : false;
        return name;
    }
}

void USBFileImpl::rewindDirectory() {
    if (_dirp) rewinddir(_dirp);
}

// ------------- USBFSImpl -------------

String USBFSImpl::makeFullPath(const char* path) const {
    String p = path ? String(path) : String();
    if (!p.startsWith("/")) p = "/" + p;
    return _mountPath + p;
}

bool USBFSImpl::begin(const char* mountPath) {
    if (mountPath && mountPath[0]) _mountPath = mountPath;
    struct stat st;
    _mounted = (stat(_mountPath.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
    if (usb_event_group) {
        if (_mounted) xEventGroupSetBits(usb_event_group, USB_MOUNTED_BIT);
        else          xEventGroupClearBits(usb_event_group, USB_MOUNTED_BIT);
    }
    // Inform base of mountpoint if it uses it
    mountpoint(_mountPath.c_str());
    return _mounted;
}

void USBFSImpl::end() {
    _mounted = false;
    if (usb_event_group) xEventGroupClearBits(usb_event_group, USB_MOUNTED_BIT);
}

fs::FileImplPtr USBFSImpl::open(const char* path, const char* mode, const bool create) {
    if (!_mounted) return fs::FileImplPtr();

    String full = makeFullPath(path);
    struct stat st;

    if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        DIR* d = opendir(full.c_str());
        if (!d) return fs::FileImplPtr();
        return std::make_shared<USBFileImpl>(nullptr, full, true, d);
    }

    // Map Arduino modes to fopen and handle create hint
    const char* m = mode ? mode : "r";
    String mstr(m);
    if (create && mstr == "r") mstr = "w+";

    // Ensure parent directories if creating
    if (create && (mstr.indexOf('w') >= 0 || mstr.indexOf('+') >= 0)) {
        String parent = full;
        int slash = parent.lastIndexOf('/');
        if (slash > 0) {
            parent = parent.substring(0, slash);
            String acc;
            for (size_t i = 1; i < parent.length(); ++i) {
                char c = parent[i];
                acc += c;
                if (c == '/') {
                    ::mkdir((_mountPath + acc).c_str(), 0775);
                }
            }
            ::mkdir(parent.c_str(), 0775);
        }
    }

    FILE* fp = fopen(full.c_str(), mstr.c_str());
    if (!fp) return fs::FileImplPtr();
    return std::make_shared<USBFileImpl>(fp, full, false, nullptr);
}

bool USBFSImpl::exists(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    struct stat st;
    return stat(full.c_str(), &st) == 0;
}

bool USBFSImpl::remove(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    return ::remove(full.c_str()) == 0;
}

bool USBFSImpl::rename(const char* pathFrom, const char* pathTo) {
    if (!_mounted) return false;
    String f1 = makeFullPath(pathFrom);
    String f2 = makeFullPath(pathTo);
    return ::rename(f1.c_str(), f2.c_str()) == 0;
}

bool USBFSImpl::mkdir(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    return ::mkdir(full.c_str(), 0775) == 0 || errno == EEXIST;
}

bool USBFSImpl::rmdir(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    return ::rmdir(full.c_str()) == 0;
}

bool USBFSImpl::mkdirRecursive(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    if (full.length() == 0) return false;
    String tmp;
    for (size_t i = 1; i < full.length(); ++i) {
        char c = full[i];
        tmp += c;
        if (c == '/') {
            ::mkdir((_mountPath + tmp).c_str(), 0775);
        }
    }
    return ::mkdir(full.c_str(), 0775) == 0 || errno == EEXIST;
}

bool USBFSImpl::rmdirRecursive(const char* path) {
    if (!_mounted) return false;
    String full = makeFullPath(path);
    DIR* d = opendir(full.c_str());
    if (!d) return false;
    struct dirent* de;
    bool ok = true;
    while ((de = readdir(d)) != nullptr) {
        if (is_dot_or_dotdot(de->d_name)) continue;
        String child = full;
        if (!child.endsWith("/")) child += "/";
        child += de->d_name;
        struct stat st;
        if (stat(child.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (!rmdirRecursive(child.c_str())) ok = false;
            } else {
                if (::remove(child.c_str()) != 0) ok = false;
            }
        }
    }
    closedir(d);
    if (::rmdir(full.c_str()) != 0) ok = false;
    return ok;
}

// ------------- USBFSFS (public wrapper) -------------

USBFSFS::USBFSFS() : fs::FS(std::make_shared<USBFSImpl>()) {}

bool USBFSFS::begin(const char* mountPath) { return impl()->begin(mountPath); }
void USBFSFS::end() { impl()->end(); }
bool USBFSFS::mounted() const { return impl()->mounted(); }
const char* USBFSFS::mountPath() const { return impl()->mountPath(); }
bool USBFSFS::mkdir(const char* path, bool recursive) { return recursive ? impl()->mkdirRecursive(path) : impl()->mkdir(path); }
bool USBFSFS::rmdir(const char* path, bool recursive) { return recursive ? impl()->rmdirRecursive(path) : impl()->rmdir(path); }

// Global instance
USBFSFS USBFS;
