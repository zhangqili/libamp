# libamp

## Add to your project

```bash
git submodule add https://github.com/zhangqili/libamp.git
git submodule update --init --recursive
```

Make sure you have provided the keyboard_conf.h
```cmake
add_subdirectory(libamp)
# Add user defined header
target_include_directories(libamp PUBLIC
    ./libamp_user/
)
# Add linked libraries
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
