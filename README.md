# libamp

## Add to your project

```bash
git submodule add https://github.com/zhangqili/libamp.git
git submodule update --init --recursive
```

Make sure you have provided the keyboard_conf.h
```cmake
# Define user config header directory
# This folder contains keyboard_conf.h
set(LIBAMP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libamp_user)
# Then add subdirectory
add_subdirectory(libamp)
# Don't forget to link library
target_link_libraries(${CMAKE_PROJECT_NAME}
    libamp
)
```

## Test

```bash
git clone https://github.com/zhangqili/libamp.git --recursive
cd libamp
mkdir build
cd build
cmake .. -DLIBAMP_BUILD_TESTS=ON
make
make test
```
# Sample project 

<https://github.com/zhangqili/oholeo-keyboard-firmware>
<https://github.com/zhangqili/trinity-pad-firmware>
