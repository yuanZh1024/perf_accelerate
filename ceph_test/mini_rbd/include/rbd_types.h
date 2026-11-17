#ifndef MINI_RBD_TYPES_H
#define MINI_RBD_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*
 * RBD Format 2 naming conventions
 */
#define RBD_ID_PREFIX       "rbd_id."
#define RBD_HEADER_PREFIX   "rbd_header."
#define RBD_DATA_PREFIX     "rbd_data."

/*
 * Default object order:
 * object_size = 1ULL << order
 *
 * order = 22 => 4 MiB
 */
#define RBD_DEFAULT_ORDER   22

#define RBD_MAX_IMAGE_NAME  128
#define RBD_MAX_IMAGE_ID    64
#define RBD_MAX_OBJECT_NAME 256

typedef struct {
    char name[RBD_MAX_IMAGE_NAME];

    char id[RBD_MAX_IMAGE_ID];

    char object_prefix[RBD_MAX_OBJECT_NAME];

    uint64_t size;

    uint64_t features;

    uint8_t order;

    uint64_t object_size;

    uint64_t stripe_unit;

    uint64_t stripe_count;

} rbd_image_t;

/*
 * Logical offset -> RADOS Object
 */
typedef struct {
    uint64_t object_no;

    uint64_t object_offset;

    uint64_t length;

} rbd_object_extent_t;

static inline uint64_t rbd_get_object_size(uint8_t order)
{
    return 1ULL << order;
}

#endif
