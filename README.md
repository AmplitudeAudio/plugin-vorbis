# plugin-vorbis

The official Amplitude Audio plugin that adds encoding/decoding of OGG/Vorbis files to your project.

## How to build

This plugin used [CMake](https://cmake.org) and [vcpkg](https://vcpkg.io). You may need to install them first before to build.

Configure the CMake project by giving the path to your Amplitude Audio SDK installation:

```bash
$ cmake -DCMAKE_TOOLCHAIN_FILE:STRING=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DAM_SDK_PATH:STRING=/path/to/sdk --no-warn-unused-cli -S/path/to/plugin-vorbis -B/path/to/plugin-vorbis/build -G Ninja
```

> Note: We recommend to use Ninja as the generator, but you can use the one you wish.

Once configured, you can build the project using:

```bash
$ cmake --build /path/to/plugin-vorbis/build --target all
```

Then, you can install the plugin in the SDK folder with:

```bash
$ cmake --build /path/to/plugin-vorbis/build --target install
```

## How to use

The plugin identifier is `AmplitudeVorbisCodecPlugin`, you will just have to use that name when loading the plugin through the engine:

```cpp
// Release builds
Engine::LoadPlugin(AM_OS_STRING("AmplitudeVorbisCodecPlugin"));

// Debug builds
Engine::LoadPlugin(AM_OS_STRING("AmplitudeVorbisCodecPlugin_d"));
```

## License

Licensed under the [Apache License 2.0](https://github.com/AmplitudeAudio/plugin-vorbis/blob/main/LICENSE).
