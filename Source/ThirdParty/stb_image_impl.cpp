// Single translation unit that emits stb_image's implementation. Other
// modules just include "ThirdParty/stb_image.h" (no define) for the
// declarations.
//
// Only the GIF decoder is enabled — JUCE's juce::ImageFileFormat already
// covers PNG/JPG. We use stb purely because juce::GIFImageFormat decodes
// only the first frame, and we want animated GIF cover art for packs.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_LINEAR
#define STBI_NO_HDR

#if defined(_MSC_VER)
 #pragma warning(push)
 #pragma warning(disable: 4244 4456 4457 4701 4702 4838 26451)
#endif

#include "stb_image.h"

#if defined(_MSC_VER)
 #pragma warning(pop)
#endif
