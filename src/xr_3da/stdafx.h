#pragma once

#include "Common/Common.hpp"
//Debrovski: for reasons i dunno, Visual Studio 15.9.6 cant compile xr_3da with xrCore.h included
//..so temporarily referencing necessary stuff manually
//#include "xrCore/xrCore.h"
#include <stdio.h>
#include "../xrCore/_types.h"
IC size_t xr_strlen(const char* S) { return strlen(S); }
#define DEBUG_INFO {__FILE__, __LINE__, __FUNCTION__}
class ErrorLocation
{
public:
    const char* File = nullptr;
    int Line = -1;
    const char* Function = nullptr;

    ErrorLocation(const char* file, int line, const char* function)
    {
        File = file;
        Line = line;
        Function = function;
    }

    ErrorLocation& operator=(const ErrorLocation& rhs)
    {
        File = rhs.File;
        Line = rhs.Line;
        Function = rhs.Function;
        return *this;
    }
};

class XRCORE_API xrDebug
{
public:
    static void Fatal(const ErrorLocation& loc, const char* format, ...);
    static void Initialize(const bool& dedicated);
};
#define FATAL(desc) xrDebug::Fatal(DEBUG_INFO, "%s", desc)

struct LogCallback
{
    typedef void(*Func)(void* context, const char* s);
    Func Log;
    void* Context;

    LogCallback() : Log(nullptr), Context(nullptr) {}
    LogCallback(nullptr_t) : Log(nullptr), Context(nullptr) {}
    LogCallback(Func log, void* ctx) : Log(log), Context(ctx) {}
    void operator()(const char* s) { Log(Context, s); }
    operator bool() const { return !!Log; }
};

class XRCORE_API xrCore
{
public:
    void Initialize(
        pcstr ApplicationName, LogCallback cb = nullptr, bool init_fs = true, pcstr fs_fname = nullptr, bool plugin = false);
    void _destroy();
};
XRCORE_API xrCore Core;
