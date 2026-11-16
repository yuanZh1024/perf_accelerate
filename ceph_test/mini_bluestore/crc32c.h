// crc32c.h
#ifndef CRC32C_H
#define CRC32C_H
#include <stddef.h>
#include <stdint.h>
/**
 * 计算 CRC32C（Castagnoli）校验值，用于检测静默数据损坏。
 * @param buf  待校验数据的起始地址。
 * @param len  待校验数据的字节数。
 * @return     32 位校验值；标准向量 crc32c("123456789",9) = 0xE3069283。
 */
uint32_t crc32c(const void *buf, size_t len);
#endif
