// file: firmware/src/jpeg_response.h
// purpose: async web server response classes for jpeg camera frames
// dependencies: ESPAsyncWebServer.h, esp_camera.h

#pragma once

#include <ESPAsyncWebServer.h>
#include <esp_camera.h>

class AsyncJpegCopyResponse : public AsyncAbstractResponse {
private:
    uint8_t* _buf;
    size_t _len;
    size_t _index;
public:
    AsyncJpegCopyResponse(const camera_fb_t* fb) : _buf(nullptr), _len(0), _index(0) {
        _code = 200;
        _contentType = "image/jpeg";
        if (fb && fb->len > 0) {
            _buf = static_cast<uint8_t*>(malloc(fb->len));
            if (!_buf) {
                _buf = static_cast<uint8_t*>(ps_malloc(fb->len));
            }
            if (_buf) {
                memcpy(_buf, fb->buf, fb->len);
                _len = fb->len;
                _contentLength = _len;
            } else {
                _code = 503;
            }
        } else {
            _code = 503;
        }
    }
    ~AsyncJpegCopyResponse() {
        if (_buf) free(_buf);
    }
    bool _sourceValid() const override {
        return _buf != nullptr;
    }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override {
        if (!_buf || _index >= _len) return 0;
        size_t ret = _len - _index;
        if (ret > maxLen) ret = maxLen;
        memcpy(buf, _buf + _index, ret);
        _index += ret;
        return ret;
    }
};

static bool isValidJpegFrame(const camera_fb_t* frame_buffer) {
    if (frame_buffer == nullptr || frame_buffer->buf == nullptr || frame_buffer->len < 4) {
        return false;
    }

    const uint8_t jpeg_start_0 = 0xFF;
    const uint8_t jpeg_start_1 = 0xD8;
    const uint8_t jpeg_end_0 = 0xFF;
    const uint8_t jpeg_end_1 = 0xD9;

    return frame_buffer->buf[0] == jpeg_start_0
        && frame_buffer->buf[1] == jpeg_start_1
        && frame_buffer->buf[frame_buffer->len - 2] == jpeg_end_0
        && frame_buffer->buf[frame_buffer->len - 1] == jpeg_end_1;
}

class HeapJpegResponse : public AsyncAbstractResponse {
private:
    uint8_t* _buf;
    size_t _len;
    size_t _index;
public:
    HeapJpegResponse(uint8_t* buf, size_t len) : _buf(buf), _len(len), _index(0) {
        _code = 200;
        _contentType = "image/jpeg";
        _contentLength = len;
    }
    ~HeapJpegResponse() {
        if (_buf) free(_buf);
    }
    bool _sourceValid() const override {
        return _buf != nullptr;
    }
    size_t _fillBuffer(uint8_t* data, size_t maxLen) override {
        if (!_buf || _index >= _len) return 0;
        size_t ret = _len - _index;
        if (ret > maxLen) ret = maxLen;
        memcpy(data, _buf + _index, ret);
        _index += ret;
        return ret;
    }
};
