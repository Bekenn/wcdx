#ifndef IOLIB_STRING_VIEW_INCLUDED
#define IOLIB_STRING_VIEW_INCLUDED
#pragma once

#include <string_view>


namespace iolib {

using std::basic_string_view;
using std::string_view;
using std::wstring_view;

int stoi(string_view str, size_t* idx = nullptr, int base = 10);
long stol(string_view str, size_t* idx = nullptr, int base = 10);
unsigned long stoul(string_view str, size_t* idx = nullptr, int base = 10);
long long stoll(string_view str, size_t* idx = nullptr, int base = 10);
unsigned long long stoull(string_view str, size_t* idx = nullptr, int base = 10);
#if 0
float stof(string_view str, size_t* idx = nullptr);
double stod(string_view str, size_t* idx = nullptr);
long double stold(string_view str, size_t* idx = nullptr);
#endif

int stoi(wstring_view str, size_t* idx = nullptr, int base = 10);
long stol(wstring_view str, size_t* idx = nullptr, int base = 10);
unsigned long stoul(wstring_view str, size_t* idx = nullptr, int base = 10);
long long stoll(wstring_view str, size_t* idx = nullptr, int base = 10);
unsigned long long stoull(wstring_view str, size_t* idx = nullptr, int base = 10);
#if 0
float stof(wstring_view str, size_t* idx = nullptr);
double stod(wstring_view str, size_t* idx = nullptr);
long double stold(wstring_view str, size_t* idx = nullptr);
#endif

}

#endif
