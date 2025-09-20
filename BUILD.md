# Windows build

```bash
cmake -Bbuild_x64 --preset windows-x64 -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build build_x64 --preset windows-x64
```