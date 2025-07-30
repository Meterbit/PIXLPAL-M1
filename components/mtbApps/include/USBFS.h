#ifndef USBFS_H
#define USBFS_H

#include <Arduino.h>
#include <FS.h>


extern EventGroupHandle_t usb_event_group;
#define USB_MOUNTED_BIT BIT0

class USBFile : public Stream {
public:
    USBFile(FILE* fp, const char* name);
    ~USBFile();

    size_t write(uint8_t) override;
    size_t write(const uint8_t *buf, size_t size) override;
    int read() override;
    int peek() override;
    int available() override;
    void flush() override;
    void close();

    size_t read(uint8_t *buf, size_t size);
    size_t position();
    size_t size();
    bool seek(uint32_t pos, fs::SeekMode mode = fs::SeekSet);
    bool isOpen() const;
    const char* name() const;

    using Print::write;

private:
    FILE* _fp;
    String _name;
};

class USBFSClass {
public:
    bool begin();
    void end();
    bool mounted();

    USBFile open(const char* path, const char* mode = "r");
    bool exists(const char* path);
    bool remove(const char* path);
    bool rename(const char* from, const char* to);
    bool mkdir(const char* path);
    bool rmdir(const char* path);

private:
    bool _mounted = false;
};

extern USBFSClass USBFS;

#endif
