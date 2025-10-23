# 🧩 MỤC TIÊU

Cài SDL3 từ file SDL3_portable.tar.gz (đã build sẵn từ máy khác) lên máy hiện tại.

✅ CÁC BƯỚC CỤ THỂ

1️⃣ — Tạo thư mục chứa và giải nén
mkdir -p ~/SDL3
tar xzf SDL3_portable.tar.gz -C ~/SDL3

📁 Sau khi giải nén, bạn sẽ có:

~/SDL3/
├── include/
├── libSDL3.so
├── libSDL3.so.0
├── libSDL3.so.0.3.0
└── sdl3.pc

2️⃣ — Copy thư viện vào hệ thống

Cần quyền sudo để cài vào /usr/local:

sudo mkdir -p /usr/local/include
sudo mkdir -p /usr/local/lib
sudo mkdir -p /usr/local/lib/pkgconfig

Rồi copy các file vào:

sudo cp -r ~/SDL3/include /usr/local/include/
sudo cp ~/SDL3/libSDL3*.so /usr/local/lib/
sudo cp ~/SDL3/sdl3.pc /usr/local/lib/pkgconfig/

3️⃣ — Cập nhật cache của linker
sudo ldconfig

4️⃣ — Kiểm tra cài đặt thành công

Kiểm tra file .so:
ls /usr/local/lib | grep libSDL3

Kiểm tra gói pkg-config:
pkg-config --modversion sdl3

👉 Nếu ra ví dụ 3.0.0 là OK ✅

5️⃣ — Nếu pkg-config chưa thấy SDL3

Có thể bạn cần thêm đường dẫn:
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

Bạn có thể thêm dòng này vào cuối file ~/.bashrc để tự động áp dụng:
echo 'export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
source ~/.bashrc

6️⃣ — Kiểm tra thử với build

Bây giờ bạn có thể build lại chương trình:
./build.sh

Nếu hết lỗi "No package 'sdl3' found" thì SDL3 đã cài thành công 🎉

sudo apt update
sudo apt install -y \
  libvorbis-dev libopus-dev libogg-dev libmp3lame-dev librtmp-dev libbz2-dev \
  libxml2-dev libxv-dev libxext-dev libasound2-dev libpulse-dev libsndio-dev \
  libxcb-shm0-dev libxcb-xfixes0-dev libxcb-render0-dev libxcb-shape0-dev

