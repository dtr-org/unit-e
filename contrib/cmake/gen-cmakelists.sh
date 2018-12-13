#!/usr/bin/env bash

if [[ "$OSTYPE" == "darwin"* ]]; then
  FIND_CMD='find -E'
  QT_PATH=$(brew info qt 2>/dev/null | head -n4 | tail -n1 | cut -f1 -d' ')
  QT_CMAKE_PATH="${QT_PATH}/lib/cmake"
  OPENSSL_INCLUDE=/usr/local/opt/openssl/include
  OPENSSL_LIB=/usr/local/opt/openssl/lib
else
  FIND_CMD='find'
  QT_CMAKE_PATH=/usr/lib/x86_64-linux-gnu/cmake
  OPENSSL_INCLUDE=/usr/include/openssl
  OPENSSL_LIB=/usr/lib/x86_64-linux-gnu
fi

cd "$(dirname "${BASH_SOURCE[0]}")/../.."

(
echo "cmake_minimum_required(VERSION 3.12)"
echo "project(unit_e)"
echo ""
echo "set(CMAKE_CXX_STANDARD 11)"
echo ""
echo "add_definitions(-DHAVE_CONFIG_H)"
echo ""

echo "include_directories($OPENSSL_INCLUDE)"
echo "link_directories($OPENSSL_LIB)"
echo ""

echo "set(CMAKE_PREFIX_PATH $QT_CMAKE_PATH)"
echo ""
echo "find_package(Qt5Core REQUIRED)"
echo "find_package(Qt5Widgets REQUIRED)"
echo ""

echo "find_package(Boost COMPONENTS"
echo "        system"
echo "        filesystem"
echo "        unit_test_framework"
echo "        REQUIRED)"
echo ""

echo 'include_directories(${Qt5Widgets_INCLUDE_DIRS})'
echo 'include_directories(${Boost_INCLUDE_DIRS})'

echo "include_directories(src)"
$FIND_CMD src -type d -regex '.+/[^/.]+' | awk '{ print "include_directories(" $0 ")" }'

echo ""

echo "add_executable(unit_e"
$FIND_CMD src -type f \( -name "*.cpp" -or -name "*.hpp" -or -name "*.c" -or -name "*.h" \) \
    | awk '{ print "      " $0 }'
echo ")"
) > CMakeLists.txt

