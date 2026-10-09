#pragma once

#include <string>

// TOBFastLoading.asi (src/tobfix): TNOFastLoading's fixes for Wolfenstein: The Old Blood,
// "Wolfenstein The Old Blood - Fast Loading" by Deepo. 1.0.0 is the first release; 0.x were test
// builds.
#define FIX_NAME "TOBFastLoading"
#define REPO_URL ""

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

inline const std::string FixVersion = VERSION_STRING;
inline const std::string FixName = FIX_NAME;

#define COMPANY_NAME      ""
#define PRODUCT_NAME      "Wolfenstein The Old Blood - Fast Loading"
#define PRODUCT_VERSION   VERSION_STRING
#define FILE_VERSION      VERSION_STRING
#define LEGAL_COPYRIGHT   "Copyright (c) 2026 Deepo. MIT License. Third-party notices: TOBFastLoading-THIRD-PARTY-NOTICES.txt."
#define LEGAL_TRADEMARKS  ""
#define COMMENTS          "ASI plugin for Wolfenstein: The Old Blood (the GOG, Steam and Xbox app / PC Game Pass versions): shorter loads (stream reads stop sleeping 1 ms each; the post-load material preload is checked every 1 ms instead of 100 ms), no press-to-continue prompt after a load unless a loading cutscene plays, no start-up logo video, no warning screens before the main menu, and a faster Quit to Desktop (the texture caches are no longer blanked page by page at exit). Each switchable in TOBFastLoading.ini; any other build of the game is left untouched."
#define FILE_DESCRIPTION  "Wolfenstein The Old Blood - Fast Loading (ASI plugin)"
#define INTERNAL_NAME     FIX_NAME ".asi"
#define ORIGINAL_FILENAME FIX_NAME ".asi"
