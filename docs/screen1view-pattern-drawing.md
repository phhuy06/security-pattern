# Đầu việc 3 — Xử lý vẽ Pattern trên màn hình

**File chính:** `TouchGFX\gui\src\screen1_screen\Screen1View.cpp`

---

## Tổng quan

Module này xử lý toàn bộ logic bắt cảm ứng, vẽ đường nối pattern, và điều khiển luồng hoạt động của ứng dụng khóa pattern thông qua máy trạng thái.

### Chức năng chính

1. **Bắt sự kiện cảm ứng** (chạm, kéo, nhả)
2. **Hit-test điểm** (tính khoảng cách từ ngón tay đến các điểm)
3. **Tự động chèn điểm giữa** (như điện thoại Android)
4. **Vẽ đường nối** giữa các điểm
5. **Máy trạng thái** điều khiển các chế độ: LOCKED, UNLOCKED, REGISTER_FIRST, REGISTER_CONFIRM
6. **Hiển thị thông báo** và đổi màu theo trạng thái

---

## 🎯 Cấu trúc dữ liệu

### Hằng số quan trọng

```cpp
/* Tâm 9 điểm (lưới 3x3), khớp toạ độ widget trong Designer */
static const int16_t CX[9] = { 50, 120, 190,  50, 120, 190,  50, 120, 190 };
static const int16_t CY[9] = { 90,  90,  90, 160, 160, 160, 230, 230, 230 };

static const uint8_t MIN_LEN = 4;     // số điểm tối thiểu của pattern
static const int HIT_R2 = 22 * 22;    // bán kính bắt điểm (bình phương)

/* Thời lượng (đơn vị tick, ~60fps) */
static const int SUCCESS_TICKS    = 180;  // ~3s hiện thông báo mở khóa
static const int ERROR_TICKS      = 90;   // ~1.5s hiện báo lỗi
static const int SHOW_FIRST_TICKS = 60;   // ~1s giữ pattern vừa vẽ
static const int SAVE_DELAY_TICKS = 2;    // để khung "Dang luu..." kịp vẽ
```


### Lưới 3×3 điểm

```
Sơ đồ lưới pattern:

0 (50,90)    1 (120,90)    2 (190,90)
     •            •            •

3 (50,160)   4 (120,160)   5 (190,160)
     •            •            •

6 (50,230)   7 (120,230)   8 (190,230)
     •            •            •
```

### Màu sắc trạng thái

```cpp
inline colortype cActive()  { return Color::getColorFromRGB(0, 170, 255); }  // xanh dương
inline colortype cSuccess() { return Color::getColorFromRGB(0, 200, 80);  }  // xanh lá
inline colortype cError()   { return Color::getColorFromRGB(230, 40, 40); }  // đỏ
inline colortype cIdle()    { return Color::getColorFromRGB(255, 255, 255); } // trắng
```

---

## 🔄 Máy trạng thái

### Enum AppState

```cpp
enum AppState
{
    ST_LOCKED,             // khóa, chờ nhập pattern mở khóa
    ST_REGISTER_FIRST,     // đăng ký: vẽ pattern mới
    ST_REGISTER_CONFIRM,   // đăng ký: vẽ lại để xác nhận
    ST_UNLOCKED            // mở khóa thành công, đang hiện thông báo
};
```


### Enum Pending (Hành động trễ)

```cpp
enum Pending
{
    P_NONE,
    P_LOCK,                // quay về trạng thái khóa
    P_REG_AGAIN,           // bắt đầu lại đăng ký từ bước vẽ
    P_CONFIRM_CLEAR,       // xóa nét, giữ bước xác nhận
    P_SAVE                 // ghi pattern vào Flash (blocking)
};
```

### Sơ đồ chuyển trạng thái

```
ST_LOCKED
    ├─ vẽ đúng pattern → ST_UNLOCKED (hiện ~3s) → ST_LOCKED
    ├─ vẽ sai pattern → hiện lỗi (~1.5s) → ST_LOCKED
    └─ giữ nút BOOT 3s → ST_REGISTER_FIRST

ST_REGISTER_FIRST
    ├─ vẽ ≥4 điểm → ST_REGISTER_CONFIRM (hiện ~1s) → xóa nét, giữ nguyên state
    └─ vẽ <4 điểm → hiện lỗi (~1.5s) → ST_REGISTER_FIRST

ST_REGISTER_CONFIRM
    ├─ vẽ trùng lần 1 → lưu Flash → ST_LOCKED (hiện ~3s)
    └─ vẽ không khớp → hiện lỗi (~1.5s) → ST_REGISTER_FIRST

ST_UNLOCKED
    └─ sau ~3s → ST_LOCKED
```

---

## 👆 Xử lý sự kiện cảm ứng

### handleClickEvent

Bắt sự kiện **chạm** (PRESSED) và **nhả** (RELEASED):

```cpp
void Screen1View::handleClickEvent(const ClickEvent& evt)
{
    if (autoTimer > 0) {
        return;   // đang hiện thông báo -> chặn nhập
    }

    if (evt.getType() == ClickEvent::PRESSED) {
        if (state == ST_UNLOCKED) {
            return;
        }
        if (state == ST_LOCKED && !presenter->patternSet()) {
            return;   // chưa đăng ký pattern -> không cho mở khóa
        }
        beginSequence(evt.getX(), evt.getY());
    }
    else if (evt.getType() == ClickEvent::RELEASED) {
        if (dragging) {
            finishSequence();
        }
    }
}
```


**Giải thích:**
- Kiểm tra `autoTimer > 0`: nếu đang countdown (hiện thông báo), chặn mọi thao tác mới
- **PRESSED**: bắt đầu vẽ pattern → gọi `beginSequence()`
- **RELEASED**: kết thúc vẽ → gọi `finishSequence()` để đánh giá

### handleDragEvent

Bắt sự kiện **kéo** (di chuyển ngón tay khi đang chạm):

```cpp
void Screen1View::handleDragEvent(const DragEvent& evt)
{
    if (dragging) {
        processPoint(evt.getNewX(), evt.getNewY());
    }
}
```

**Giải thích:**
- Mỗi khi ngón tay di chuyển, gọi `processPoint()` với toạ độ mới
- `processPoint()` sẽ kiểm tra xem ngón tay có đi qua điểm nào mới không

### handleTickEvent

Chạy mỗi frame (~60fps) để đếm ngược `autoTimer`:

```cpp
void Screen1View::handleTickEvent()
{
    if (autoTimer > 0) {
        autoTimer--;
        if (autoTimer == 0) {
            runPending();
        }
    }
}
```

**Giải thích:**
- `autoTimer`: bộ đếm tick (1 tick ≈ 1/60 giây)
- Khi countdown về 0 → gọi `runPending()` để thực hiện hành động trễ (P_LOCK, P_SAVE, ...)

---

## 🎯 Hit-test điểm

### hitDot() — Kiểm tra ngón tay có trúng điểm nào không

```cpp
int Screen1View::hitDot(int16_t x, int16_t y) const
{
    for (int i = 0; i < 9; i++) {
        int dx = x - CX[i];
        int dy = y - CY[i];
        if (dx * dx + dy * dy <= HIT_R2) {
            return i;
        }
    }
    return -1;
}
```


**Giải thích từng bước:**

1. **Duyệt 9 điểm**: với mỗi điểm `i`, tính khoảng cách từ ngón tay `(x, y)` đến tâm điểm `(CX[i], CY[i])`
2. **Tính dx, dy**: khoảng cách ngang (`dx`), khoảng cách dọc (`dy`)
3. **So bình phương**: `dx² + dy² ≤ HIT_R2` (tương đương `√(dx² + dy²) ≤ 22`)
   - Dùng bình phương để tránh tính căn bậc hai (tốn thời gian trên vi điều khiển)
   - `HIT_R2 = 22 * 22 = 484`
4. **Trả về**: nếu trúng → trả về số thứ tự điểm `i` (0-8); không trúng → `-1`

### Ví dụ cụ thể

```
Điểm 1: tâm (120, 90)

Chạm tại (125, 95):
  dx = 125 - 120 = 5
  dy = 95 - 90 = 5
  dx² + dy² = 25 + 25 = 50 ≤ 484 ✓
  → Trúng điểm 1

Chạm tại (150, 120):
  Tính với điểm 1: dx=30, dy=30 → 900 + 900 = 1800 > 484 ✗
  Tính với điểm 2: dx=-40, dy=30 → 1600 + 900 = 2500 > 484 ✗
  ...
  → Không trúng điểm nào (trả về -1)
```

---

## 🔗 Tự động chèn điểm giữa

### middleDot() — Tìm điểm nằm giữa 2 điểm thẳng hàng

```cpp
int Screen1View::middleDot(uint8_t a, uint8_t b) const
{
    int ra = a / 3, ca = a % 3;
    int rb = b / 3, cb = b % 3;
    int sr = ra + rb, sc = ca + cb;

    /* Điểm giữa tồn tại khi hai điểm cách nhau 2 ô trên cùng đường thẳng */
    if ((sr % 2) == 0 && (sc % 2) == 0) {
        int mid = (sr / 2) * 3 + (sc / 2);
        if (mid != a && mid != b) {
            return mid;
        }
    }
    return -1;
}
```


**Giải thích từng bước:**

1. **Đổi điểm sang (hàng, cột)**: 
   - Lưới 3×3: điểm `n` → hàng `n/3`, cột `n%3`
   - VD: điểm 5 → hàng 1, cột 2
2. **Tính tổng hàng & tổng cột**: 
   - `sr = ra + rb`
   - `sc = ca + cb`
3. **Kiểm tra tồn tại điểm giữa**:
   - Nếu cả `sr` và `sc` đều **chẵn** → 2 điểm đối xứng qua 1 điểm lưới
   - Điểm giữa = `(sr/2, sc/2)` đổi về số thứ tự: `(sr/2) * 3 + (sc/2)`
4. **Loại trường hợp đặc biệt**: điểm giữa phải khác cả `a` và `b`
5. **Trả về**: nếu có điểm giữa → trả về số thứ tự; không có → `-1`

### Ví dụ cụ thể

#### Trường hợp 1: Kéo 0 → 2 (ngang)

```
Điểm 0: hàng 0, cột 0
Điểm 2: hàng 0, cột 2

sr = 0 + 0 = 0 (chẵn)
sc = 0 + 2 = 2 (chẵn)
→ Có điểm giữa

mid = (0/2) * 3 + (2/2) = 0*3 + 1 = 1
→ Tự chèn điểm 1
→ Pattern thành: 0 → 1 → 2
```

#### Trường hợp 2: Kéo 0 → 8 (chéo)

```
Điểm 0: hàng 0, cột 0
Điểm 8: hàng 2, cột 2

sr = 0 + 2 = 2 (chẵn)
sc = 0 + 2 = 2 (chẵn)
→ Có điểm giữa

mid = (2/2) * 3 + (2/2) = 1*3 + 1 = 4
→ Tự chèn điểm 4
→ Pattern thành: 0 → 4 → 8
```

#### Trường hợp 3: Kéo 0 → 5 (không thẳng hàng)

```
Điểm 0: hàng 0, cột 0
Điểm 5: hàng 1, cột 2

sr = 0 + 1 = 1 (lẻ)
sc = 0 + 2 = 2 (chẵn)
→ sr lẻ → không có điểm giữa
→ Trả về -1
→ Pattern: 0 → 5 (nối thẳng)
```


#### Trường hợp 4: Kéo 1 → 7 (dọc)

```
Điểm 1: hàng 0, cột 1
Điểm 7: hàng 2, cột 1

sr = 0 + 2 = 2 (chẵn)
sc = 1 + 1 = 2 (chẵn)
→ Có điểm giữa

mid = (2/2) * 3 + (2/2) = 1*3 + 1 = 4
→ Tự chèn điểm 4
→ Pattern thành: 1 → 4 → 7
```

---

## 🖌️ Logic vẽ Pattern

### beginSequence() — Bắt đầu vẽ

```cpp
void Screen1View::beginSequence(int16_t x, int16_t y)
{
    clearDrawing();
    dragging = true;
    processPoint(x, y);
}
```

**Giải thích:**
- Xóa nét vẽ cũ
- Đặt cờ `dragging = true`
- Xử lý điểm chạm đầu tiên

### processPoint() — Xử lý điểm khi kéo

```cpp
void Screen1View::processPoint(int16_t x, int16_t y)
{
    int idx = hitDot(x, y);
    if (idx < 0 || inSeq[idx]) {
        return;   // không trúng điểm hoặc đã chọn rồi
    }

    if (seqLen > 0) {
        int mid = middleDot(seq[seqLen - 1], (uint8_t)idx);
        if (mid >= 0 && !inSeq[mid]) {
            addDot((uint8_t)mid);   // tự chèn điểm trung gian
        }
    }
    addDot((uint8_t)idx);
}
```

**Giải thích:**
1. **Hit-test**: gọi `hitDot()` xem ngón tay có trúng điểm nào không
2. **Kiểm tra đã chọn**: nếu điểm đã nằm trong pattern → bỏ qua
3. **Tìm điểm giữa**: nếu có điểm trước đó, gọi `middleDot()` để tìm điểm cần tự chèn
4. **Chèn điểm giữa**: nếu tồn tại và chưa chọn → gọi `addDot(mid)`
5. **Thêm điểm hiện tại**: gọi `addDot(idx)`


### addDot() — Thêm điểm vào pattern

```cpp
void Screen1View::addDot(uint8_t idx)
{
    inSeq[idx] = true;
    seq[seqLen] = idx;
    seqLen++;

    setDotColor(idx, cActive());

    if (seqLen >= 2) {
        uint8_t li = (uint8_t)(seqLen - 2);
        uint8_t a = seq[seqLen - 2];
        uint8_t b = seq[seqLen - 1];
        lines[li]->setStart(CX[a], CY[a]);
        lines[li]->setEnd(CX[b], CY[b]);
        linePainters[li]->setColor(cActive());
        lines[li]->setVisible(true);
        lines[li]->invalidate();
    }
}
```

**Giải thích:**
1. **Đánh dấu điểm**: `inSeq[idx] = true` (để không chọn lại)
2. **Lưu vào chuỗi**: `seq[seqLen] = idx`, tăng `seqLen`
3. **Đổi màu điểm**: gọi `setDotColor()` với màu xanh (cActive)
4. **Vẽ đường nối**: nếu đã có ≥2 điểm, vẽ line từ điểm trước đến điểm hiện tại
   - `lines[li]`: đối tượng Line widget
   - `setStart()`, `setEnd()`: đặt tọa độ 2 đầu
   - `invalidate()`: yêu cầu vẽ lại

### finishSequence() — Kết thúc vẽ, đánh giá pattern

```cpp
void Screen1View::finishSequence()
{
    dragging = false;
    if (seqLen == 0) {
        return;   // không chạm điểm nào
    }

    switch (state)
    {
    case ST_LOCKED:
        if (seqLen >= MIN_LEN && presenter->verifyPattern(seq, seqLen)) {
            colorWholePattern(cSuccess());
            setStatus("Mo khoa OK");
            state = ST_UNLOCKED;
            pending = P_LOCK;
            autoTimer = SUCCESS_TICKS;
        } else {
            colorWholePattern(cError());
            setStatus("Sai, thu lai");
            pending = P_LOCK;
            autoTimer = ERROR_TICKS;
        }
        break;

    case ST_REGISTER_FIRST:
        if (seqLen < MIN_LEN) {
            colorWholePattern(cError());
            setStatus("Toi thieu 4 diem");
            pending = P_REG_AGAIN;
            autoTimer = ERROR_TICKS;
        } else {
            memcpy(firstReg, seq, seqLen);
            firstRegLen = seqLen;
            colorWholePattern(cActive());
            setStatus("Ve lai de xac nhan");
            state = ST_REGISTER_CONFIRM;
            pending = P_CONFIRM_CLEAR;
            autoTimer = SHOW_FIRST_TICKS;
        }
        break;

    case ST_REGISTER_CONFIRM:
        if (seqLen == firstRegLen && memcmp(seq, firstReg, seqLen) == 0) {
            colorWholePattern(cActive());
            setStatus("Dang luu...");
            pending = P_SAVE;
            autoTimer = SAVE_DELAY_TICKS;
        } else {
            colorWholePattern(cError());
            setStatus("Khong khop");
            pending = P_REG_AGAIN;
            autoTimer = ERROR_TICKS;
        }
        break;

    default:
        break;
    }
}
```

**Giải thích theo từng trạng thái:**

#### ST_LOCKED (Mở khóa)
- **Đúng pattern**: 
  - Điều kiện: `seqLen ≥ 4` và `presenter->verifyPattern()` trả về true
  - Đổi màu xanh lá (success)
  - Hiện "Mo khoa OK"
  - Chuyển sang `ST_UNLOCKED`, đặt `autoTimer = 180 tick` (~3s)
  - Sau 3s tự động về `ST_LOCKED`
- **Sai pattern**: 
  - Đổi màu đỏ (error)
  - Hiện "Sai, thu lai"
  - Đặt `autoTimer = 90 tick` (~1.5s)
  - Sau 1.5s xóa nét và về `ST_LOCKED`

#### ST_REGISTER_FIRST (Đăng ký lần 1)
- **<4 điểm**: 
  - Đổi màu đỏ, hiện "Toi thieu 4 diem"
  - Sau 1.5s xóa nét và bắt đầu lại
- **≥4 điểm**:
  - Lưu pattern vào `firstReg[]`
  - Đổi màu xanh, hiện "Ve lai de xac nhan"
  - Chuyển sang `ST_REGISTER_CONFIRM`
  - Sau 1s xóa nét (giữ nguyên state)

#### ST_REGISTER_CONFIRM (Đăng ký lần 2)
- **Khớp với lần 1**: 
  - Hiện "Dang luu..."
  - Đặt `pending = P_SAVE`, sau 2 tick gọi Flash
  - Nếu lưu thành công → về `ST_LOCKED` (hiện "Da luu" 3s)
- **Không khớp**: 
  - Đổi màu đỏ, hiện "Khong khop"
  - Sau 1.5s quay về `ST_REGISTER_FIRST`


---

## 🎨 Hiển thị & Màu sắc

### clearDrawing() — Xóa tất cả nét vẽ

```cpp
void Screen1View::clearDrawing()
{
    for (int i = 0; i < 8; i++) {
        lines[i]->setVisible(false);
        lines[i]->invalidate();
    }
    for (int i = 0; i < 9; i++) {
        setDotColor((uint8_t)i, cIdle());
        inSeq[i] = false;
    }
    seqLen = 0;
}
```

**Giải thích:**
- Ẩn tất cả 8 line (đường nối)
- Đổi tất cả 9 điểm về màu trắng (idle)
- Reset mảng `inSeq[]` và `seqLen`

### colorWholePattern() — Đổi màu toàn bộ pattern

```cpp
void Screen1View::colorWholePattern(colortype color)
{
    for (uint8_t k = 0; k < seqLen; k++) {
        setDotColor(seq[k], color);
    }
    for (uint8_t li = 0; li + 1 < seqLen; li++) {
        linePainters[li]->setColor(color);
        lines[li]->invalidate();
    }
}
```

**Giải thích:**
- Đổi màu tất cả điểm trong `seq[]`
- Đổi màu tất cả đường nối
- Dùng khi: thành công (xanh lá), lỗi (đỏ), hoặc giữ màu active (xanh dương)

### setStatus() — Hiển thị thông báo

```cpp
void Screen1View::setStatus(const char* msg)
{
    txtStatus.invalidate();        // xóa vùng cũ
    Unicode::strncpy(txtStatusBuffer, msg, TXTSTATUS_SIZE);
    txtStatus.resizeToCurrentText();
    txtStatus.setX((int16_t)(120 - txtStatus.getWidth() / 2));
    txtStatus.invalidate();        // vẽ vùng mới
}
```

**Giải thích:**
- Xóa text widget cũ
- Copy chuỗi mới vào buffer
- Resize theo nội dung
- Căn giữa màn hình (x = 120 - width/2)
- Vẽ lại


---

## ⏱️ Hành động trễ (Pending Actions)

### runPending() — Chạy hành động khi autoTimer về 0

```cpp
void Screen1View::runPending()
{
    Pending p = pending;
    pending = P_NONE;

    switch (p)
    {
    case P_LOCK:
        resetToLocked();
        break;
    case P_REG_AGAIN:
        clearDrawing();
        state = ST_REGISTER_FIRST;
        setStatus("Ve pattern moi");
        break;
    case P_CONFIRM_CLEAR:
        clearDrawing();           // giữ nguyên ST_REGISTER_CONFIRM
        setStatus("Ve lai de xac nhan");
        break;
    case P_SAVE:
        doSave();
        break;
    default:
        break;
    }
}
```

**Giải thích:**
- **P_LOCK**: quay về màn hình khóa
- **P_REG_AGAIN**: bắt đầu lại đăng ký từ đầu
- **P_CONFIRM_CLEAR**: xóa nét vẽ lần 1, giữ state xác nhận
- **P_SAVE**: gọi hàm lưu Flash

### doSave() — Lưu pattern vào Flash

```cpp
void Screen1View::doSave()
{
    if (presenter->savePattern(firstReg, firstRegLen)) {
        colorWholePattern(cSuccess());
        setStatus("Da luu");
        state = ST_LOCKED;
        pending = P_LOCK;
        autoTimer = SUCCESS_TICKS;
    } else {
        colorWholePattern(cError());
        setStatus("Loi luu Flash");
        state = ST_REGISTER_FIRST;
        pending = P_REG_AGAIN;
        autoTimer = ERROR_TICKS;
    }
}
```

**Giải thích:**
- Gọi `presenter->savePattern()` (blocking, ghi Flash)
- **Thành công**: hiện "Da luu" (xanh lá) 3s → về ST_LOCKED
- **Thất bại**: hiện "Loi luu Flash" (đỏ) 1.5s → về ST_REGISTER_FIRST


---

## 🔧 Các hàm điều khiển trạng thái

### startRegistration() — Bắt đầu đăng ký pattern

```cpp
void Screen1View::startRegistration()
{
    if (state == ST_REGISTER_FIRST || state == ST_REGISTER_CONFIRM) {
        return;   // đang đăng ký rồi
    }
    clearDrawing();
    dragging = false;
    state = ST_REGISTER_FIRST;
    pending = P_NONE;
    autoTimer = 0;
    setStatus("Ve pattern moi");
}
```

**Giải thích:**
- Được gọi từ `Screen1Presenter` khi người dùng giữ nút BOOT 3 giây
- Chuyển trực tiếp sang `ST_REGISTER_FIRST`
- Reset tất cả biến trạng thái

### resetToLocked() — Quay về màn hình khóa

```cpp
void Screen1View::resetToLocked()
{
    clearDrawing();
    dragging = false;
    state = ST_LOCKED;
    pending = P_NONE;
    autoTimer = 0;
    if (presenter->patternSet()) {
        setStatus("Nhap pattern");
    } else {
        setStatus("Giu BOOT 3s");
    }
}
```

**Giải thích:**
- Xóa tất cả nét vẽ
- Chuyển về `ST_LOCKED`
- Hiển thị:
  - **Đã có pattern**: "Nhap pattern"
  - **Chưa có pattern**: "Giu BOOT 3s" (hướng dẫn đăng ký)

---

## 📊 Bảng tóm tắt các thành viên class

### Biến trạng thái

| Biến | Kiểu | Mô tả |
|------|------|-------|
| `state` | `AppState` | Trạng thái hiện tại (LOCKED, UNLOCKED, REGISTER_FIRST, REGISTER_CONFIRM) |
| `pending` | `Pending` | Hành động trễ sẽ chạy khi autoTimer về 0 |
| `autoTimer` | `int` | Bộ đếm tick (countdown) |
| `dragging` | `bool` | Đang kéo ngón tay hay không |

### Biến pattern hiện tại

| Biến | Kiểu | Mô tả |
|------|------|-------|
| `seq[9]` | `uint8_t[]` | Chuỗi điểm đang vẽ (0-8) |
| `seqLen` | `uint8_t` | Số điểm trong `seq[]` |
| `inSeq[9]` | `bool[]` | Đánh dấu điểm đã nằm trong pattern |

### Biến đăng ký

| Biến | Kiểu | Mô tả |
|------|------|-------|
| `firstReg[9]` | `uint8_t[]` | Pattern vẽ lần đầu khi đăng ký |
| `firstRegLen` | `uint8_t` | Số điểm trong `firstReg[]` |


### Widget pointers

| Biến | Kiểu | Mô tả |
|------|------|-------|
| `circles[9]` | `Circle*[]` | Con trỏ tới 9 Circle widget |
| `circlePainters[9]` | `PainterRGB565*[]` | Con trỏ tới painter của 9 circle |
| `lines[8]` | `Line*[]` | Con trỏ tới 8 Line widget (tối đa 9 điểm → 8 đường) |
| `linePainters[8]` | `PainterRGB565*[]` | Con trỏ tới painter của 8 line |

---

## 🧪 Kịch bản sử dụng

### Kịch bản 1: Mở khóa thành công

```
1. State: ST_LOCKED
2. User chạm điểm 0 → beginSequence() → dragging = true
3. User kéo qua điểm 1 → processPoint() → addDot(1)
4. User kéo qua điểm 2 → processPoint() → addDot(2)
5. User kéo qua điểm 5 → processPoint() → addDot(5)
6. User nhả tay → finishSequence()
   - Gọi presenter->verifyPattern([0,1,2,5], 4)
   - Đúng → colorWholePattern(xanh lá)
   - setStatus("Mo khoa OK")
   - state = ST_UNLOCKED
   - autoTimer = 180 tick
7. Sau 3 giây → handleTickEvent() → runPending()
   - resetToLocked()
   - state = ST_LOCKED
```

### Kịch bản 2: Mở khóa sai

```
1. State: ST_LOCKED
2. User vẽ pattern [0,1,2] (chỉ 3 điểm)
3. User nhả tay → finishSequence()
   - seqLen < 4 hoặc pattern sai
   - colorWholePattern(đỏ)
   - setStatus("Sai, thu lai")
   - autoTimer = 90 tick
4. Sau 1.5 giây → runPending()
   - resetToLocked()
   - Xóa nét, về ST_LOCKED
```

### Kịch bản 3: Đăng ký pattern mới

```
1. State: ST_LOCKED
2. User giữ nút BOOT 3s → startRegistration()
   - state = ST_REGISTER_FIRST
   - setStatus("Ve pattern moi")

3. User vẽ pattern [0,4,8,7] (4 điểm)
4. User nhả tay → finishSequence()
   - seqLen >= 4 → OK
   - Lưu firstReg = [0,4,8,7], firstRegLen = 4
   - colorWholePattern(xanh)
   - setStatus("Ve lai de xac nhan")
   - state = ST_REGISTER_CONFIRM
   - autoTimer = 60 tick

5. Sau 1 giây → runPending()
   - clearDrawing() (xóa nét, giữ state)
   - setStatus("Ve lai de xac nhan")

6. User vẽ lại [0,4,8,7]
7. User nhả tay → finishSequence()
   - memcmp(seq, firstReg) == 0 → khớp
   - setStatus("Dang luu...")
   - pending = P_SAVE
   - autoTimer = 2 tick

8. Sau 2 tick → runPending() → doSave()
   - presenter->savePattern(firstReg, 4)
   - Thành công → colorWholePattern(xanh lá)
   - setStatus("Da luu")
   - state = ST_LOCKED
   - autoTimer = 180 tick

9. Sau 3 giây → resetToLocked()
```


### Kịch bản 4: Đăng ký không khớp

```
1-5. (giống kịch bản 3, vẽ lần 1 xong)

6. User vẽ lại [0,1,2,5] (khác lần 1)
7. User nhả tay → finishSequence()
   - memcmp(seq, firstReg) != 0 → không khớp
   - colorWholePattern(đỏ)
   - setStatus("Khong khop")
   - pending = P_REG_AGAIN
   - autoTimer = 90 tick

8. Sau 1.5 giây → runPending()
   - clearDrawing()
   - state = ST_REGISTER_FIRST
   - setStatus("Ve pattern moi")
   - (Bắt đầu lại từ đầu)
```

---

## 🎓 Câu hỏi thường gặp (FAQ)

### Q1: Tại sao dùng bình phương khoảng cách thay vì căn bậc hai?

**A:** Để kiểm tra điểm `(x, y)` có nằm trong bán kính `R` hay không:
- **Cách thông thường**: `√(dx² + dy²) ≤ R` → phải tính căn (chậm)
- **Cách tối ưu**: `dx² + dy² ≤ R²` → chỉ cần nhân (nhanh hơn)

Kết quả hai cách hoàn toàn giống nhau, nhưng cách 2 nhanh hơn rất nhiều trên vi điều khiển.

### Q2: Máy trạng thái là gì?

**A:** Máy trạng thái hữu hạn (Finite State Machine - FSM) là mô hình có:
- **Tập hữu hạn trạng thái**: ST_LOCKED, ST_UNLOCKED, ST_REGISTER_FIRST, ST_REGISTER_CONFIRM
- **Trạng thái hiện tại**: chỉ ở 1 trong 4 trạng thái
- **Chuyển trạng thái**: dựa vào sự kiện (vẽ đúng, vẽ sai, timeout, ...)

Ví dụ: Đèn giao thông (xanh → vàng → đỏ → xanh...)

### Q3: Tại sao phải có hành động trễ (Pending)?

**A:** Một số hành động cần:
- **Hiển thị thông báo trước**: "Dang luu..." phải hiện ~33ms (2 tick) để người dùng thấy
- **Blocking operation**: lưu Flash mất vài chục ms, không nên chạy ngay trong `finishSequence()`
- **UX tốt hơn**: người dùng thấy được phản hồi (màu xanh/đỏ + text) trước khi chuyển màn hình

### Q4: Sao có 9 điểm nhưng chỉ 8 line?

**A:** Line nối giữa 2 điểm:
- 1 điểm → không có line
- 2 điểm → 1 line
- 3 điểm → 2 line
- ...
- 9 điểm → 8 line

Công thức: `số line = số điểm - 1`


### Q5: Vì sao cần `invalidate()`?

**A:** TouchGFX dùng cơ chế vẽ lại theo vùng (dirty region):
- `widget->invalidate()`: đánh dấu vùng widget cần vẽ lại
- Nếu không gọi → widget thay đổi nhưng màn hình không cập nhật
- Giống `repaint()` trong Java Swing hoặc `update()` trong game loop

### Q6: Tại sao có cả `inSeq[]` và `seq[]`?

**A:** Hai mảng có vai trò khác nhau:
- **`seq[]`**: lưu thứ tự điểm (để kiểm tra pattern)
  - VD: `[0, 1, 2, 5]` nghĩa là đi qua 0 → 1 → 2 → 5
- **`inSeq[]`**: đánh dấu nhanh điểm đã chọn (để không chọn lại)
  - VD: `inSeq[1] = true` nghĩa là điểm 1 đã được chọn
  - Tra cứu O(1) thay vì duyệt `seq[]` O(n)

### Q7: Tự chèn điểm giữa có thể tắt được không?

**A:** Có thể, chỉ cần comment hoặc xóa đoạn này trong `processPoint()`:

```cpp
if (seqLen > 0) {
    int mid = middleDot(seq[seqLen - 1], (uint8_t)idx);
    if (mid >= 0 && !inSeq[mid]) {
        addDot((uint8_t)mid);   // ← comment dòng này
    }
}
```

Khi đó, kéo 0 → 2 sẽ chỉ nối thẳng 0→2, không tự chèn điểm 1.

---

## 🛠️ Công nghệ sử dụng

- **TouchGFX Framework**: thư viện GUI cho STM32
- **Circle, Line Widget**: đối tượng vẽ hình tròn và đường thẳng
- **PainterRGB565**: xử lý màu sắc (16-bit RGB)
- **ClickEvent, DragEvent**: sự kiện cảm ứng
- **Tick-based timing**: mỗi tick ≈ 1/60 giây (60fps)

---

## 📚 Tham chiếu

- **File header**: `TouchGFX\gui\include\gui\screen1_screen\Screen1View.hpp`
- **File implementation**: `TouchGFX\gui\src\screen1_screen\Screen1View.cpp`
- **Liên quan**: 
  - `Screen1Presenter`: xử lý logic nghiệp vụ (verify, save pattern)
  - `pattern_storage.c`: lưu/đọc Flash
  - **Báo cáo chi tiết**: `readme.md` (mục 4.3, 4.4, 4.5)

---

## 📝 Ghi chú kỹ thuật

1. **Thread-safe**: code chạy trên 1 thread (GUI task), không cần mutex
2. **Memory**: pattern tối đa 9 điểm × 1 byte = 9 bytes
3. **Performance**: hit-test O(9), middleDot O(1), tổng complexity O(n) với n≤9
4. **Tọa độ**: màn hình 240×320, gốc (0,0) góc trên-trái
5. **Color format**: RGB565 (5 bit đỏ, 6 bit xanh lá, 5 bit xanh dương)

---

**Tác giả tài liệu:** Được tạo tự động từ mã nguồn  
**Ngày cập nhật:** 2026-07-21
