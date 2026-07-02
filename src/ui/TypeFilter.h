#pragma once
#include "storage/Record.h"

namespace hopy {

// Cycle order (left/right arrows): 文本 → 图片 → 文件 → 收藏 → 全部 → 文本 …
enum class ContentFilter { Text = 0, Image = 1, Files = 2, Favorites = 3, All = 4 };

ContentFilter nextFilter(ContentFilter f);
ContentFilter prevFilter(ContentFilter f);
// A record matches when its type/favorite flag fits the active filter.
bool matchesFilter(ContentFilter f, ContentType t, bool favorite);
const char* filterLabel(ContentFilter f);

} // namespace hopy
