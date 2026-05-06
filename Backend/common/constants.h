#ifndef CONSTANTS_H
#define CONSTANTS_H

namespace Constants {
    const int SERVER_PORT = 8888;
    const int CHUNK_SIZE = 64 * 1024; // 64KB chunks
    const int MAX_CONCURRENT_REQUESTS = 10;
}

#endif // CONSTANTS_H