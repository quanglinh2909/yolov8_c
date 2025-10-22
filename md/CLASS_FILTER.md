# Class Filter - Lọc Object Detection

## Cách Sử Dụng

Để giới hạn chỉ detect một hoặc nhiều loại object cụ thể, sửa file `postprocess.h`:

```cpp
static const int DETECT_CLASS_FILTER[] = {};  // Giá trị hiện tại
```

## Giá Trị Cấu Hình

### Detect Tất Cả (Mặc định)
```cpp
static const int DETECT_CLASS_FILTER[] = {};  // Mảng rỗng = detect all
```

### Chỉ Detect Người (Person Only)
```cpp
static const int DETECT_CLASS_FILTER[] = {0};  // Chỉ person
```

### Detect Người VÀ Xe Hơi (Person + Car)
```cpp
static const int DETECT_CLASS_FILTER[] = {0, 2};  // person, car
```

### Detect Người VÀ Tất Cả Phương Tiện (Person + All Vehicles)
```cpp
static const int DETECT_CLASS_FILTER[] = {0, 2, 3, 5, 7};  
// person, car, motorcycle, bus, truck
```

### Chỉ Detect Các Phương Tiện (Vehicles Only)
```cpp
static const int DETECT_CLASS_FILTER[] = {2, 3, 5, 7};  
// car, motorcycle, bus, truck
```

### Detect Người VÀ Động Vật (Person + Animals)
```cpp
static const int DETECT_CLASS_FILTER[] = {0, 14, 15, 16, 17, 18, 19};  
// person, bird, cat, dog, horse, sheep, cow
```

## COCO Dataset Class IDs (80 classes)

| Class ID | Tên Class       | Mô tả                    |
|----------|-----------------|--------------------------|
| 0        | person          | Người                    |
| 1        | bicycle         | Xe đạp                   |
| 2        | car             | Xe hơi                   |
| 3        | motorcycle      | Xe máy                   |
| 4        | airplane        | Máy bay                  |
| 5        | bus             | Xe buýt                  |
| 6        | train           | Tàu hỏa                  |
| 7        | truck           | Xe tải                   |
| 8        | boat            | Thuyền                   |
| 9        | traffic light   | Đèn giao thông           |
| 10       | fire hydrant    | Trụ cứu hỏa              |
| 11       | stop sign       | Biển báo dừng            |
| 12       | parking meter   | Đồng hồ đỗ xe            |
| 13       | bench           | Ghế băng                 |
| 14       | bird            | Chim                     |
| 15       | cat             | Mèo                      |
| 16       | dog             | Chó                      |
| 17       | horse           | Ngựa                     |
| 18       | sheep           | Cừu                      |
| 19       | cow             | Bò                       |
| 20       | elephant        | Voi                      |
| 21       | bear            | Gấu                      |
| 22       | zebra           | Ngựa vằn                 |
| 23       | giraffe         | Hươu cao cổ              |
| 24       | backpack        | Ba lô                    |
| 25       | umbrella        | Ô/Dù                     |
| 26       | handbag         | Túi xách                 |
| 27       | tie             | Cà vạt                   |
| 28       | suitcase        | Vali                     |
| 29       | frisbee         | Đĩa bay                  |
| 30       | skis            | Ván trượt tuyết          |
| 31       | snowboard       | Ván trượt tuyết lớn      |
| 32       | sports ball     | Bóng thể thao            |
| 33       | kite            | Diều                     |
| 34       | baseball bat    | Gậy bóng chày            |
| 35       | baseball glove  | Găng tay bóng chày       |
| 36       | skateboard      | Ván trượt                |
| 37       | surfboard       | Ván lướt sóng            |
| 38       | tennis racket   | Vợt tennis               |
| 39       | bottle          | Chai/Lọ                  |
| 40       | wine glass      | Ly rượu                  |
| 41       | cup             | Cốc/Ly                   |
| 42       | fork            | Nĩa                      |
| 43       | knife           | Dao                      |
| 44       | spoon           | Thìa                     |
| 45       | bowl            | Bát/Tô                   |
| 46       | banana          | Chuối                    |
| 47       | apple           | Táo                      |
| 48       | sandwich        | Bánh sandwich            |
| 49       | orange          | Cam                      |
| 50       | broccoli        | Bông cải xanh            |
| 51       | carrot          | Cà rốt                   |
| 52       | hot dog         | Xúc xích                 |
| 53       | pizza           | Pizza                    |
| 54       | donut           | Bánh donut               |
| 55       | cake            | Bánh ngọt                |
| 56       | chair           | Ghế                      |
| 57       | couch           | Ghế sofa                 |
| 58       | potted plant    | Cây chậu                 |
| 59       | bed             | Giường                   |
| 60       | dining table    | Bàn ăn                   |
| 61       | toilet          | Bồn cầu                  |
| 62       | tv              | TV                       |
| 63       | laptop          | Laptop                   |
| 64       | mouse           | Chuột máy tính           |
| 65       | remote          | Điều khiển từ xa         |
| 66       | keyboard        | Bàn phím                 |
| 67       | cell phone      | Điện thoại               |
| 68       | microwave       | Lò vi sóng               |
| 69       | oven            | Lò nướng                 |
| 70       | toaster         | Máy nướng bánh mì        |
| 71       | sink            | Bồn rửa                  |
| 72       | refrigerator    | Tủ lạnh                  |
| 73       | book            | Sách                     |
| 74       | clock           | Đồng hồ                  |
| 75       | vase            | Bình hoa                 |
| 76       | scissors        | Kéo                      |
| 77       | teddy bear      | Gấu bông                 |
| 78       | hair drier      | Máy sấy tóc              |
| 79       | toothbrush      | Bàn chải đánh răng       |

## Ví Dụ Thường Dùng

### 1. Đếm người (People Counter)
```cpp
static const int DETECT_CLASS_FILTER[] = {0};  // person
```

### 2. Giám sát giao thông - Người + Xe (Traffic + Pedestrian)
```cpp
static const int DETECT_CLASS_FILTER[] = {0, 2, 3, 5, 7};  
// person, car, motorcycle, bus, truck
```

### 3. Bãi đỗ xe - Chỉ xe hơi (Parking - Cars Only)
```cpp
static const int DETECT_CLASS_FILTER[] = {2};  // car only
```

### 4. Giám sát an ninh - Người + Xe (Security - Person + Vehicle)
```cpp
static const int DETECT_CLASS_FILTER[] = {0, 2, 7};  
// person, car, truck
```

### 5. Đếm khách hàng trong shop (Customer Counting)
```cpp
static const int DETECT_CLASS_FILTER[] = {0};  // person
```

### 6. Kiểm soát thú cưng (Pet Detection)
```cpp
static const int DETECT_CLASS_FILTER[] = {15, 16};  // cat, dog
```

### 7. Phát hiện chim (Bird Detection)
```cpp
static const int DETECT_CLASS_FILTER[] = {14};  // bird
```

## Sau Khi Thay Đổi

1. Sửa `postprocess.h`
2. Build lại:
   ```bash
   ./build.sh
   ```
3. Chạy:
   ```bash
   ./run.sh
   ```

## Lưu Ý

- **Mảng rỗng `{}`** = Detect tất cả 80 classes
- **Có giá trị trong mảng** = Chỉ detect các class trong mảng
- Có thể thêm bao nhiêu class tùy thích: `{0, 2, 3, 5, 7, 14, 15, 16}`
- Filter này hoạt động **sau NMS**, nên vẫn tiết kiệm hiệu năng
- Tracking vẫn hoạt động bình thường với các class được filter

## Cú Pháp Mảng C

```cpp
// Chỉ 1 class
static const int DETECT_CLASS_FILTER[] = {0};

// Nhiều classes - có thể xuống dòng cho dễ đọc
static const int DETECT_CLASS_FILTER[] = {
    0,   // person
    2,   // car
    3,   // motorcycle
    5,   // bus
    7    // truck
};

// Tất cả classes
static const int DETECT_CLASS_FILTER[] = {};
```
