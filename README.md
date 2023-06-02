# libflowdrop

Experimental implementation of flowdrop in C++.

On Windows download and install Apple's [Bonjour SDK for Windows v3.0](https://raw.githubusercontent.com/FlowDrop/libflowdrop/master/redist/bonjoursdksetup.exe)

The source code is published under GPL-3.0 license with anti-commercial clause, the license is available [here](https://github.com/FlowDrop/libflowdrop/blob/master/LICENSE).

#### TODO:

- Get rid of temporary files, namely, to pack and unpack files on the fly
- Add Windows ARM support
- Reduce the weight and number of dependencies
- Add support for mobile operating systems
- Make a C header (currently has a C++ header)


## Authors

- **Michael Neonov** ([email](mailto:two.nelonn@gmail.com), [github](https://github.com/Nelonn))


## Third-party

* asio 1.28.0 ([Boost Software License 1.0](https://www.boost.org/LICENSE_1_0.txt))
* bzip2 1.0.8 ([bzip2 License](https://gitlab.com/bzip2/bzip2/-/blob/bzip2-1.0.8/LICENSE))
* curl 8.1.1 ([curl License](https://curl.se/docs/copyright.html))
* libarchive 3.6.2 ([New BSD License](https://raw.githubusercontent.com/libarchive/libarchive/master/COPYING))
* libhv 1.3.1 ([BSD 3-Clause License](https://github.com/ithewei/libhv/blob/v1.3.1/LICENSE))
* nlohmann_json 3.11.2 ([MIT License](https://github.com/nlohmann/json/blob/v3.11.2/LICENSE.MIT))
* liblzma 5.4.3 ([public domain](http://tukaani.org/xz/))
* zlib 1.2.11 ([zlib License](http://www.zlib.net/zlib_license.html))
* zstd 1.5.5 ([BSD License](https://github.com/facebook/zstd/blob/v1.5.5/LICENSE))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
