# deferShader

## Introduction

This code contains C++ and GLSL code for:

read HDF5 file - get depth, normal, mask, diffuseColor, mask and depth properties. 

apply lighting model (eg. Phong model) 

## Requirements

- [GLFW](https://learnopengl.com/Getting-started/Creating-a-window) Put GLFW include, lib and dll files in the correct location. My include file is in C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\include\GLFW, lib file is in C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\lib\x86, dll file is in C:\Windows\System32\
- [GLAD 0.1.33](https://learnopengl.com/Getting-started/Creating-a-window) Put the include file in the correct location. My GLAD include file is in C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\include\glad
- [GLM](https://learnopengl.com/Getting-started/Transformations) Put the include file in the correct location. My inlcude file is in C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\include\GL
- [HDF5 1.8.16 (32 BIT)](https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8/hdf5-1.8.16/bin/windows/hdf5-1.8.16-win32-vs2013-shared.zip) My install installation path is C:\Program Files (x86)\HDF_Group\HDF5\1.8.16. You need to configure in the Visual Studio project adding correct include and lib path.  

## How to run 

- Open the project with Visual Studio 2017
- Put the input HDF5 file in .\textureMapping\textureMapping\ (e.g. MPAS_000000_3.27890_20.000000_90.0000026563_90.0000026563.h5)
- compile and run, then the program will output "res.png" in .\textureMapping\textureMapping\