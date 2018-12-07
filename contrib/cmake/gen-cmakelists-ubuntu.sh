#!/usr/bin/env bash

cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )/../.."

(
echo "cmake_minimum_required(VERSION 3.12)"
echo "project(unite_e)"
echo ""
echo "set(CMAKE_CXX_STANDARD 11)"
echo ""
echo "add_definitions(-DHAVE_CONFIG_H)"
echo ""

echo "include_directories(/usr/include/openssl)"
echo "link_directories(/usr/lib/x86_64-linux-gnu)"
echo ""

echo "set(CMAKE_PREFIX_PATH /usr/lib/x86_64-linux-gnu/cmake)"
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

find src -type d -regex '.+/[^/.]+' | awk '{ print "include_directories(" $0 ")" }'

echo ""

echo "add_executable(unit_e"
find src -type f -regextype gnu-awk -regex '.+\.[ch](pp)?' | awk '{ print "      " $0 }'
echo ")"
) > CMakeLists.txt

