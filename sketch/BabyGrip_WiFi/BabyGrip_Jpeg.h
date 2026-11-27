#pragma once
#include <stddef.h>
#include <stdint.h>

// Reader offsets are absolute. With a null buffer, skip and return the number
// of bytes available. Output is a 412 x 412, little-endian RGB565 display copy.
using BabyGripJpegReader = size_t (*)(void*, size_t, uint8_t*, size_t);
using BabyGripJpegService = void (*)();
bool BabyGripDecodeJpeg(size_t length, BabyGripJpegReader reader, void* source,
                        uint8_t* destination, size_t capacity,
                        BabyGripJpegService service = nullptr);
