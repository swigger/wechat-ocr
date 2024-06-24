#pragma once
#if __has_include(<Python.h>)
#define USE_PYTHON 1
#define Py_LIMITED_API 0x03080000

// we dont have python310_d.lib, so temporarily disable debug mode
#pragma push_macro("_DEBUG")
#undef _DEBUG
#include <Python.h>
#pragma pop_macro("_DEBUG")

#endif
