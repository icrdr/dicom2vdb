## Getting Started

1. install Visual Studio Community
   https://visualstudio.microsoft.com/downloads/
   Then install C++ IDE package

2. install cmake
   https://cmake.org/

3. install vcpkg
   https://vcpkg.io/en/getting-started.html

4. add env to PATH

```
path\to\vcpkg
path\to\vcpkg\installed\x64-windows\bin
```

In order to use vcpkg with CMake outside of an IDE, you can use the toolchain file:
in vscode setting.json

```json
"cmake.configureSettings": {
  "CMAKE_TOOLCHAIN_FILE":"path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
}
```
reboot your system OS! (maybe)

5. install openvdb
```shell
set VCPKG_DEFAULT_TRIPLET=x64-windows
vcpkg install openvdb
vcpkg install itk
```

6. install 

path\to\sciter-sdk\bin\packfolder.exe ui resources.cpp -v "resources"