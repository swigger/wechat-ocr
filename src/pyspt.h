#pragma once

#ifndef USE_PYTHON
#if __has_include(<Python.h>)
#define USE_PYTHON 1
#endif
#endif

#if USE_PYTHON+0
# define Py_LIMITED_API 0x03080000
# pragma push_macro("_DEBUG")
# undef _DEBUG  // we dont have python310_d.lib, so temporarily disable debug mode
# include <Python.h>
# pragma pop_macro("_DEBUG")
#endif
