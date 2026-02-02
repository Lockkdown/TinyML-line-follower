# Tổng Quan Dự Án: Ứng Dụng CNN Trong Phân Loại Hình Ảnh Đường Đi Trên Thiết Bị Nhúng

## 1. Thông Tin Cơ Bản

| Mục | Nội dung |
|-----|----------|
| **Tên đề tài** | Ứng Dụng CNN Trong Phân Loại Hình Ảnh Đường Đi Trên Thiết Bị Nhúng |
| **Loại bài toán** | Image Classification (Phân loại ảnh) |
| **Mô tả mục tiêu** | Xây dựng hệ thống phân loại ảnh đường đi sử dụng mạng CNN. Ứng dụng demo điều hướng robot dò line theo thời gian thực. |

---

## 2. Flow Tổng Quan Dự Án

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FLOW DỰ ÁN TỔNG QUAN                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌────────┐ │
│  │ Thu thập │    │  Train   │    │  Deploy  │    │ Inference│    │  Demo  │ │
│  │ Dataset  │ -> │  Model   │ -> │  Model   │ -> │ Realtime │ -> │ Robot  │ │
│  │ (ảnh)    │    │  (CNN)   │    │ (ESP32)  │    │ (Camera) │    │ Dò Line│ │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘    └────────┘ │
│       │               │               │               │               │     │
│       ▼               ▼               ▼               ▼               ▼     │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌────────┐ │
│  │ 4 class: │    │ Edge     │    │ Arduino  │    │ 10-15    │    │ Robot  │ │
│  │ forward  │    │ Impulse  │    │ Library  │    │ FPS      │    │ tự đi  │ │
│  │ left     │    │ hoặc     │    │ .zip     │    │ classify │    │ theo   │ │
│  │ right    │    │ Python   │    │ upload   │    │ mỗi      │    │ line   │ │
│  │ nothing  │    │ training │    │ ESP32-CAM│    │ frame    │    │ đen    │ │ 
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘    └────────┘ │ 
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Chi Tiết Kỹ Thuật

### 3.1. Input / Output

| | Mô tả |
|--|-------|
| **Input** | Ảnh RGB/Grayscale từ ESP32-CAM (96x96 hoặc 48x48 pixels) |
| **Output** | 1 trong 4 nhãn: `forward`, `left`, `right`, `nothing` + confidence score |
| **Tốc độ** | Real-time ~10-15 FPS |

### 3.2. Ý Nghĩa Các Class

| Class | Ý nghĩa | Hành động robot |
|-------|---------|-----------------|
| `forward` | Line ở giữa khung hình | Đi thẳng |
| `left` | Line lệch sang phải | Rẽ trái |
| `right` | Line lệch sang trái | Rẽ phải |
| `nothing` | Không thấy line | Dừng / tìm line |

---

## 4. Thiết Bị Phần Cứng

### 4.1. Danh Sách Linh Kiện

| STT | Linh kiện | Giá (VND) | Link tham khảo |
|-----|-----------|-----------|----------------|
| 1 | Bộ kit xe 3 bánh (khung + motor + L298N + HC-SR04) | 485,000 | [linhkientot.vn](https://www.linhkientot.vn/products/bo-xe-robot-3-banh-tranh-vat-can-kem-code-mau-va-huong-dan-lap-rap) |
| 2 | ESP32-CAM AI-Thinker | 85,000 - 100,000 | Shopee / nshop.com.vn |
| 3 | Module FTDI FT232RL (nạp code) | 25,000 - 35,000 | Shopee / nshop.com.vn |
| 4 | Pin 18650 x2 | 40,000 - 50,000 | Shopee |
| 5 | Đế pin 2 cell | 15,000 - 20,000 | Shopee |
| 6 | Sạc pin 18650 | 25,000 - 35,000 | Shopee |
| 7 | TCS3200 (cảm biến màu - optional) | 40,000 - 50,000 | Shopee |
| 8 | Buzzer 5V | 5,000 - 10,000 | Shopee |
| 9 | Dây điện, ốc vít, băng keo | 20,000 - 30,000 | Tiệm điện tử |

**Tổng ngân sách ước tính: ~750,000 - 850,000 VND**

### 4.2. Vai Trò Thiết Bị Chính

| Thiết bị | Vai trò |
|----------|---------|
| **ESP32-CAM** | Bộ não chính - chụp ảnh, chạy CNN model, điều khiển motor |
| **L298N** | Driver điều khiển 2 motor DC |
| **HC-SR04** | Cảm biến siêu âm tránh vật cản |
| **Motor DC x2** | Di chuyển robot |

---

## 5. Dataset

### 5.1. Yêu Cầu Dataset

| Tiêu chí | Yêu cầu |
|----------|---------|
| Số class | 4 (forward, left, right, nothing) |
| Số ảnh tối thiểu | 800 ảnh (200 ảnh/class) |
| Số ảnh khuyến nghị | 1000-1400 ảnh (250-350 ảnh/class) |
| Kích thước ảnh | 96x96 hoặc 48x48 pixels |
| Định dạng | JPEG/PNG |

### 5.2. Phương Pháp Thu Thập

**Tự tạo dataset** (khuyến nghị):
- Chụp trực tiếp từ ESP32-CAM
- Đa dạng hóa: ánh sáng, góc độ, nền
- Dùng Data Augmentation để tăng số lượng

**Kỹ thuật Data Augmentation:**
- Brightness adjustment (sáng/tối)
- Blur (làm mờ)
- Noise (thêm nhiễu)
- Rotation (xoay ±5-10°)
- Horizontal flip (lật ngang - **phải đổi label**: left ↔ right)

### 5.3. Cấu Trúc Thư Mục Dataset

```
dataset/
├── forward/
│   ├── img_001.jpg
│   ├── img_002.jpg
│   └── ...
├── left/
│   ├── img_001.jpg
│   └── ...
├── right/
│   ├── img_001.jpg
│   └── ...
└── nothing/
    ├── img_001.jpg
    └── ...
```

---

## 6. Model CNN

### 6.1. Các Model Phù Hợp ESP32-CAM

| Model | RAM cần | Phù hợp ESP32-CAM | Ghi chú |
|-------|---------|-------------------|---------|
| **MobileNet V1 0.25** | ~1-2 MB | ✅ Có | Khuyến nghị dùng |
| **MobileNet V2 0.35** | ~2-3 MB | ✅ Có | Nặng hơn một chút |
| **Custom LeNet-5** | ~500 KB | ✅ Có | Nhẹ, đơn giản |
| **FOMO (Edge Impulse)** | ~50-100 KB | ✅ Rất tốt | Siêu nhẹ |
| YOLOv8 nano | ~6 MB | ❌ Không | Quá nặng |
| Instance Segmentation | ~10-50 MB | ❌ Không | Quá nặng |

### 6.2. Platform Training

| Platform | Ưu điểm | Link |
|----------|---------|------|
| **Edge Impulse** (khuyến nghị) | Miễn phí, dễ dùng, export Arduino library | [edgeimpulse.com](https://edgeimpulse.com) |
| TensorFlow Lite | Linh hoạt, cần code nhiều | [tensorflow.org](https://www.tensorflow.org/lite) |
| PyTorch + ONNX | Linh hoạt, cần convert model | [pytorch.org](https://pytorch.org) |

### 6.3. Thông Số Model Khuyến Nghị (Edge Impulse)

```
Image size: 96x96
Color depth: Grayscale (tiết kiệm RAM) hoặc RGB
Model: MobileNet V1 0.25
Transfer learning: Enabled
Training cycles: 20-50
Learning rate: 0.001
```

---

## 7. Quy Trình Thực Hiện (Workflow)

### Phase 1: Chuẩn Bị (Tuần 1-2)
- [ ] Mua linh kiện
- [ ] Lắp ráp khung xe
- [ ] Cài đặt Arduino IDE + ESP32 board
- [ ] Test motor, camera hoạt động

### Phase 2: Thu Thập Dataset (Tuần 3)
- [ ] Dán line test trên sàn
- [ ] Chụp 200-400 ảnh từ ESP32-CAM
- [ ] Phân loại ảnh vào 4 folder
- [ ] Áp dụng data augmentation

### Phase 3: Training Model (Tuần 4)
- [ ] Upload dataset lên Edge Impulse
- [ ] Tạo Impulse (Image + Transfer Learning)
- [ ] Train model
- [ ] Đánh giá accuracy, confusion matrix
- [ ] Export Arduino Library

### Phase 4: Deploy & Test (Tuần 5-6)
- [ ] Upload model lên ESP32-CAM
- [ ] Test phân loại real-time
- [ ] Tích hợp điều khiển motor
- [ ] Tinh chỉnh, debug

### Phase 5: Demo & Báo Cáo (Tuần 7-8)
- [ ] Test toàn bộ hệ thống
- [ ] Quay video demo
- [ ] Viết báo cáo
- [ ] Chuẩn bị thuyết trình

---

## 8. Tài Liệu Tham Khảo

### Video Tutorial
- [DroneBot Workshop - ESP32-CAM Object Detection](https://dronebotworkshop.com/esp32-object-detect/) - Hướng dẫn A-Z Edge Impulse + ESP32-CAM

### Documentation
- [Edge Impulse - Image Classification](https://docs.edgeimpulse.com/docs/tutorials/end-to-end-tutorials/computer-vision/image-classification)
- [ESP32-CAM + Edge Impulse Example](https://github.com/edgeimpulse/example-esp32-cam)

### Research Paper
- [CNN for Line Following Robot (MDPI 2025)](https://www.mdpi.com/1999-4893/18/1/51)

---

## 9. Lưu Ý Quan Trọng

1. **ESP32-CAM không chạy được YOLO** - chỉ dùng MobileNet/LeNet/FOMO
2. **Dataset phải tự tạo** - mỗi robot có góc camera khác nhau
3. **Robot chỉ là platform demo** - trọng tâm là CNN + Image Classification
4. **Cần module FTDI** để nạp code cho ESP32-CAM (không có USB onboard)
5. **Backup plan**: Nếu ML không đạt, vẫn có thể chuyển sang dùng cảm biến IR truyền thống

---

