#ifndef U_INT_H_
#define U_INT_H_

#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;
static_assert(sizeof(f32) == 4, "Sizeof float32 must be 4 bytes (32 bits)");
static_assert(sizeof(f64) == 8, "Sizeof float64 must be 8 bytes (64 bits)");

static_assert(sizeof(char) == 1, "Sizeof char must be 1 byte (8 bits)");
static_assert(sizeof(void *) == 8, "Sizeof void * must be 8 bytes (64 bits)");

#endif /* U_INT_H_ */
