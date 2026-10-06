#pragma once

#include <string>

// TNOFastLoading.asi (src/loadfix): the public file, "Wolfenstein The New Order - Fast Loading"
// by Deepo on Nexus Mods. 1.0.0 is the first release.
#define FIX_NAME "TNOFastLoading"
#define REPO_URL ""

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 1

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

inline const std::string FixVersion = VERSION_STRING;
inline const std::string FixName = FIX_NAME;

#define COMPANY_NAME      ""
#define PRODUCT_NAME      "Wolfenstein The New Order - Fast Loading"
#define PRODUCT_VERSION   VERSION_STRING
#define FILE_VERSION      VERSION_STRING
#define LEGAL_COPYRIGHT   "Copyright (c) 2026 Deepo. MIT License. Third-party notices: TNOFastLoading-THIRD-PARTY-NOTICES.txt."
#define LEGAL_TRADEMARKS  ""
#define COMMENTS          "ASI plugin for Wolfenstein: The New Order (the GOG, Steam, Epic Games Store and Xbox app / PC Game Pass versions): shorter loads (stream reads stop sleeping 1 ms each; the post-load material preload is checked every 1 ms instead of 100 ms), no press-X prompt after a load unless a loading cutscene plays, and no start-up logo video. Each switchable in TNOFastLoading.ini; any other build of the game is left untouched."
#define FILE_DESCRIPTION  "Wolfenstein The New Order - Fast Loading (ASI plugin)"
#define INTERNAL_NAME     FIX_NAME ".asi"
#define ORIGINAL_FILENAME FIX_NAME ".asi"
