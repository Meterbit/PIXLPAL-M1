#include "USBFS.h"
#include <sys/stat.h>
#include <dirent.h>

#define USB_MOUNT_PATH "/usb"

/* ---------------- USBFile Class ---------------- */

USBFile::USBFile(FILE* fp, const char* name) : _fp(fp), _name(name) {}
USBFile::~USBFile() { close(); }

size_t USBFile::write(uint8_t c) {
    return _fp ? fwrite(&c, 1, 1, _fp) : 0;
}

size_t USBFile::write(const uint8_t *buf, size_t size) {
    return _fp ? fwrite(buf, 1, size, _fp) : 0;
}

int USBFile::read() {
    return _fp ? fgetc(_fp) : -1;
}

size_t USBFile::read(uint8_t *buf, size_t size) {
    return _fp ? fread(buf, 1, size, _fp) : 0;
}

int USBFile::peek() {
    if (!_fp) return -1;
    int c = fgetc(_fp);
    ungetc(c, _fp);
    return c;
}

int USBFile::available() {
    if (!_fp) return 0;
    long cur = ftell(_fp);
    fseek(_fp, 0, SEEK_END);
    long end = ftell(_fp);
    fseek(_fp, cur, SEEK_SET);
    return end - cur;
}

void USBFile::flush() {
    if (_fp) fflush(_fp);
}

void USBFile::close() {
    if (_fp) {
        fclose(_fp);
        _fp = nullptr;
    }
}

size_t USBFile::position() {
    return _fp ? ftell(_fp) : 0;
}

size_t USBFile::size() {
    if (!_fp) return 0;
    long cur = ftell(_fp);
    fseek(_fp, 0, SEEK_END);
    long size = ftell(_fp);
    fseek(_fp, cur, SEEK_SET);
    return size;
}

bool USBFile::seek(uint32_t pos, fs::SeekMode mode) {
    if (!_fp) return false;
    int whence = (mode == fs::SeekSet) ? SEEK_SET :
                 (mode == fs::SeekCur) ? SEEK_CUR : SEEK_END;
    return fseek(_fp, pos, whence) == 0;
}

bool USBFile::isOpen() const {
    return _fp != nullptr;
}

const char* USBFile::name() const {
    return _name.c_str();
}

/* ---------------- USBFS Class ---------------- */

bool USBFSClass::begin() {
    struct stat st;
    _mounted = (stat(USB_MOUNT_PATH, &st) == 0);
    return _mounted;
}

void USBFSClass::end() {
    _mounted = false;
}

bool USBFSClass::mounted() {
    return _mounted;
}

USBFile USBFSClass::open(const char* path, const char* mode) {
    if (!_mounted) return USBFile(nullptr, "");
    String fullPath = String(USB_MOUNT_PATH) + path;
    FILE* fp = fopen(fullPath.c_str(), mode);
    return USBFile(fp, fullPath.c_str());
}

bool USBFSClass::exists(const char* path) {
    struct stat st;
    String fullPath = String(USB_MOUNT_PATH) + path;
    return stat(fullPath.c_str(), &st) == 0;
}

bool USBFSClass::remove(const char* path) {
    String fullPath = String(USB_MOUNT_PATH) + path;
    return ::remove(fullPath.c_str()) == 0;
}

bool USBFSClass::rename(const char* from, const char* to) {
    String f1 = String(USB_MOUNT_PATH) + from;
    String f2 = String(USB_MOUNT_PATH) + to;
    return ::rename(f1.c_str(), f2.c_str()) == 0;
}

bool USBFSClass::mkdir(const char* path) {
    String p = String(USB_MOUNT_PATH) + path;
    return ::mkdir(p.c_str(), 0775) == 0;
}

bool USBFSClass::rmdir(const char* path) {
    String p = String(USB_MOUNT_PATH) + path;
    return ::rmdir(p.c_str()) == 0;
}

USBFSClass USBFS;
