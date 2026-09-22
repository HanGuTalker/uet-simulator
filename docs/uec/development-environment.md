# Development environment

## Pinned environment

- Host: Windows 11 with WSL2.
- Distribution: Ubuntu 26.04.1 LTS.
- Kernel: Microsoft WSL2 6.18 series.
- Compiler: GCC 15.2.
- Language mode: the ns-3.47 default, C++23.
- CMake: 4.2.3.
- Ninja: 1.13.2.
- Python: 3.14.4.
- ns-3: 3.47.

The source tree is shared at:

```text
Windows: D:\Work\CodeX_WorkSpace\UEC协议仿真
WSL:     /mnt/d/Work/CodeX_WorkSpace/UEC协议仿真
```

## Commands

Run from the repository root in Ubuntu:

```bash
bash scripts/uec/configure.sh
bash scripts/uec/build.sh
bash scripts/uec/test.sh
```

Or run the complete verification sequence:

```bash
bash scripts/uec/check.sh
```

Only the `uet` module and its required ns-3 dependencies are selected by the development
configuration. Full-tree builds are reserved for release integration checks.
