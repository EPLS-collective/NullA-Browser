# Building NullA Browser

This guide covers building NullA Browser from source. If you want to contribute code, see [CONTRIBUTING.md](CONTRIBUTING.md) as well.

Some parts of this process, including cloning and compiling, can take time depending on your hardware.

---

## Dependencies

### Windows

- Windows 10 or 11
- Qt 6.5 or later (with Core, WebEngine and Multimedia)
- CMake 3.20 or later
- A C++17 compatible compiler (MSVC or MinGW, typically included with Qt)

### Linux

- Qt 6.5 or later (Core, WebEngine and Multimedia)
- CMake 3.20 or later
- GCC 11+

---

## Getting the Source

```
git clone https://github.com/EPLS-collective/NullA-Browser.git
cd NullA-Browser
```

---

## Building

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

The executable will be in ```build/bin/```.

### Build Options

- ```-DCMAKE_BUILD_TYPE=Release``` for optimized build
- ```-DCMAKE_PREFIX_PATH=/path/to/Qt``` if Qt is not in PATH

---

## To Run

On Linux: ```./build/bin/NullA```

On Windows: ```build\bin\NullA.exe```

---

## Questions?

Open an issue with the `question` tag.
