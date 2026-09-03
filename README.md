# ESDR3_Player


Плеер IQ-записей ExpertSDR3 для Mac Silicon, Mac Intel, Win, Linux
A light player for ExpertSDR3 IQ for Mac Silicon, Mac Intel, Win, Linux


C панорамой, водопадом, зумом и приёмом в модах CW, USB, LSB, AM, FM. Самодостаточный: единственная внешняя зависимость это Qt, liquid-dsp лежит в дереве и собирается статически.
Comes with a panadapter, waterfall, zoom and CW/SSB/AM/FM audio. Self-contained: Qt is the only external dependency, liquid-dsp is vendored.

## Make

Qt 6.10.3, CMake 3.21+, компилятор C++20.
Целевая и единственная поддерживаемая ветка Qt: 6.10.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/Qt/6.10.3/macos
cmake --build build
```

## Docs

- [DOCS/user-guide.md](DOCS/user-guide.md): руководство пользователя (RU),
  [DOCS/user-guide.en.md](DOCS/user-guide.en.md) (EN).

## Состав и лицензии. Composition and Licenses

- `third_party/liquid-dsp`: liquid-dsp, MIT, Joseph Gaeddert.
- `src/ui/Palettes.cpp`: «Classic» based on CuteSDR/Gqrx,
  Simplified BSD, Moe Wheatley, Alexandru Csete OZ9AEC.
- The rest of code: GPL-3.0-or-later,
  [LICENSE](LICENSE).

## Download
[Release 1.0.0] (https://github.com/VK6NX/ESDR3-Player/releases)
