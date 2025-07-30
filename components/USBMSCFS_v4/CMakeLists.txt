idf_component_register(SRCS "USBMSCFS.cpp"
                      INCLUDE_DIRS "."
                      REQUIRES usb vfs fatfs usb_host_msc)
