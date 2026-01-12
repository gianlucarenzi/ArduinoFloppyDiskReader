# removed; merged into waffleCopyPRO-Universal.pro
# Include this .pri in waffleCopyPRO-Universal.pro to optionally build capsimg sources
# Usage:
#  - To compile capsimg into the binary: add "CONFIG += capsimg_static" to your .pro
#    This will compile the selected capsimg sources and define CAPS_USER so the
#    local CapsPlug will call the library functions directly instead of dlopen.
#  - Alternatively link against an external libcapsimage using -lcapsimg and -L/path

contains(CONFIG, capsimg_static) {
    message("Including capsimg sources in build (capsimg_static)")
    DEFINES += CAPS_USER
    INCLUDEPATH += $$PWD/lib/capsimg
    INCLUDEPATH += $$PWD/lib/capsimg/Core
    INCLUDEPATH += $$PWD/lib/capsimg/LibIPF
    INCLUDEPATH += $$PWD/lib/capsimg/Codec
    INCLUDEPATH += $$PWD/lib/capsimg/Device
    INCLUDEPATH += $$PWD/lib/capsimg/CAPSImg
    SOURCES += \
        lib/capsimg/CAPSImg/CAPSImg.cpp \
        lib/capsimg/CAPSImg/StreamImage.cpp \
        lib/capsimg/CAPSImg/CapsFormatMFM.cpp \
        lib/capsimg/CAPSImg/DiskImageFactory.cpp \
        lib/capsimg/CAPSImg/DiskImage.cpp \
        lib/capsimg/CAPSImg/CapsImage.cpp \
        lib/capsimg/CAPSImg/CapsFile.cpp \
        lib/capsimg/CAPSImg/CapsImageStd.cpp \
        lib/capsimg/CAPSImg/CapsAPI.cpp \
        lib/capsimg/CAPSImg/CapsLoader.cpp \
        lib/capsimg/CAPSImg/StreamCueImage.cpp \
        lib/capsimg/Codec/CTRawCodec.cpp \
        lib/capsimg/Codec/CTRawCodecDecompressor.cpp \
        lib/capsimg/Codec/DiskEncoding.cpp \
        lib/capsimg/Core/CRC.cpp \
        lib/capsimg/Core/DiskFile.cpp \
        lib/capsimg/Core/BitBuffer.cpp \
        lib/capsimg/Core/BaseFile.cpp \
        lib/capsimg/Core/MemoryFile.cpp \
        lib/capsimg/CAPSImg/stdafx.cpp
}
else {
    # Not building sources: link against the compiled capsimg in lib/capsimg (relative to this project)
    # Makefile in lib/capsimg copies the built library into this directory as capsimg.so (Unix/macOS) or capsimg.dll (Windows)
    unix:!macx { LIBS += $$PWD/lib/capsimg/capsimg.so }
    macx { LIBS += $$PWD/lib/capsimg/capsimg.so }
    win32 { LIBS += $$PWD/lib/capsimg/capsimg.dll }

    # Header include for linking against the library
    INCLUDEPATH += $$PWD/lib/capsimg/LibIPF
}
