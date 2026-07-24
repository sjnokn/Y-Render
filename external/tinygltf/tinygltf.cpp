// Single canonical owner of the tinygltf implementation.
// TINYGLTF_IMPLEMENTATION must be defined in exactly one TU.
// TINYGLTF_NO_STB_IMAGE / TINYGLTF_NO_STB_IMAGE_WRITE / TINYGLTF_USE_CPP14 are
// provided globally via the project preprocessor definitions so every TU that
// includes <tinygltf.h> sees a consistent configuration (avoids ODR drift).
#pragma warning(push, 0)
#pragma warning(disable: 4996 4244 4267 4189 4100 4456 4459 4324)

#ifndef TINYGLTF_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#endif

#include <tinygltf.h>

#pragma warning(pop)
