#include "meta.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static const char META_MAGIC[8] = {'O', 'B', 'S', 'M', 'E', 'T', 'A', '1'};

void meta_init(obj_meta_t *m)
{
    memset(m, 0, sizeof(*m));
}

void meta_free(obj_meta_t *m)
{
    if (m == NULL)
        return;
    free(m->crc);
    m->crc = NULL;
}

uint64_t meta_stripe_bytes(const obj_meta_t *m, uint32_t s)
{
    uint64_t stripe_size = (uint64_t)m->shard_size * (uint64_t)m->k;
    uint64_t off = (uint64_t)s * stripe_size;
    uint64_t left = m->size > off ? m->size - off : 0;

    return left < stripe_size ? left : stripe_size;
}

int meta_encode(const obj_meta_t *m, uint8_t **buf, size_t *len)
{
    size_t ncrc = (size_t)m->num_stripes * (size_t)m->nshards;
    size_t total = META_HEADER_SIZE + ncrc * 4u + 4u;
    uint8_t *p;
    uint32_t crc;
    size_t i;

    p = (uint8_t *)calloc(total, 1);
    if (p == NULL)
        return -1;

    memcpy(p, META_MAGIC, 8);
    le_put32(p + 8, META_VERSION);
    le_put32(p + 12, (uint32_t)m->k);
    le_put32(p + 16, (uint32_t)m->m);
    le_put32(p + 20, (uint32_t)m->nshards);
    le_put64(p + 24, m->size);
    le_put32(p + 32, m->shard_size);
    le_put32(p + 36, m->num_stripes);
    le_put32(p + 40, m->etag);
    le_put32(p + 44, 0);
    le_put64(p + 48, m->created_at);

    for (i = 0; i < ncrc; i++)
        le_put32(p + META_HEADER_SIZE + i * 4u, m->crc[i]);

    crc = crc32_buf(p, total - 4u);
    le_put32(p + total - 4u, crc);

    *buf = p;
    *len = total;
    return 0;
}

int meta_decode(const uint8_t *buf, size_t len, obj_meta_t *m)
{
    size_t ncrc, want;
    uint32_t crc, version;
    int i;

    if (buf == NULL || m == NULL)
        return -1;
    memset(m, 0, sizeof(*m));

    if (len < META_HEADER_SIZE + 4u)
        return -1;
    if (memcmp(buf, META_MAGIC, 8) != 0)
        return -1;

    version = le_get32(buf + 8);
    if (version != META_VERSION)
        return -1;

    i = (int)le_get32(buf + 12);
    m->m = (int)le_get32(buf + 16);
    m->nshards = (int)le_get32(buf + 20);
    if (i < 1 || m->m < 1 || m->nshards != i + m->m)
        return -1;
    m->k = i;

    m->size = le_get64(buf + 24);
    m->shard_size = le_get32(buf + 32);
    m->num_stripes = le_get32(buf + 36);
    m->etag = le_get32(buf + 40);
    m->created_at = le_get64(buf + 48);

    if (m->shard_size == 0)
        return -1;

    ncrc = (size_t)m->num_stripes * (size_t)m->nshards;
    want = META_HEADER_SIZE + ncrc * 4u + 4u;
    if (len != want)
        return -1;

    crc = crc32_buf(buf, len - 4u);
    if (crc != le_get32(buf + len - 4u))
        return -1;

    /* geometry sanity check against the declared size */
    {
        uint64_t stripe_size = (uint64_t)m->shard_size * (uint64_t)m->k;
        uint64_t expect_stripes =
            m->size == 0 ? 0 : (m->size + stripe_size - 1) / stripe_size;
        if (expect_stripes != (uint64_t)m->num_stripes)
            return -1;
    }

    if (ncrc > 0) {
        size_t j;
        m->crc = (uint32_t *)malloc(ncrc * sizeof(uint32_t));
        if (m->crc == NULL)
            return -1;
        for (j = 0; j < ncrc; j++)
            m->crc[j] = le_get32(buf + META_HEADER_SIZE + j * 4u);
    }
    return 0;
}
