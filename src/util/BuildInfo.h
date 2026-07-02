#pragma once

namespace hopy {
// Seed translation unit so hopy_lib always has at least one source.
// Returns the application short name; real build/version info can grow here.
const char* buildName();
}
