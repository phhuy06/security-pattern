/**
  ******************************************************************************
  * @file    pattern_storage.c
  * @brief   Lưu/đọc pattern mở khóa trong Flash nội.
  *
  * Vùng lưu: Flash sector 23 (128KB cuối của 2MB trên STM32F429ZI), base
  * 0x081E0000 — nằm xa vùng code nên an toàn để erase/ghi.
  *
  * Bản ghi (16 byte = 4 word):
  *   uint32_t magic;     // 0x50415454 ("PATT")
  *   uint8_t  len;       // số điểm 1..9
  *   uint8_t  dots[9];   // chỉ số điểm 0..8
  *   uint16_t crc;       // CRC16-CCITT của {len, dots[0..8]}
  ******************************************************************************
  */
#include "pattern_storage.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define STORAGE_SECTOR    FLASH_SECTOR_23
#define STORAGE_ADDR      0x081E0000U
#define STORAGE_MAGIC     0x50415454U   /* "PATT" */

typedef struct
{
    uint32_t magic;
    uint8_t  len;
    uint8_t  dots[PATTERN_MAX_LEN];
    uint16_t crc;
} PatternRecord;   /* 16 byte */

/* CRC16-CCITT (poly 0x1021, init 0xFFFF) trên buffer {len, dots[9]} = 10 byte. */
static uint16_t pattern_crc(uint8_t len, const uint8_t* dots)
{
    uint8_t buf[1 + PATTERN_MAX_LEN];
    uint16_t crc = 0xFFFFu;

    buf[0] = len;
    memcpy(&buf[1], dots, PATTERN_MAX_LEN);

    for (uint32_t i = 0; i < sizeof(buf); i++)
    {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x8000u)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

bool PatternStorage_Load(uint8_t* dots, uint8_t* len)
{
    const PatternRecord* rec = (const PatternRecord*)STORAGE_ADDR;

    if (rec->magic != STORAGE_MAGIC)
    {
        return false;
    }
    if (rec->len < 1u || rec->len > PATTERN_MAX_LEN)
    {
        return false;
    }
    if (pattern_crc(rec->len, rec->dots) != rec->crc)
    {
        return false;
    }

    *len = rec->len;
    memcpy(dots, rec->dots, PATTERN_MAX_LEN);
    return true;
}

bool PatternStorage_Save(const uint8_t* dots, uint8_t len)
{
    PatternRecord rec;
    uint32_t words[4];
    uint32_t sectorError = 0;
    FLASH_EraseInitTypeDef erase;
    bool ok = true;

    if (len < 1u || len > PATTERN_MAX_LEN)
    {
        return false;
    }

    /* Đóng gói bản ghi. */
    memset(&rec, 0, sizeof(rec));
    rec.magic = STORAGE_MAGIC;
    rec.len   = len;
    memcpy(rec.dots, dots, len);                 /* các byte còn lại = 0 */
    rec.crc   = pattern_crc(len, rec.dots);
    memcpy(words, &rec, sizeof(words));           /* 16 byte -> 4 word */

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR);

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Sector       = STORAGE_SECTOR;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* board chạy 3.3V */
    erase.Banks        = FLASH_BANK_2;            /* bỏ qua với sector erase, để rõ nghĩa */

    if (HAL_FLASHEx_Erase(&erase, &sectorError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return false;
    }

    for (uint32_t i = 0; i < 4u; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORAGE_ADDR + i * 4u, words[i]) != HAL_OK)
        {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    return ok;
}
