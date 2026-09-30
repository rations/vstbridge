# Versions of the Wine and FEX builds used by vstbridge on aarch64. Sourced by the build
# scripts in this directory and hashed by the CI workflow's cache key, so changing anything
# here rebuilds everything.

# Wine and the wine-staging patch set. staging/upstream-commit in the staging tag must match
# the Wine tag.
WINE_VERSION=11.18
WINE_STAGING_TAG=v11.18

# Installation prefix on the Pi. Kept private so it never conflicts with Debian's wine.
WINE_PREFIX=/opt/vstbridge/wine

# llvm-mingw builds the PE side of Wine (arm64ec, aarch64, i386) and the FEX DLLs.
LLVM_MINGW_VERSION=20260922
LLVM_MINGW_SHA256_AARCH64=07d21263c56bfe9a713db6fdb3f7434bf4c121a005e40397d3b4c0170fb06769

# FEX-2609-137-g0df84d384
FEX_COMMIT=0df84d3844bcdb87bb7d3f5b8fb0959cd009c038
