# SECURITY PATTERN - Khóa mở bằng Pattern trên STM32F429 + TouchGFX

Hệ thống khóa/mở khóa bằng **pattern lưới 3x3** (giống mở khóa điện thoại) chạy trên kit
**STM32F429I-DISCO**, giao diện vẽ bằng **TouchGFX**. Pattern được **lưu vĩnh viễn trong Flash**
nội nên vẫn còn sau khi khởi động lại thiết bị.

---

## 1. GIỚI THIỆU

### Đề bài
Xây dựng chức năng mở khóa bằng pattern giống điện thoại trên STM32F429 dùng TouchGFX, gồm
3 chức năng: **đăng ký**, **kiểm tra (so khớp)** và **lưu trữ** pattern.

- Giữ nút **BOOT (USER_BUTTON)** có sẵn trên kit trong **3 giây** để vào trạng thái **đăng ký**
  pattern, xác nhận và lưu pattern vào **vùng nhớ Flash vĩnh viễn** để đọc lại khi khởi động.
- Ở trạng thái bình thường, thiết bị **khóa** và hiển thị **9 điểm** để người dùng nhập pattern.
- Mở khóa thành công thì màn hình hiện **thông báo**, rồi **tự quay về trạng thái khóa sau vài giây**.

### Sản phẩm
1. **Đăng ký pattern**: giữ nút BOOT 3s để vẽ pattern mới, vẽ lại để xác nhận, hệ thống lưu Flash.
2. **Kiểm tra / mở khóa**: vẽ pattern; đúng thì mở khóa (xanh) + thông báo; sai thì báo lỗi (đỏ) và cho thử lại.
3. **Lưu trữ vĩnh viễn**: pattern ghi vào Flash sector cuối, tồn tại qua các lần reset/tắt nguồn.

### Ảnh minh họa
<p align="center">
  <!-- TODO: chèn ảnh tổng quan sản phẩm -->
  <img src="docs/images/overview.jpg" width="320" alt="Tổng quan sản phẩm"><br/>
  <em>Hình 1. Màn hình khóa với lưới 9 điểm</em>
</p>

---

## 2. TÁC GIẢ

| STT | Họ và tên | Mã số sinh viên |
|:---:|:----------|:----------------|
| 1 | Phạm Quang Huy | 20225338 |
| 2 | Trương Huy Khuê | 20225348 |
| 3 | Tạ Minh Hiếu | 20225317 |
| 4 | Hoàng Thành Long | 20225207 |
| 5 | Nguyễn Cảnh Huy | 20225334 |

---

## 3. MÔI TRƯỜNG HOẠT ĐỘNG

- **Kit:** STM32F429I-DISCO (MCU STM32F429ZIT6, 2MB Flash, màn LCD 240x320 16bpp, cảm ứng STMPE811).
- **Framework đồ họa:** TouchGFX 4.26.1 (kiến trúc MVP) chạy trên **FreeRTOS** (CMSIS-RTOS v2).
- **Nút vào đăng ký:** USER_BUTTON nối **PA0** (active-high), chính là nút "BOOT" trên kit.
- **Công cụ build/nạp:** STM32CubeIDE hoặc TouchGFX Designer + STM32CubeProgrammer.

---

## 4. THIẾT KẾ HỆ THỐNG

### 4.1. Kiến trúc phần mềm

```
pattern_storage.c/.h  (C thuần, Core/)  -> đọc/ghi/xóa Flash sector 23 (0x081E0000)
        ^ extern "C"
Model (C++)   -> nạp pattern lúc khởi động; poll nút PA0 mỗi tick; verify / save
        ^ ModelListener::notifyRegisterRequested()
Screen1Presenter  -> cầu nối Model <-> View
        ^
Screen1View   -> máy trạng thái + cảm ứng + hit-test 9 điểm + vẽ 8 đường nối + thông báo
```

### 4.2. Đánh số 9 điểm (lưới 3x3)

```
 0   1   2        tâm (x,y):  0:(50,90)   1:(120,90)   2:(190,90)
 3   4   5                    3:(50,160)  4:(120,160)  5:(190,160)
 6   7   8                    6:(50,230)  7:(120,230)  8:(190,230)
```
Khi kéo ngón đi qua giữa hai điểm thẳng hàng cách nhau 2 ô (vd 0->2, 0->8, 1->7...), điểm ở giữa
được **tự động chèn** vào pattern, giống cơ chế trên điện thoại.

### 4.3. Máy trạng thái

```
LOCKED --(vẽ đúng)----------------> UNLOCKED --(~3s)--> LOCKED
  |   \--(vẽ sai)--> báo đỏ "Sai, thu lai" --(~1.5s)--> LOCKED
  |
  \--(giữ BOOT 3s)--> REGISTER_FIRST --(>=4 điểm)--> REGISTER_CONFIRM
                                                       |--(khớp)--> lưu Flash -> "Da luu" -> LOCKED
                                                       \--(lệch)--> "Khong khop" -> REGISTER_FIRST
```
Lần đầu chưa có pattern: màn khóa hiện **"Giu BOOT 3s"** và chặn mở khóa cho tới khi đăng ký.

### 4.4. Bảng thành phần

| Thành phần | Vai trò |
|:-----------|:--------|
| `Core/Src/pattern_storage.c` | Đọc/ghi/xóa pattern trong Flash (magic + CRC16) |
| `TouchGFX/gui/src/model/Model.cpp` | Nạp pattern khi khởi động, poll nút PA0 (giữ 3s), verify/save |
| `TouchGFX/gui/src/screen1_screen/Screen1Presenter.cpp` | Chuyển sự kiện nút và truy cập Model cho View |
| `TouchGFX/gui/src/screen1_screen/Screen1View.cpp` | Cảm ứng, hit-test, vẽ đường nối, đổi màu, thông báo, timer |
| `TouchGFX/assets/texts/texts.xml` | Khai báo dải ký tự cho thông báo (wildcard) |

### 4.5. Đặc tả hàm chính

```c
/* pattern_storage.h - lưu trữ Flash */
bool PatternStorage_Load(uint8_t* dots, uint8_t* len);        // true nếu Flash có pattern hợp lệ
bool PatternStorage_Save(const uint8_t* dots, uint8_t len);   // erase + ghi pattern (vĩnh viễn)
```

```cpp
/* Model - logic nghiệp vụ */
void Model::tick();                                                // poll PA0; giữ 3s -> báo đăng ký
bool Model::verifyPattern(const uint8_t* dots, uint8_t len) const; // so khớp pattern đã lưu
bool Model::savePattern(const uint8_t* dots, uint8_t len);         // ghi Flash + cập nhật RAM
```

```cpp
/* Screen1View - tương tác & hiển thị */
void Screen1View::processPoint(int16_t x, int16_t y);     // bắt điểm khi kéo, tự chèn điểm giữa
void Screen1View::finishSequence();                       // đánh giá pattern khi nhả tay
void Screen1View::startRegistration();                    // vào chế độ đăng ký (gọi từ Presenter)
```

---

## 5. HƯỚNG DẪN SỬ DỤNG

1. **Lần đầu:** màn hình hiện *"Giu BOOT 3s"*. Giữ nút **BOOT** ~3 giây để vào đăng ký.
2. **Đăng ký:** vẽ pattern (>= 4 điểm), màn hiện *"Ve lai de xac nhan"*, vẽ lại đúng như vậy,
   *"Dang luu..."* (màn hình **đứng ~1s** do xóa Flash), rồi *"Da luu"* và về khóa.
3. **Mở khóa:** vẽ đúng pattern thì hiện *"Mo khoa OK"* (xanh) và tự khóa lại sau ~3s. Sai thì *"Sai, thu lai"* (đỏ).

---

## 6. BUILD & NẠP

- **STM32CubeIDE:** mở thư mục dự án (managed build tự nhận file `.c` mới trong `Core/Src`), Build, Run.
- **TouchGFX Designer:** mở `TouchGFX/SecurityPattern.touchgfx`, **Generate Code**, Run Target
  (dùng GCC + STM32CubeProgrammer).
- Vùng lưu pattern: **Flash sector 23** (`0x081E0000`, 128KB cuối của 2MB), không đụng vùng code.

---

## 7. KẾT QUẢ

### Video sản phẩm
- 🎥 _(TODO: dán link video demo)_

### Ảnh chụp sản phẩm
<p align="center">
  <!-- TODO: chèn ảnh các trạng thái -->
  <img src="docs/images/locked.jpg"   width="240" alt="Khóa">
  <img src="docs/images/drawing.jpg"  width="240" alt="Đang vẽ"><br/>
  <em>Hình 2. Trạng thái khóa và đang vẽ pattern</em>
</p>
<p align="center">
  <img src="docs/images/unlocked.jpg" width="240" alt="Mở khóa">
  <img src="docs/images/register.jpg" width="240" alt="Đăng ký"><br/>
  <em>Hình 3. Mở khóa thành công và đăng ký pattern mới</em>
</p>
