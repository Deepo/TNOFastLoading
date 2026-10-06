#include "stdafx.h"
#include "patch_result.h"

namespace Patch
{
    const char* ToString(Result r)
    {
        switch (r) {
        case Result::Patched: return "patched";
        case Result::AlreadyPatched: return "already patched";
        case Result::UnknownBuild: return "unknown build, nothing changed";
        default: return "failed, nothing changed";
        }
    }
}
