# Pattern Unlock — Design Spec

**Board:** STM32F429I-DISCO (STM32F429ZIT6, 2MB Flash, 240×320 16bpp LCD, STMPE811 touch)
**Framework:** TouchGFX 4.26.1 (MVP) + FreeRTOS (CMSIS-RTOS v2)
**Date:** 2026-06-23

## 1. Mục tiêu

Khóa mở bằng pattern lưới 3×3 giống điện thoại, với 3 chức năng: **đăng ký**, **kiểm tra**, **lưu trữ vĩnh viễn** trong Flash.

- Trạng thái bình thường: thiết bị **khóa**, hiện 9 điểm để nhập pattern mở khóa.
- Giữ nút **BOOT (USER_BUTTON = PA0)** 3 giây → vào chế độ **đăng ký**: vẽ pattern mới → vẽ lại để xác nhận → lưu Flash.
- Mở khóa đúng → hiện thông báo + đổi màu xanh → tự quay về khóa sau ~3s.
- Mở khóa sai → báo lỗi (đỏ) → tự reset về khóa.
- Lần đầu chưa có pattern trong Flash → màn khóa nhắc "giữ BOOT 3s để đăng ký", chặn mở khóa.

## 2. Quyết định đã chốt

| Vấn đề | Lựa chọn |
|---|---|
| Lần đầu chưa có pattern | Hiện nhắc đăng ký, chặn unlock cho tới khi đăng ký |
| Quy tắc pattern | Giống điện thoại: kéo qua điểm, mỗi điểm 1 lần, **tự chèn điểm trung gian**, tối thiểu 4 điểm |
| Nhập sai | Báo lỗi + cho thử lại không giới hạn |
| Phản hồi UI | **Màu sắc là chính** (font hiện chỉ có glyph `?`); text qua wildcard sẽ hiện sau khi regenerate Designer |

## 3. Kiến trúc (hybrid C + C++ TouchGFX)

```
pattern_storage.c/.h (Core, C thuần)   → đọc/ghi/xóa Flash sector 23 (0x081E0000)
        ▲ extern "C"
Model (C++)   → nạp pattern lúc khởi động; poll PA0 mỗi tick → event đăng ký; verify/save
        ▲ ModelListener::notifyRegisterRequested()
Screen1Presenter → cầu nối Model ↔ View
        ▲
Screen1View   → máy trạng thái + touch + hit-test 9 điểm + vẽ 8 line + timer + màu/thông báo
```

## 4. Module Flash — `pattern_storage` (C)

- Vùng lưu: **Flash sector 23**, base `0x081E0000` (128KB cuối của 2MB, xa vùng code).
- Layout ghi: `{ uint32_t magic=0x50415454; uint8_t len; uint8_t dots[9]; uint16_t crc16; }` (ghi theo word 32-bit).
- API:
  - `bool PatternStorage_Load(uint8_t* dots, uint8_t* len)` → `true` nếu magic + crc hợp lệ.
  - `bool PatternStorage_Save(const uint8_t* dots, uint8_t len)` → `HAL_FLASH_Unlock` → erase sector 23 → program → `Lock`.
- CRC16-CCITT đơn giản trên `len + dots[len]`.
- **Lưu ý:** erase sector 128KB làm CPU stall ~1s → GUI "đứng" một nhịp khi lưu (chỉ lúc đăng ký). Hiện "Dang luu..." trước khi ghi.

## 5. Model (C++)

- Thành viên: `uint8_t storedDots[9]; uint8_t storedLen; bool patternSet;`
- Constructor / init: gọi `PatternStorage_Load` (chỉ đọc bộ nhớ, an toàn).
- `tick()`: đọc `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` (active-high). Dùng `HAL_GetTick()` đo thời gian giữ; ≥3000ms và chưa bắn → `modelListener->notifyRegisterRequested()` một lần (chặn lặp tới khi nhả nút).
- `bool isPatternSet()`, `bool verifyPattern(dots,len)`, `bool savePattern(dots,len)` (gọi storage + cập nhật RAM).

## 6. ModelListener / Presenter

- `ModelListener`: thêm `virtual void notifyRegisterRequested() {}`.
- `Screen1Presenter`: override `notifyRegisterRequested()` → `view.startRegistration()`.

## 7. Screen1View — máy trạng thái

```
enum AppState { ST_LOCKED, ST_REGISTER_FIRST, ST_REGISTER_CONFIRM, ST_UNLOCKED };
```

**Hình học 9 điểm (tâm):** cx = {50,120,190,50,120,190,50,120,190}, cy = {90,90,90,160,160,160,230,230,230}.
Ánh xạ index 0..8 → circle1..circle9 (đã kiểm khớp toạ độ generated). lines[0..7] → line1..line8.

**Bảng điểm trung gian** `middleDot(a,b)`: (0,2)→1 (3,5)→4 (6,8)→7 (0,6)→3 (1,7)→4 (2,8)→5 (0,8)→4 (2,6)→4 và đối xứng; còn lại −1.

**Touch:**
- `handleClickEvent` PRESSED → nếu cho phép: bắt đầu chuỗi mới, xóa line, hit-test điểm đầu.
- `handleClickEvent` RELEASED → chốt & đánh giá theo state.
- `handleDragEvent` → `processPoint(getNewX, getNewY)`.
- `processPoint`: với mỗi điểm chưa dùng, nếu `(dx²+dy²) ≤ 22²` → nếu có điểm trung gian chưa dùng giữa điểm cuối và điểm này thì thêm trung gian trước, rồi thêm điểm này; mỗi lần thêm (từ điểm thứ 2) vẽ `lines[k]` từ tâm trước → tâm mới.

**Đánh giá khi nhả tay:**
- `ST_LOCKED`: seqLen≥4 và `verifyPattern` đúng → xanh + "Mo khoa OK" + `ST_UNLOCKED`, timer ~3s về khóa. Sai → đỏ + "Sai, thu lai", timer ~1.5s reset. seqLen==0 → bỏ qua im lặng. Chưa có pattern → chặn.
- `ST_REGISTER_FIRST`: seqLen<4 → "Toi thieu 4 diem", giữ nguyên. Đủ → lưu tạm `firstReg`, "Ve lai de xac nhan", `ST_REGISTER_CONFIRM`.
- `ST_REGISTER_CONFIRM`: khớp `firstReg` → "Dang luu..." → `savePattern` → xanh "Da luu" → timer về khóa. Lệch → đỏ "Khong khop" → quay lại `ST_REGISTER_FIRST`.

**`handleTickEvent`:** đếm ngược `autoTimer`; hết hạn chạy hành động chờ (về khóa / vẽ lại).

**`startRegistration()`** (từ nút): `ST_REGISTER_FIRST`, xóa vẽ, reset màu trắng, "Ve pattern moi". Bỏ qua nếu đang đăng ký.

**Màu:** ACTIVE = cyan (0,170,255) khi đang vẽ; SUCCESS = xanh lá (0,200,80); ERROR = đỏ (230,40,40); IDLE dot = trắng. Đổi màu painter của circle/line rồi `invalidate()`.

## 8. Text (txtStatus)

- Viết chuỗi ASCII không dấu vào `txtStatusBuffer` bằng `Unicode::snprintf`, gọi `resizeToCurrentText()` + recenter + `invalidate()`.
- `texts.xml`: thêm `WildcardCharacters="..."` (A–Z a–z 0–9 và dấu cơ bản) cho typography `Default` → sau khi **regenerate trong TouchGFX Designer**, chữ hiện đúng. Trước đó hiện `?` nhưng màu vẫn truyền tải trạng thái.

## 9. Vùng chạm build / verify

- Thêm `Core/Src/pattern_storage.c` + `Core/Inc/pattern_storage.h` → tự được build (gcc Makefile glob + CubeIDE managed build). Không sửa Makefile.
- Sửa file C++ có sẵn: `Model`, `ModelListener`, `Screen1Presenter`, `Screen1View`.
- Sửa `texts.xml`.
- **Không build/regenerate được trên máy phát triển hiện tại** (thiếu `arm-none-eabi-gcc`, `touchgfx` CLI). Verify = review code kỹ; build/nạp thực hiện trên STM32CubeIDE / TouchGFX Designer của người dùng.

## 10. Hằng số

`PATTERN_MIN_LEN = 4`, `HOLD_MS = 3000` (nút), `HIT_RADIUS = 22`, success ~3s, error ~1.5s (theo tick ~60fps).
