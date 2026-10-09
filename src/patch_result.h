#pragma once

// The outcome of a one-time patch, shared by every module of both plugins. Each module writes all
// of its bytes or none of them.
namespace Patch
{
    enum class Result { Patched, AlreadyPatched, UnknownBuild, Failed };

    const char* ToString(Result r);
}
