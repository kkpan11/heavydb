#!/usr/bin/env python
#
# This file (setup.in.py) is a template, which cmakes uses to generate setup.py
# in build directory where it can be used for installation into python environment.

from Cython.Build import cythonize
from distutils.core import setup, Extension

import os
import sys
import numpy as np
import pyarrow as pa

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# conda-forge packages heavydbe and pypyheavydbe are built
# separately. HEAVYDB_ROOT_PATH is defined by the heavydbe activate
# script that determines the location of libDBEngine.so and is
# required for linking Python extension module heavydbe.
extra_library_dirs = []
if 'HEAVYDB_ROOT_PATH' in os.environ:
    extra_library_dirs.append(os.path.join(os.environ['HEAVYDB_ROOT_PATH'], 'lib'))

dbe = Extension(
    "heavydbe",
    ["@CMAKE_CURRENT_SOURCE_DIR@/Python/dbe.pyx"],
    language="c++17",
    include_dirs=[
        np.get_include(),
        pa.get_include(),
        root,
        "@CMAKE_SOURCE_DIR@",
        "@CMAKE_CURRENT_SOURCE_DIR@",
        "@CMAKE_SOURCE_DIR@/ThirdParty/rapidjson",
        "@CMAKE_SOURCE_DIR@/Distributed/os",
    ],
    library_dirs=pa.get_library_dirs() + ["@CMAKE_CURRENT_BINARY_DIR@", "."] + extra_library_dirs,
    runtime_library_dirs=pa.get_library_dirs() + ["$ORIGIN/../../"] + extra_library_dirs,
    libraries=pa.get_libraries() + ["DBEngine", "boost_system"],
    extra_compile_args=["-std=c++17", "-DRAPIDJSON_HAS_STDSTRING"],
)
# Try uncommenting the following line on Linux
# if you get weird linker errors or runtime crashes
#    dbe.define_macros.append(("_GLIBCXX_USE_CXX11_ABI", "0"))

# "fat" wheel
data_files = []
if False:  # TODO: implement an option?
    data_files = [
        ("lib", ["$<TARGET_FILE:DBEngine>"]),
        (
            "bin",
            ["@CMAKE_BINARY_DIR@/bin/calcite-1.0-SNAPSHOT-jar-with-dependencies.jar"],
        ),
        (
            "QueryEngine",
            [
                "@CMAKE_BINARY_DIR@/QueryEngine/RuntimeFunctions.bc",
                "@CMAKE_BINARY_DIR@/QueryEngine/ExtensionFunctions.ast",
            ],
        ),
        (
            "include",
            [
                "@CMAKE_CURRENT_SOURCE_DIR@/DBEngine.h",
                "@CMAKE_CURRENT_SOURCE_DIR@/DBEngine.pxd",
            ],
        ),
    ]

setup(
    name="heavydbe",
    version="0.1",
    ext_modules=cythonize(
        dbe,
        compiler_directives={
            "c_string_type": "str",
            "c_string_encoding": "utf8",
            "language_level": "3",
        },
        include_path=[
            "@CMAKE_CURRENT_SOURCE_DIR@",
            "@CMAKE_CURRENT_SOURCE_DIR@/Python",
        ],
    ),
    data_files=data_files,
)
