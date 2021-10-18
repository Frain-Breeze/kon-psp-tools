#include <teaio_file.hpp>

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include <logging.hpp>

bool Tea::FileMemory::open(uint8_t* data, size_t data_size, Endian endian) {
    this->close();
    if(!data)
        return false;

    _buffer = data;
    _size = data_size;
    _endian = endian;
    _owner = false;

    return true;
}

bool Tea::FileMemory::open_owned(uint8_t* data, size_t data_size, Endian endian) {
    this->open(data, data_size, endian);
    _owner = true;

    return true;
}

bool Tea::FileMemory::read(uint8_t* data, size_t size) {
    if(!_buffer)
        return false;

    if(_offset + size > _size) { return false; }
    memcpy(data, &_buffer[_offset], size);
    _offset += size;

    return true;
}

bool Tea::FileMemory::read_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_buffer)
        return false;

    if(_offset + size > _size) { return false; }

    memcpy(data, &_buffer[_offset], size);

    //swap to endian
    if(endian == Tea::Endian_current) { endian = _endian; }

    //assume we're on little endian (x86)
    if(endian == Tea::Endian_big) {
        for(int low = 0, high = size - 1; low < high; low++, high--) {
            uint8_t tmp = data[low];
            data[low] = data[high];
            data[high] = tmp;
        }
    }

    _offset += size;

    return true;
}

bool Tea::FileMemory::seek(int64_t pos, Tea::Seek mode) {
    if(!_buffer)
        return false;

    if(mode == Tea::Seek_current) {
        if(_offset + pos < 0) { return false; };
        if(_offset + pos > _size) {
            if(_owner) { _buffer = (uint8_t*)realloc((void*)_buffer, _offset + pos); _size = _offset + pos; }
            else { return false; }
        }
        _offset += pos;
    }
    else if(mode == Tea::Seek_end) { _offset = _size; }
    else if(mode == Tea::Seek_start) {
        if(pos < 0) { return false; };
        if(pos > _size) {
            if(_owner) { _buffer = (uint8_t*)realloc((void*)_buffer, pos); _size = pos; }
            else { return false; }
        }
        _offset = pos;
    }
    else { return false; }

    return true;
}

bool Tea::FileMemory::skip(int64_t length) {
    return this->seek(length, Tea::Seek_current);
}

bool Tea::FileMemory::write(uint8_t* data, size_t size) {
    LOGERR("WRITING NOT IMPLEMENTED (FileMemory)");
    //TODO: implement
    return false;
}

bool Tea::FileMemory::close() {
    LOGERR("NOT IMPLEMENTED (FileMemory)");
    //TODO: implement
    return false;
}

Tea::FileMemory::~FileMemory() {
    this->close();
}
