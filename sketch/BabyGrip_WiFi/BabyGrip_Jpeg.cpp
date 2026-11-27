#include "BabyGrip_Jpeg.h"
#include <esp_jpg_decode.h>

namespace {
constexpr size_t WIDTH = 412, HEIGHT = 412;
constexpr size_t FRAME_BYTES = WIDTH * HEIGHT * 2;
constexpr size_t MAX_JPEG_BYTES = 196608;
struct DecodeContext {
  size_t length;
  BabyGripJpegReader reader;
  void* source;
  uint8_t* destination;
  BabyGripJpegService service;
  size_t pixelsWritten = 0;
  unsigned blocks = 0;
  bool dimensionsValid = false;
  bool failed = false;
};
size_t readJpeg(void* opaque, size_t offset, uint8_t* bytes, size_t count) {
  auto& context = *static_cast<DecodeContext*>(opaque);
  if (offset > context.length) return 0;
  if (count > context.length - offset) count = context.length - offset;
  return context.reader(context.source, offset, bytes, count);
}
bool writeJpeg(void* opaque, uint16_t x, uint16_t y, uint16_t width,
               uint16_t height, uint8_t* source) {
  auto& context = *static_cast<DecodeContext*>(opaque);
  if (!source) {
    // esp_jpg_decode ignores this initial callback's return value. Persist the
    // validation and check it again for every actual MCU block and at the end.
    if (x == 0 && y == 0) {
      context.dimensionsValid = width == WIDTH && height == HEIGHT;
      if (!context.dimensionsValid) context.failed = true;
    }
    return !context.failed;
  }
  if (context.failed || !context.dimensionsValid || !width || !height ||
      x >= WIDTH || y >= HEIGHT || width > WIDTH - x || height > HEIGHT - y) {
    context.failed = true;
    return false;
  }
  for (size_t row = 0; row < height; ++row) {
    uint8_t* target = context.destination + ((y + row) * WIDTH + x) * 2;
    const uint8_t* pixel = source + row * width * 3;
    for (size_t col = 0; col < width; ++col, pixel += 3, target += 2) {
      const uint16_t color = ((pixel[0] >> 3) << 11) |
                             ((pixel[1] >> 2) << 5) | (pixel[2] >> 3);
      target[0] = color & 0xff;
      target[1] = color >> 8;
    }
  }
  context.pixelsWritten += (size_t)width * height;
  if (context.pixelsWritten > WIDTH * HEIGHT) {
    context.failed = true;
    return false;
  }
  // Decode and audio controls both run on the Arduino loop task. Never call
  // WebServer/LVGL here, because the output buffer is still being prepared.
  if (context.service && ++context.blocks % 16 == 0) context.service();
  return true;
}
}  // namespace

bool BabyGripDecodeJpeg(size_t length, BabyGripJpegReader reader, void* source,
                        uint8_t* destination, size_t capacity,
                        BabyGripJpegService service) {
  if (!reader || !destination || capacity < FRAME_BYTES ||
      length < 128 || length > MAX_JPEG_BYTES) return false;
  uint8_t start[2], end[2];
  if (reader(source, 0, start, 2) != 2 ||
      reader(source, length - 2, end, 2) != 2 ||
      start[0] != 0xff || start[1] != 0xd8 ||
      end[0] != 0xff || end[1] != 0xd9) return false;
  DecodeContext context{length, reader, source, destination, service};
  // The ESP32 core's camera conversion library uses the S3 ROM TJpgDec. It has
  // a static work area: this helper is intentionally called only on loop task.
  const esp_err_t result = esp_jpg_decode(length, JPG_SCALE_NONE, readJpeg,
                                         writeJpeg, &context);
  return result == ESP_OK && context.dimensionsValid && !context.failed &&
         context.pixelsWritten == WIDTH * HEIGHT;
}
