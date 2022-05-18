# kwebp
webp module support for kangle web server.
## usage
* download and install libwebp
```
git clone https://github.com/webmproject/libwebp
cd libwebp
mkdir build && cd build
cmake .. -D CMAKE_C_FLAGS=-fPIC
make
sudo make install
```
* build kwebp use cmake
```
git clone https://github.com/keengo99/kwebp
cd kwebp
mkdir build && cd build
cmake .. -D KANGLE_DIR=kangle_installed_dir -D LIBWEBP_DIR=libwebp_dir
make
make install
```
