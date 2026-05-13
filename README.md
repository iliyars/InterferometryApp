# InterferometryApp

C++ приложение для обработки интерферограмм с GUI на MFC и ядром на OpenCV.

## Требования

- Visual Studio 2022 (или 2019)
- CMake 3.20+
- OpenCV 4.12.0
- Python 3.13 (для вспомогательных скриптов)
- Doxygen (опционально, для документации)

## Сборка

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
