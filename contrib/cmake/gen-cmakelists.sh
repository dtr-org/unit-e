#!/bin/bash

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

find -E src -type d -regex '.+/[^/.]+' | awk '{ print "include_directories(" $0 ")" }'

echo ""

echo "add_executable(unit_e"
find -E src -type f -regex '.+\.[ch](pp)?' | awk '{ print "      " $0 }'
echo ")"
) > CMakeLists.txt

