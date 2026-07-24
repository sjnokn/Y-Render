// Single canonical owner of the stb_image / stb_image_write implementations.
// Included exactly once across the whole project to satisfy the *_IMPLEMENTATION
// macros (defining them in more than one TU triggers LNK2005).
#pragma warning(push, 0)
#pragma warning(disable: 4996 4244 4267 4189)

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif

// UTF-8 filesystem paths (project compiles with /utf-8) and human-readable errors.
#ifndef STBI_WINDOWS_UTF8
#define STBI_WINDOWS_UTF8
#endif
#ifndef STBI_FAILURE_USERMSG
#define STBI_FAILURE_USERMSG
#endif

#include <stb_image.h>
#include <stb_image_write.h>

#pragma warning(pop)
