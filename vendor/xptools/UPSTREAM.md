X-Plane/xptools commit 0137e80cdf03e8bb4cb58edb081f4ddf4a32c042
https://github.com/X-Plane/xptools/tree/0137e80cdf03e8bb4cb58edb081f4ddf4a32c042
DSFLib and XChunkyFileUtils: MIT, notices retained in sources.
LZMA SDK 19: public domain, see source headers.

Local changes: DSFPointPool include replaced by standard headers (no writer dependency); fix USE_7Z=0 offset declaration, file-length subtraction, polygon pass flag; add cooperative Cancel_f callback. Runtime uses DSFReadMem and its own Unicode-aware bounded 7z loader, not upstream DSFReadFile.
Additional reader patches: standard types in DSFLib.h; patch-pass guard for final EndPatch; checked packed reads and point indices; pool/scale depth validation. The host validates atom lengths, string termination, RLE runs and decoded pool limits before DSFReadMem. Packed reads target little-endian hosts.
MD5 helper: RSA Data Security, Inc. MD5 Message-Digest Algorithm, original license retained in sources and LICENSE.txt.
