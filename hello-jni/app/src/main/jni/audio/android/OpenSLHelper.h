/****************************************************************************
Copyright (c) 2016 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#ifndef COCOS_OPENSLHELPER_H
#define COCOS_OPENSLHELPER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <functional>
#include <string>

#define SL_DESTROY_OBJ(OBJ)    \
    if ((OBJ) != NULL) { \
        (*(OBJ))->Destroy(OBJ); \
        (OBJ) = NULL; \
    }

#define SL_RETURN_VAL_IF_FAILED(r, rval, ...) \
    if (r != SL_RESULT_SUCCESS) {\
        LOGE("SL result %d is wrong, msg: %s", r, __VA_ARGS__); \
        return rval; \
    }

#define SL_RETURN_IF_FAILED(r, ...) \
    if (r != SL_RESULT_SUCCESS) {\
        LOGE("SL result %d is wrong, msg: %s", r, __VA_ARGS__); \
        return; \
    }

typedef std::function<int(const std::string&, off_t* start, off_t* length)> FdGetterCallback;

#endif //COCOS_OPENSLHELPER_H
