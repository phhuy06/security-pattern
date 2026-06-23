/**
  ******************************************************************************
  * @file    pattern_storage.h
  * @brief   Lưu/đọc pattern mở khóa trong Flash nội (sector cuối), tồn tại
  *          qua các lần khởi động lại thiết bị.
  ******************************************************************************
  */
#ifndef PATTERN_STORAGE_H
#define PATTERN_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Số điểm tối đa của một pattern (lưới 3x3). */
#define PATTERN_MAX_LEN  9u

/**
  * @brief  Đọc pattern đã lưu từ Flash.
  * @param  dots  Mảng nhận các chỉ số điểm (0..8), kích thước >= PATTERN_MAX_LEN.
  * @param  len   Nhận số điểm của pattern.
  * @retval true nếu Flash chứa pattern hợp lệ (magic + CRC đúng), false nếu chưa có.
  */
bool PatternStorage_Load(uint8_t* dots, uint8_t* len);

/**
  * @brief  Xóa sector lưu trữ và ghi pattern mới (vĩnh viễn).
  * @note   Erase sector 128KB làm CPU stall ~1s -> chỉ gọi khi đăng ký.
  * @param  dots  Mảng chỉ số điểm (0..8).
  * @param  len   Số điểm (1..PATTERN_MAX_LEN).
  * @retval true nếu ghi thành công.
  */
bool PatternStorage_Save(const uint8_t* dots, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* PATTERN_STORAGE_H */
