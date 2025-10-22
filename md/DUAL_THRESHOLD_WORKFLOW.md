# 🎯 Quy Trình Tracking Với Ngưỡng Kép Chi Tiết

## Luồng Hoạt Động (Workflow)

### 📍 GIAI ĐOẠN 1: Phát hiện lần đầu

```
Detection confidence > BOX_THRESH (0.5)
        ↓
   Tạo TENTATIVE track
        ↓
   Bắt đầu đếm hits
```

**Ví dụ**:
```
Frame 1: Person detected, confidence 60%
         60% > 0.5 (BOX_THRESH) ✅
         → Tạo TENTATIVE ID:1 (hits=1/3)
         → Chưa hiển thị trên màn hình
```

---

### 📍 GIAI ĐOẠN 2: Xác nhận (Confirmation Phase)

```
TENTATIVE track cần:
- Confidence > BOX_THRESH (0.5) ⚠️ VẪN CẦN CAO!
- Xuất hiện MIN_HITS_TO_CONFIRM lần (3 lần)
- Trong CONFIRMATION_TIME_WINDOW frames (30 frames)
```

**Ví dụ**:
```
Frame 1: Person 60% → TENTATIVE (1/3 hits) ✅ (60% > 0.5)
Frame 2: Person 45% → ❌ Không đếm (45% < 0.5)
Frame 3: Person 55% → TENTATIVE (2/3 hits) ✅ (55% > 0.5)
Frame 5: Person 52% → TENTATIVE (3/3 hits) ✅ (52% > 0.5)
         → ✨ Confirmed! → NEW ID:1
         → Bắt đầu hiển thị màu XANH LÁ
```

**Quan trọng**: Trong giai đoạn TENTATIVE, vẫn cần confidence CAO (> 0.5) để tránh confirm các đối tượng không chắc chắn!

---

### 📍 GIAI ĐOẠN 3: Tracking ổn định (Stable Tracking)

```
Sau khi confirmed → NEW/TRACKED
Chỉ cần: Confidence > BOX_THRESH_TRACKING (0.1) ⚡ THẤP!
```

**Ví dụ**:
```
Frame 6: Person 58% → TRACKED ID:1 (58% > 0.1) ✅
Frame 7: Person 48% → TRACKED ID:1 (48% > 0.1) ✅
Frame 8: Person 35% (bị che) → TRACKED ID:1 (35% > 0.1) ✅
Frame 9: Person 22% (bị che nhiều) → TRACKED ID:1 (22% > 0.1) ✅
Frame 10: Person 15% (gần như khuất) → TRACKED ID:1 (15% > 0.1) ✅
```

**Lợi ích**: Giữ track ngay cả khi confidence giảm mạnh do che khuất!

---

### 📍 GIAI ĐOẠN 4: Tạm thời mất (Temporary Loss)

```
Không detect được trong vài frames
        ↓
   LOST_TEMP (sau MAX_LOST_FRAMES = 30)
        ↓
Nếu detect lại với confidence > BOX_THRESH_TRACKING (0.1)
        ↓
   RE-TRACKED (giữ nguyên ID cũ)
```

**Ví dụ**:
```
Frame 11-40: Không detect được (bị che hoàn toàn)
Frame 41: → LOST_TEMP ID:1 (màu CAM)

Frame 45: Person xuất hiện lại, confidence 25%
          25% > 0.1 (BOX_THRESH_TRACKING) ✅
          → 🔄 RE-TRACKED ID:1 (giữ ID cũ!)
```

---

## 🔄 So Sánh Trước vs Sau

### ❌ TRƯỚC (Không có dual threshold)

```
BOX_THRESH = 0.5 (dùng cho tất cả)

┌─────────────────────────────────────────────────────────┐
│ Frame 1: Person 60%    → NEW ID:1        (60% > 0.5) ✅ │
│ Frame 2: Person 55%    → TRACKED ID:1    (55% > 0.5) ✅ │
│ Frame 3: Person 40%    → ❌ MẤT          (40% < 0.5) ❌ │
│ Frame 4-33: Không detect                               │
│ Frame 34: → LOST_TEMP ID:1                             │
│ Frame 40: Person 60%   → NEW ID:2        (ID MỚI!) ❌  │
└─────────────────────────────────────────────────────────┘

Vấn đề: ID thay đổi 1→2 chỉ vì confidence giảm tạm thời!
```

### ✅ SAU (Có dual threshold)

```
BOX_THRESH = 0.5 (cho TENTATIVE)
BOX_THRESH_TRACKING = 0.1 (cho TRACKED)

┌──────────────────────────────────────────────────────────────┐
│ GIAI ĐOẠN TENTATIVE (cần confidence CAO):                   │
│ Frame 1: Person 60%  → TENTATIVE (1/3)  (60% > 0.5) ✅      │
│ Frame 2: Person 45%  → ❌ Không đếm      (45% < 0.5) ❌      │
│ Frame 3: Person 55%  → TENTATIVE (2/3)  (55% > 0.5) ✅      │
│ Frame 4: Person 52%  → ✨ NEW ID:1 (3/3) (52% > 0.5) ✅      │
│                                                              │
│ GIAI ĐOẠN TRACKED (chấp nhận confidence THẤP):              │
│ Frame 5: Person 48%  → TRACKED ID:1      (48% > 0.1) ✅     │
│ Frame 6: Person 40%  → TRACKED ID:1      (40% > 0.1) ✅     │
│ Frame 7: Person 28%  → TRACKED ID:1      (28% > 0.1) ✅     │
│ Frame 8: Person 15%  → TRACKED ID:1      (15% > 0.1) ✅     │
│ Frame 9-38: Không detect                                    │
│ Frame 39: → LOST_TEMP ID:1                                  │
│ Frame 45: Person 25% → 🔄 RE-TRACKED ID:1 (25% > 0.1) ✅    │
│                        (GIỮ NGUYÊN ID!)                     │
└──────────────────────────────────────────────────────────────┘

Kết quả: ID:1 được giữ suốt quá trình! 🎯
```

---

## 📊 Bảng So Sánh Chi Tiết

| Giai đoạn | State | Ngưỡng cần | Mục đích | Màu hiển thị |
|-----------|-------|-----------|----------|--------------|
| **1. Phát hiện đầu** | - | > BOX_THRESH (0.5) | Chỉ tạo track từ detection tốt | Chưa hiển thị |
| **2. Xác nhận** | TENTATIVE | > BOX_THRESH (0.5) | Đảm bảo object thật, không phải noise | Chưa hiển thị |
| **3. Mới confirmed** | NEW | > BOX_THRESH_TRACKING (0.1) | Bắt đầu theo dõi | 🟢 XANH LÁ |
| **4. Đang theo dõi** | TRACKED | > BOX_THRESH_TRACKING (0.1) | Giữ track qua che khuất | 🔵 XANH DƯƠNG |
| **5. Mất tạm thời** | LOST_TEMP | > BOX_THRESH_TRACKING (0.1) | Tìm lại track | 🟠 CAM |
| **6. Tìm lại** | TRACKED | > BOX_THRESH_TRACKING (0.1) | Khôi phục track | 🔵 XANH DƯƠNG |

---

## 🎬 Kịch Bản Thực Tế Chi Tiết

### Kịch bản: Người đi qua cột điện

```
Cấu hình:
- BOX_THRESH = 0.5
- BOX_THRESH_TRACKING = 0.1
- MIN_HITS_TO_CONFIRM = 3
- CONFIRMATION_TIME_WINDOW = 30

Timeline:

Frame 1: 
  Person visible, conf=65%
  65% > 0.5 ✅
  → 👁️ TENTATIVE ID:1 (1/3 hits)
  → Log: "TENTATIVE (needs 3 hits in 30 frames)"

Frame 2:
  Person visible, conf=58%
  58% > 0.5 ✅
  → 👁️ TENTATIVE ID:1 (2/3 hits)

Frame 3:
  Person visible, conf=52%
  52% > 0.5 ✅
  → ✨ NEW ID:1 (3/3 hits, confirmed!)
  → Log: "NEW (confirmed with 3 hits in 3 frames)"
  → Hiển thị hộp XANH LÁ

Frame 4-10:
  Person walking normally, conf=55-62%
  Tất cả > 0.1 ✅
  → 🔵 TRACKED ID:1
  → Hiển thị hộp XANH DƯƠNG

Frame 11:
  Person starts going behind pole, conf=42%
  42% > 0.1 ✅
  → 🔵 TRACKED ID:1 (vẫn tracking!)

Frame 12:
  Partially hidden, conf=28%
  28% > 0.1 ✅
  → 🔵 TRACKED ID:1 (vẫn tracking!)

Frame 13:
  More hidden, conf=18%
  18% > 0.1 ✅
  → 🔵 TRACKED ID:1 (vẫn tracking!)

Frame 14-15:
  Almost fully hidden, conf=12%
  12% > 0.1 ✅
  → 🔵 TRACKED ID:1 (vẫn tracking!)

Frame 16-40:
  Fully hidden behind pole, no detection
  → time_since_update++

Frame 46:
  (30 frames without update)
  → ⚠️ LOST_TEMP ID:1
  → Hiển thị hộp CAM ở vị trí cuối

Frame 50:
  Person emerges, conf=22%
  22% > 0.1 ✅
  IoU với LOST_TEMP track > 0.3
  → 🔄 RE-TRACKED ID:1 (tìm lại!)
  → Log: "RE-TRACKED (was lost temporarily)"

Frame 51+:
  Person fully visible, conf=60%
  → 🔵 TRACKED ID:1

KẾT QUẢ: Giữ ID:1 từ đầu đến cuối! 🎯
```

---

## 💡 Tại Sao Cần 2 Ngưỡng Khác Nhau?

### 🎯 Nguyên lý: "Khó vào, dễ ở"

```
          BOX_THRESH (0.5)
        "Cổng vào chặt chẽ"
                ↓
        ┌───────────────┐
        │   TENTATIVE   │  ← Phải vượt qua 3 lần với conf > 0.5
        │  (xác nhận)   │
        └───────┬───────┘
                ↓
        ┌───────────────┐
        │   CONFIRMED   │  ← Đã xác nhận là object thật
        │  NEW/TRACKED  │
        └───────┬───────┘
                ↓
      BOX_THRESH_TRACKING (0.1)
      "Dễ dàng duy trì"
```

### Lý do:

1. **Giai đoạn TENTATIVE (ngưỡng CAO 0.5)**:
   - Chưa chắc object có thật
   - Có thể là: shadow, reflection, noise, motion blur
   - **Cần**: Detection ổn định với confidence cao
   - **Mục tiêu**: Lọc false positive

2. **Giai đoạn TRACKED (ngưỡng THẤP 0.1)**:
   - Đã xác nhận object thật rồi
   - Confidence giảm do: che khuất, ánh sáng thay đổi, góc nhìn
   - **Cần**: Duy trì track liên tục
   - **Mục tiêu**: ID ổn định

---

## ⚙️ Điều Chỉnh Tham Số

### Tình huống 1: Quá nhiều TENTATIVE expired
```
👁️ ID 5: TENTATIVE (needs 3 hits in 30 frames)
⏱️ ID 5: TENTATIVE expired (2 hits in 30 frames)
👁️ ID 8: TENTATIVE (needs 3 hits in 30 frames)
⏱️ ID 8: TENTATIVE expired (2 hits in 30 frames)
```

**Nguyên nhân**: BOX_THRESH quá cao, object khó đạt ngưỡng

**Giải pháp**:
```cpp
#define BOX_THRESH 0.4           // Giảm từ 0.5
#define BOX_THRESH_TRACKING 0.1  // Giữ nguyên
```

---

### Tình huống 2: Quá nhiều ID mới được tạo
```
✨ ID 5: NEW
✨ ID 6: NEW  
✨ ID 7: NEW (shadow)
✨ ID 8: NEW (reflection)
```

**Nguyên nhân**: BOX_THRESH quá thấp, tạo track từ noise

**Giải pháp**:
```cpp
#define BOX_THRESH 0.6           // Tăng từ 0.5
#define MIN_HITS_TO_CONFIRM 5    // Tăng từ 3
```

---

### Tình huống 3: Track bị mất liên tục
```
🔵 TRACKED ID:1
⚠️ LOST_TEMP ID:1
❌ LOST_PERMANENTLY ID:1
✨ NEW ID:5 (same object)
```

**Nguyên nhân**: BOX_THRESH_TRACKING quá cao, không giữ được track khi confidence giảm

**Giải pháp**:
```cpp
#define BOX_THRESH 0.5           // Giữ nguyên
#define BOX_THRESH_TRACKING 0.05 // Giảm từ 0.1
```

---

### Tình huống 4: Track giữ quá nhiều false positive
```
🔵 TRACKED ID:3 (shadow, conf=15%)
🔵 TRACKED ID:4 (reflection, conf=12%)
```

**Nguyên nhân**: BOX_THRESH_TRACKING quá thấp, giữ cả noise

**Giải pháp**:
```cpp
#define BOX_THRESH 0.5           // Giữ nguyên  
#define BOX_THRESH_TRACKING 0.2  // Tăng từ 0.1
```

---

## 📋 Bảng Cấu Hình Khuyến Nghị

| Môi trường | BOX_THRESH | BOX_THRESH_TRACKING | MIN_HITS | Lý do |
|-----------|------------|---------------------|----------|-------|
| **Indoor sạch** | 0.4 | 0.1 | 2 | Ít noise, tracking dễ |
| **Outdoor chuẩn** | 0.5 | 0.1 | 3 | Cân bằng (mặc định) |
| **Nhiều noise** | 0.6 | 0.15 | 4 | Lọc noise mạnh |
| **Che khuất nhiều** | 0.5 | 0.05 | 3 | Giữ track qua occlusion |
| **Traffic** | 0.5 | 0.1 | 3 | Xe che nhau thường xuyên |
| **Retail** | 0.45 | 0.1 | 2 | Người động nhiều |
| **Parking** | 0.55 | 0.2 | 4 | Ít động, confidence ổn định |

---

## 🧪 Testing Commands

### Test cơ bản:
```bash
./build.sh
./run.sh
```

### Test với log chi tiết:
```bash
./ff-rknn -f rtsp -i "rtsp://camera-url" \
          -x 960 -y 540 \
          -m ./model/RK3588/yolov8.rknn \
          -fps 5 -track true 2>&1 | \
          grep -E "TENTATIVE|NEW|TRACKED|LOST|confidence"
```

### Quan sát những log sau:
```
👁️  TENTATIVE → Đang xác nhận
✨ NEW → Đã confirm, bắt đầu tracking với ngưỡng thấp
🔵 TRACKED → Đang tracking ổn định
⚠️  LOST_TEMP → Tạm thời mất
🔄 RE-TRACKED → Tìm lại track
❌ LOST_PERMANENTLY → Xóa track
```

---

## 🎯 Tóm Tắt

### Quy Trình 3 Bước:

```
┌────────────────────────────────────────────┐
│ BƯỚC 1: PHÁT HIỆN & XÁC NHẬN              │
│ Ngưỡng: BOX_THRESH (0.5) - CAO            │
│ Mục đích: Lọc false positive              │
│                                            │
│ Detection > 0.5 → TENTATIVE                │
│ Xuất hiện 3/30 frames với conf > 0.5       │
│ → ✨ CONFIRMED                             │
└────────────────┬───────────────────────────┘
                 ↓
┌────────────────────────────────────────────┐
│ BƯỚC 2: TRACKING ỔN ĐỊNH                  │
│ Ngưỡng: BOX_THRESH_TRACKING (0.1) - THẤP  │
│ Mục đích: Giữ track qua che khuất         │
│                                            │
│ Detection > 0.1 → Cập nhật track           │
│ Confidence có thể giảm xuống 10%           │
│ → ID ổn định                               │
└────────────────┬───────────────────────────┘
                 ↓
┌────────────────────────────────────────────┐
│ BƯỚC 3: TÌM LẠI TRACK                     │
│ Ngưỡng: BOX_THRESH_TRACKING (0.1) - THẤP  │
│ Mục đích: Khôi phục track sau che khuất   │
│                                            │
│ LOST_TEMP + Detection > 0.1 + IoU > 0.3    │
│ → 🔄 RE-TRACKED                            │
│ → Giữ nguyên ID cũ                         │
└────────────────────────────────────────────┘
```

### Key Points:
1. ✅ **TENTATIVE cần confidence CAO** (0.5) để confirm
2. ✅ **TRACKED chỉ cần confidence THẤP** (0.1) để duy trì
3. ✅ **Giảm false positive** nhờ ngưỡng cao ban đầu
4. ✅ **ID ổn định** nhờ ngưỡng thấp khi tracking

---

**Files liên quan**:
- Config: `postprocess.h`
- Logic: `utils/tracker.cc`
- Tracking params: `utils/tracker.h`
- Documentation: `DUAL_THRESHOLD.md`

**Ngày cập nhật**: 17 October 2025
