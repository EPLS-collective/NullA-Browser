# Contributing to NullA Browser

Some parts of this process, including cloning and compiling, can take time depending on your hardware. If you get stuck, open an issue with the `question` tag.

---

## Before You Start

- An account on one of our repositories:
  - [GitHub](https://github.com/EPLS-collective/NullA-Browser)
  - [Codeberg](https://codeberg.org/EPLS/NullA-Browser)
  - [Disroot](https://git.disroot.org/EPLS/NullA-Browser)
- Basic familiarity with the command line

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

```./build/bin/NullA```

On Windows: ```build\bin\NullA.exe```

---

## Contributing

We welcome contributions of any kind. Code, bug reports, translations, design feedback all of it helps.

If you're not sure where to start, open an issue and ask.

---

## To Submit a Patch

1. Fork the repository.
2. Create a branch: ```git checkout -b fix/your-fix```
3. Make your changes.
4. Commit: ```git commit -m "fix: short description"```
5. Push and open a Pull Request.

Keep changes small and focused. One issue per PR.

---

## To Update a Submitted Patch

Make your changes, then:

```
git add .
git commit --amend
git push --force
```

The PR will update automatically.

---

## Coding Style

- Follow Qt conventions
- Use ```override``` for virtual functions
- Keep ```Browser.cpp``` from growing too large

To check style locally:

```cmake --build . --target clang-format```

---

## Questions?

Open an issue with the ```question``` tag.

---

## Principles

NullA is not neutral. Contributions that serve surveillance, advertising networks, or corporate data harvesting will be rejected without discussion.

This browser exists for users, not for profit
