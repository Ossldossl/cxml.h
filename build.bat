@echo off
set flags=-Wno-macro-redefined -O0 -g3 -gfull -Wall -fsanitize=address
clang example.c cxml.c -o out/example.exe %flags%
@echo on