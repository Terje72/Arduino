#ifndef ARD_DEBUG_H
#define ARD_DEBUG_H

#include <stddef.h>
#include <stdint.h>

#ifdef DEBUG_ESP_CORE
#define DEBUGV(fmt, ...) ::printf((PGM_P)PSTR(fmt), ## __VA_ARGS__)
#endif

#ifndef DEBUGV
#define DEBUGV(...) do { (void)0; } while (0)
#endif

#ifdef __cplusplus
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
#else
void hexdump(const void *mem, uint32_t len, uint8_t cols);
#endif

#ifdef __cplusplus
extern "C" {
#endif

void __panic_func(const char* file, int line, const char* func) __attribute__((noreturn));
#define panic() __panic_func(PSTR(__FILE__), __LINE__, __func__)

#ifdef DEBUG_ESP_CORE
#define IAMSLOW(str) \
    do { \
        static bool once = false;\
        if (!once) { \
            once = true; \
            DEBUGV((PGM_P)PSTR(str " should be overridden for better efficiency\r\n")); \
        } \
    } while (0)
#else
#define IAMSLOW(str) do { (void)0; } while (0)
#endif

#ifdef __cplusplus
}
#endif


#endif//ARD_DEBUG_H
