#include <FS.h>
#include <FSImpl.h>
#include "ff15/ff.h"

using namespace fs;

class FatFsImpl : public FSImpl {
    FatFsImpl(uint8_t *start, uint32_t size, uint32_t pageSize, uint32_t blockSize, uint32_t maxOpenFds)
        : _start(start), _size(size), _pageSize(pageSize), _blockSize(blockSize), _maxOpenFds(maxOpenFds),
          _mounted(false) {
    }

    bool begin() override {
        return false;
    }
protected:
    uint8_t *_start;
    uint32_t _size;
    uint32_t _pageSize;
    uint32_t _blockSize;
    uint32_t _maxOpenFds;

    bool     _mounted;
};