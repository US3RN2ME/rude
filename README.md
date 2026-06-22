# rude

`rude` is a C++23 project for a Reliable UDP Engine.

The project is currently in an early skeleton stage. It uses CMake, vcpkg, and Boost.Asio as the networking foundation.

## Requirements

- CMake 3.28 or newer
- A C++23-capable compiler
- vcpkg

## Dependencies

Runtime/build dependencies are managed through `vcpkg.json`:

- `boost-asio`

The optional test feature also pulls in:

- `bext-ut`

## Build

Configure and build with CMake:

```sh
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DRUDE_BUILD_TESTS=OFF \
  -DRUDE_BUILD_EXAMPLES=OFF
cmake --build build --parallel
```

By default, tests and examples are enabled.

## Tests

When tests are enabled, run them with:

```sh
ctest --test-dir build
```

## Project Layout

- `include/rude/` - public headers
- `src/` - implementation sources
- `examples/` - example programs
- `tests/` - test targets
- `cmake/` - CMake helper files and generated version header template

## License

MIT. See `LICENSE` for details.