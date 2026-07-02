#include "ui/TypeFilter.h"
#include "util/Strings.h"

namespace hopy {

namespace { constexpr int kCount = 5; }

ContentFilter nextFilter(ContentFilter f) {
    return static_cast<ContentFilter>((static_cast<int>(f) + 1) % kCount);
}

ContentFilter prevFilter(ContentFilter f) {
    return static_cast<ContentFilter>((static_cast<int>(f) + kCount - 1) % kCount);
}

bool matchesFilter(ContentFilter f, ContentType t, bool favorite) {
    switch (f) {
        case ContentFilter::Text:      return t == ContentType::Text || t == ContentType::RichText;
        case ContentFilter::Image:     return t == ContentType::Image;
        case ContentFilter::Files:     return t == ContentType::Files;
        case ContentFilter::Favorites: return favorite;
        case ContentFilter::All:       return true;
    }
    return true;
}

const char* filterLabel(ContentFilter f) {
    switch (f) {
        case ContentFilter::Text:      return str::kFilterText;
        case ContentFilter::Image:     return str::kFilterImage;
        case ContentFilter::Files:     return str::kFilterFiles;
        case ContentFilter::Favorites: return str::kFilterFavorites;
        case ContentFilter::All:       return str::kFilterAll;
    }
    return str::kFilterAll;
}

} // namespace hopy
