# 🎯 Hệ Thống Ngưỡng Kép (Dual Threshold System)

## Vấn Đề

Khi tăng `BOX_THRESH` lên cao (ví dụ 0.5) để giảm false positive:
- ✅ **Ưu điểm**: Ít phát hiện sai (bóng, phản chiếu, nhiễu)
- ❌ **Nhược điểm**: Tracking kém đi vì:
  - Khi object bị che khuất một phần → confidence giảm (0.3-0.4)
  - Confidence < BOX_THRESH → Mất detection → Track bị LOST
  - ID thay đổi liên tục khi object ra vào vùng che khuất

## Giải Pháp: Ngưỡng Kép

Sử dụng **2 ngưỡng khác nhau**:

### 1. `BOX_THRESH` - Ngưỡng Cao (0.5)
```cpp
#define BOX_THRESH 0.5  // Ngưỡng cho detection MỚI
```

**Áp dụng cho**: 
- ✨ Tạo NEW tracks
- 👁️ Xác nhận TENTATIVE → NEW

**Mục đích**:
- Chỉ tạo ID mới cho detection có confidence CAO
- Tránh tạo track từ noise/false positive
- Giảm số lượng ID rác

**Ví dụ**:
```
Detection: person 45% → ❌ Không tạo track (< 0.5)
Detection: person 60% → ✅ Tạo track mới ID:5
Detection: shadow 35% → ❌ Không tạo track
```

---

### 2. `BOX_THRESH_TRACKING` - Ngưỡng Thấp (0.25)
```cpp
#define BOX_THRESH_TRACKING 0.25  // Ngưỡng cho tracking hiện tại
```

**Áp dụng cho**:
- 🔵 Cập nhật TRACKED tracks (đã tồn tại)
- ⚠️ Duy trì LOST_TEMP tracks

**Mục đích**:
- Giữ track ngay cả khi confidence giảm tạm thời
- Xử lý tình huống che khuất một phần
- Duy trì ID ổn định

**Ví dụ**:
```
ID:5 tracking với confidence 60%
→ Object bị che 1 phần → confidence giảm 35%
→ 35% > 0.25 → ✅ Vẫn cập nhật track (giữ ID:5)
→ Không cần ngưỡng 0.5 vì track đã tồn tại!
```

---

## So Sánh: Trước vs Sau

### ❌ TRƯỚC (Chỉ có BOX_THRESH = 0.5)

```
Frame 1: Person 60% → NEW ID:1 ✅
Frame 2: Person 55% → TRACKED ID:1 ✅
Frame 3: Person 40% (bị che) → ❌ Mất detection (< 0.5)
Frame 4: Person 35% (vẫn che) → ❌ Mất detection
Frame 5: Person 38% (vẫn che) → ❌ Mất detection
Frame 35: LOST_TEMP → ID:1
Frame 50: Person 60% (hết che) → NEW ID:2 ❌ (ID mới!)

Vấn đề: Đổi ID từ 1 → 2 chỉ vì bị che tạm thời!
```

### ✅ SAU (Có cả BOX_THRESH_TRACKING = 0.25)

```
Frame 1: Person 60% → NEW ID:1 ✅ (> 0.5)
Frame 2: Person 55% → TRACKED ID:1 ✅
Frame 3: Person 40% (bị che) → TRACKED ID:1 ✅ (> 0.25)
Frame 4: Person 35% (vẫn che) → TRACKED ID:1 ✅ (> 0.25)
Frame 5: Person 38% (vẫn che) → TRACKED ID:1 ✅ (> 0.25)
Frame 50: Person 60% (hết che) → TRACKED ID:1 ✅ (giữ nguyên ID!)

Kết quả: Giữ ID:1 suốt quá trình, ổn định!
```

---

## Cấu Hình File

### File: `postprocess.h`

```cpp
#define BOX_THRESH 0.5           // Ngưỡng cao cho NEW detection
#define BOX_THRESH_TRACKING 0.25 // Ngưỡng thấp cho tracking hiện tại
```

### Khuyến nghị cấu hình:

#### 1. Môi trường sạch (indoor, ánh sáng tốt)
```cpp
#define BOX_THRESH 0.4           // Có thể thấp hơn
#define BOX_THRESH_TRACKING 0.2  // Cho phép tracking linh hoạt
```
- Ít noise → có thể dùng ngưỡng thấp hơn
- Tracking rất ổn định

#### 2. Môi trường tiêu chuẩn (outdoor, giám sát chung)
```cpp
#define BOX_THRESH 0.5           // Mặc định
#define BOX_THRESH_TRACKING 0.25 // Mặc định
```
- Cân bằng giữa false positive và tracking
- Phù hợp hầu hết trường hợp

#### 3. Môi trường nhiễu (outdoor, gió, bóng mờ)
```cpp
#define BOX_THRESH 0.6           // Cao để tránh noise
#define BOX_THRESH_TRACKING 0.3  // Vẫn thấp hơn để tracking
```
- Nhiều false positive → cần ngưỡng cao
- Vẫn giữ khoảng cách 0.3 giữa 2 ngưỡng

#### 4. Tracking cực kỳ ổn định (bãi đỗ xe, khu vực ít che khuất)
```cpp
#define BOX_THRESH 0.55          
#define BOX_THRESH_TRACKING 0.35 // Khoảng cách nhỏ hơn
```
- Object ít bị che → không cần ngưỡng quá thấp

#### 5. Che khuất liên tục (rừng cây, cột điện nhiều)
```cpp
#define BOX_THRESH 0.5           
#define BOX_THRESH_TRACKING 0.15 // Rất thấp để giữ track
```
- Che khuất thường xuyên → cần ngưỡng tracking rất thấp
- Risk: có thể giữ một số false positive lâu hơn

---

## Quy Tắc Vàng

### 1. Khoảng cách giữa 2 ngưỡng
```
Khuyến nghị: BOX_THRESH - BOX_THRESH_TRACKING = 0.2 đến 0.3

Ví dụ tốt:
✅ 0.5 và 0.25 (chênh 0.25)
✅ 0.6 và 0.3 (chênh 0.3)
✅ 0.4 và 0.15 (chênh 0.25)

Ví dụ không tốt:
❌ 0.5 và 0.45 (chênh 0.05) → Quá gần, tracking vẫn kém
❌ 0.5 và 0.1 (chênh 0.4) → Quá xa, giữ quá nhiều noise
```

### 2. Luôn luôn
```
BOX_THRESH > BOX_THRESH_TRACKING

Nếu bằng nhau hoặc ngược lại → không có tác dụng!
```

### 3. Điều chỉnh theo môi trường
- **Nhiều false positive** → Tăng BOX_THRESH (0.5 → 0.6)
- **Track bị mất liên tục** → Giảm BOX_THRESH_TRACKING (0.25 → 0.2)
- **ID thay đổi liên tục** → Tăng khoảng cách (ví dụ: 0.5 và 0.2)

---

## Cách Hoạt Động Trong Code

### File: `utils/tracker.cc`

#### Matching với existing tracks (dùng ngưỡng THẤP):
```cpp
// Line ~112: Khi tìm match cho track hiện tại
for (int j = 0; j < num_detections; j++) {
    if (detections[j].cls_id != track->cls_id) continue;
    
    // ✅ Dùng ngưỡng THẤP cho tracking
    if (detections[j].prop < BOX_THRESH_TRACKING) continue;
    
    float iou = calculate_iou(&track->box, &detections[j].box);
    // ... matching logic
}
```

#### Tạo NEW tracks (dùng ngưỡng CAO):
```cpp
// Line ~197: Khi tạo track mới
for (int j = 0; j < num_detections; j++) {
    if (det_matched[j]) continue;
    
    // ✅ Dùng ngưỡng CAO cho new track
    if (detections[j].prop < BOX_THRESH) continue;
    
    // Create new track...
}
```

---

## Kịch Bản Thực Tế

### Kịch bản 1: Người đi qua cây
```
BOX_THRESH = 0.5
BOX_THRESH_TRACKING = 0.25

Frame 1-10: Person visible, confidence 65%
           → NEW ID:1 (65% > 0.5) ✅

Frame 11-15: Walking behind tree, confidence drops to 35%
            → TRACKED ID:1 (35% > 0.25) ✅
            → Giữ track nhờ ngưỡng thấp!

Frame 16-20: Fully hidden, no detection
            → LOST_TEMP ID:1

Frame 21-30: Emerges from tree, confidence 40%
            → RE-TRACKED ID:1 (40% > 0.25) ✅
            → Tìm lại track nhờ ngưỡng thấp!

Frame 31+: Fully visible, confidence 60%
          → TRACKED ID:1 ✅

Kết quả: Giữ ID:1 suốt quá trình! 🎯
```

### Kịch bản 2: Shadow detection (false positive)
```
BOX_THRESH = 0.5
BOX_THRESH_TRACKING = 0.25

Frame 1: Shadow detected, confidence 30%
        → ❌ Không tạo track (30% < 0.5)
        → Lọc được false positive!

Frame 2-5: Shadow still detected, confidence 28-32%
          → ❌ Vẫn không tạo track
          → Tiếp tục lọc!

Kết quả: Không có ID rác từ shadow! ✅
```

### Kịch bản 3: Xe qua gờ giảm tốc (confidence fluctuates)
```
BOX_THRESH = 0.5
BOX_THRESH_TRACKING = 0.25

Frame 1-20: Car on road, confidence 70%
           → NEW ID:5 ✅

Frame 21-25: Car on speed bump, partially hidden
            Confidence drops: 45%, 38%, 32%, 40%, 48%
            → TRACKED ID:5 (all > 0.25) ✅
            → Giữ track qua gờ giảm tốc!

Frame 26+: Car back on road, confidence 68%
          → TRACKED ID:5 ✅

Kết quả: Smooth tracking qua obstacle! 🎯
```

---

## Testing & Debugging

### Xem log khi chạy:
```bash
./ff-rknn -f rtsp -i "rtsp://your-camera" \
          -x 960 -y 540 \
          -m ./model/RK3588/yolov8.rknn \
          -fps 5 -track true 2>&1 | grep -E "NEW|LOST|confidence"
```

### Dấu hiệu cần điều chỉnh:

#### Dấu hiệu 1: Nhiều "NEW" cho cùng 1 object
```
✨ ID 5: NEW
❌ ID 5: LOST_PERMANENTLY
✨ ID 12: NEW (cùng vị trí)
```
**Giải pháp**: Giảm `BOX_THRESH_TRACKING` (0.25 → 0.2)

#### Dấu hiệu 2: Quá nhiều ID được tạo
```
✨ ID 5: NEW
✨ ID 6: NEW
✨ ID 7: NEW (noise)
✨ ID 8: NEW (shadow)
```
**Giải pháp**: Tăng `BOX_THRESH` (0.5 → 0.6)

#### Dấu hiệu 3: Track chuyển LOST_TEMP → LOST_PERMANENT nhanh
```
⚠️  ID 5: LOST_TEMPORARILY
❌ ID 5: LOST_PERMANENTLY (sau 30 frames)
✨ ID 12: NEW (object quay lại)
```
**Giải pháp**: 
- Giảm `BOX_THRESH_TRACKING` (0.25 → 0.2)
- Hoặc tăng `MAX_LOST_FRAMES` (30 → 60)

---

## Bảng Tham Chiếu Nhanh

| Tình huống | BOX_THRESH | BOX_THRESH_TRACKING | Lý do |
|-----------|------------|---------------------|-------|
| Indoor, sạch | 0.4 | 0.2 | Ít noise, tracking linh hoạt |
| Outdoor, tiêu chuẩn | 0.5 | 0.25 | Cân bằng (mặc định) |
| Nhiều noise | 0.6 | 0.3 | Lọc noise mạnh |
| Che khuất nhiều | 0.5 | 0.15 | Giữ track qua occlusion |
| Bãi đỗ xe | 0.55 | 0.35 | Ít động, ít che |
| Traffic monitoring | 0.5 | 0.25 | Xe sau xe khác thường xuyên |
| Retail store | 0.45 | 0.2 | Người di chuyển nhanh |

---

## Tóm Tắt

### ✅ Ưu điểm của Dual Threshold:
1. **Giảm false positive**: Chỉ tạo NEW track với confidence cao
2. **Tracking ổn định**: Giữ track ngay cả khi confidence giảm tạm thời
3. **ID ổn định**: Ít thay đổi ID khi object bị che khuất
4. **Linh hoạt**: Dễ điều chỉnh theo môi trường

### 🎯 Khi nào cần điều chỉnh:
- **Quá nhiều ID mới** → Tăng `BOX_THRESH`
- **Track mất liên tục** → Giảm `BOX_THRESH_TRACKING`
- **ID thay đổi liên tục** → Tăng khoảng cách giữa 2 ngưỡng

### 📝 Nhớ rằng:
```
BOX_THRESH: Cái cổng vào (khó vào)
BOX_THRESH_TRACKING: Cái cổng ở lại (dễ ở)

→ Khó tạo ID mới, dễ giữ ID cũ!
```

---

**File liên quan**:
- Cấu hình: `postprocess.h`
- Logic: `utils/tracker.cc`
- Build: `./build.sh`
- Test: `./run.sh` hoặc `./ff-rknn -track true`

**Cập nhật**: October 17, 2025
