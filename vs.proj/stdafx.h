#pragma once
#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>
#include <span>
#include <mutex>
#include <condition_variable>

using std::string;
using std::wstring;
typedef const std::string& crefstr;
