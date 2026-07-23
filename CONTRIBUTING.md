# Contributing to NullA Browser

## Before You Start

- An account on one of our repositories:
  - [GitHub](https://github.com/EPLS-collective/NullA-Browser)
  - [Codeberg](https://codeberg.org/EPLS/NullA-Browser)
  - [Disroot](https://git.disroot.org/EPLS/NullA-Browser)
- Basic familiarity with the command line
- A working local build, see [BUILDING.md](BUILDING.md) if you haven't set one up yet.

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
