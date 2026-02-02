# Lưu đồ tổng quan dự án

```mermaid
flowchart TD
    A[Thu thập dữ liệu ảnh từ ESP32-CAM] --> B[Tiền xử lý & gán nhãn
4 lớp: forward/left/right/nothing]
    B --> C[Huấn luyện CNN
(MobileNet V1/LeNet/FOMO)]
    C --> D[Đánh giá mô hình
Accuracy, Confusion Matrix]
    D --> E[Triển khai lên ESP32-CAM
Arduino Library]
    E --> F[Inference thời gian thực
10–15 FPS]
    F --> G[Điều khiển robot
bám line & tránh vật cản]
```
