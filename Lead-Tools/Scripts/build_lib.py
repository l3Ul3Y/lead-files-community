import os
import platform
import subprocess
import sys
import json
import re

VERSION = "3.3.8"
SOURCE_TAR = f"mariadb-connector-c-{VERSION}-src.tar.gz"
SOURCE_URL = f"https://downloads.mariadb.com/Connectors/c/connector-c-{VERSION}/{SOURCE_TAR}"
SOURCE_DIR = f"mariadb-connector-c-{VERSION}-src"
BUILD_DIR = "build"

def run_command(cmd):
    print(f"Executing: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"Error executing command: {cmd}")
        sys.exit(1)

def force_patch_cmake(directory):
    """Finds CMakeLists.txt and forcefully changes VERSION to 3.5"""
    cmake_file = os.path.join(directory, "CMakeLists.txt")
    if os.path.exists(cmake_file):
        print(f"--- Patching {cmake_file} for CMake 4.0+ compatibility ---")
        with open(cmake_file, "r") as f:
            lines = f.readlines()
        
        with open(cmake_file, "w") as f:
            for line in lines:
                if "CMAKE_MINIMUM_REQUIRED" in line.upper():
                    f.write("CMAKE_MINIMUM_REQUIRED(VERSION 3.5)\n")
                else:
                    f.write(line)
        return True
    return False

def build_freebsd():
    print("--- Detected FreeBSD ---")
    run_command(["pkg", "install", "-y", "cmake", "openssl"])

    if not os.path.exists(SOURCE_TAR):
        run_command(["fetch", SOURCE_URL])
    if not os.path.exists(SOURCE_DIR):
        run_command(["tar", "-xf", SOURCE_TAR])

    force_patch_cmake(SOURCE_DIR)
    
    abs_build_path = os.path.join(os.getcwd(), SOURCE_DIR, BUILD_DIR)
    os.makedirs(abs_build_path, exist_ok=True)
    os.chdir(abs_build_path)
    
    cmake_cmd = [
        "cmake", "..",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_C_FLAGS=-m32 -Wno-deprecated-non-prototype",
        "-DCMAKE_CXX_FLAGS=-m32",
        "-DSTATIC_LIB=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DWITH_SSL=OPENSSL",
        "-DOPENSSL_ROOT_DIR=/usr/lib32",
        "-DOPENSSL_LIBRARIES=/usr/lib32/libssl.a;/usr/lib32/libcrypto.a",
        "-DOPENSSL_INCLUDE_DIR=/usr/include",
        "-DWITH_ZLIB=SYSTEM",
        "-DWITH_UNIT_TESTS=OFF",
        "-DWITH_MYSQLCOMPAT=ON"
    ]
    run_command(cmake_cmd)
    run_command(["make", "mariadbclient"])

def build_windows():
    print("--- Detected Windows ---")
    
    if not os.path.exists(SOURCE_TAR):
        run_command(["curl", "-L", "-o", SOURCE_TAR, SOURCE_URL])
    
    if not os.path.exists(SOURCE_DIR):
        run_command(["tar", "-xf", SOURCE_TAR])

    if not force_patch_cmake(SOURCE_DIR):
        print("Error: Could not find CMakeLists.txt to patch!")
        sys.exit(1)

    root_dir = os.getcwd()
    abs_source_path = os.path.join(root_dir, SOURCE_DIR)
    abs_build_path = os.path.join(abs_source_path, "build")
    
    if not os.path.exists(abs_build_path):
        os.makedirs(abs_build_path)
    
    os.chdir(abs_build_path)
    
    openssl_path = "C:/OpenSSL-Win32" 
    
    cmake_cmd = [
        "cmake", "..",
        "-A", "Win32",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DSTATIC_LIB=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DWITH_SSL=OPENSSL",
        "-DOPENSSL_ROOT_DIR=" + openssl_path,
        "-DOPENSSL_MSVC_STATIC_RT=ON",
        "-DWITH_ZLIB=BUNDLED",
        "-DWITH_UNIT_TESTS=OFF",
        "-DWITH_MYSQLCOMPAT=ON"
    ]
    
    run_command(cmake_cmd)
    run_command(["cmake", "--build", ".", "--config", "Release", "--target", "mariadbclient"])
    print(f"\nSuccess! Library should be in: {abs_build_path}/libmariadb/Release/mariadbclient.lib")

if __name__ == "__main__":
    os_name = platform.system()
    if os_name == "FreeBSD":
        if os.getuid() != 0:
            print("Please run as root/sudo.")
            sys.exit(1)
        build_freebsd()
    elif os_name == "Windows":
        build_windows()
    else:
        print(f"Unsupported OS: {os_name}")
        sys.exit(1)
