# Compilation

I'll only cover the build process on Linux since I will be providing Windows and macOS binaries and that building trojan on every platform is similar.

## Dependencies

Install these dependencies before you build:

- [CMake](https://cmake.org/) >= 2.8.12
- [Boost](http://www.boost.org/) >= 1.53.0
- [OpenSSL](https://www.openssl.org/) >= 1.0.2

## Clone

Type in

```bash
git clone https://github.com/GreaterFire/trojan.git
cd trojan/
git checkout stable
```

to clone the project and go into the directory and change to `stable` branch.

## Build and Install

Type in

```bash
mkdir build
cd build/
cmake ..
make
sudo make install
```

to build and install trojan. If everything goes well you'll be able to use trojan.

[Homepage](.) | [Prev Page](config) | [Next Page](usage)
