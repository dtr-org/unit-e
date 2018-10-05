#!/usr/bin/env bash

cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )/../.."

(
echo "cmake_minimum_required(VERSION 3.12)"
echo "project(unit_e)"
echo ""
echo "set(CMAKE_CXX_STANDARD 11)"
echo ""
echo "add_definitions(-DHAVE_CONFIG_H)"
echo ""

echo "include_directories(/usr/local/opt/openssl/include)"
echo "link_directories(/usr/local/opt/openssl/lib)"
echo ""

QT_PATH=$(brew info qt 2>/dev/null | head -n4 | tail -n1 | cut -f1 -d' ')
echo "set(CMAKE_PREFIX_PATH $QT_PATH/lib/cmake)"
echo ""
echo "find_package(Qt5Core REQUIRED)"
echo "find_package(Qt5Widgets REQUIRED)"
echo "find_package(Qt5Quick REQUIRED)"
echo ""

echo "find_package(Boost COMPONENTS"
echo "        system"
echo "        filesystem"
echo "        unit_test_framework"
echo "        REQUIRED)"
echo ""
echo 'include_directories(${Boost_INCLUDE_DIRS})'

echo "include_directories(src)"
find -E src -type d -regex '.+/[^/.]+' | awk '{ print "include_directories(" $0 ")" }'

echo ""

echo "add_executable(unit_e"
find -E src -type f -regex '.+\.[ch](pp)?' | awk '{ print "      " $0 }'
echo ")"
) > CMakeLists.txt

