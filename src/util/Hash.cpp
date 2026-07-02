#include "util/Hash.h"

namespace hopy::Hash {

quint64 fnv1a(const QByteArray& data) {
    quint64 hash = 0xcbf29ce484222325ULL;
    for (char c : data) {
        hash ^= static_cast<quint8>(c);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

quint64 contentHash(ContentType type, const QByteArray& payload) {
    QByteArray buf;
    buf.reserve(payload.size() + 1);
    buf.append(static_cast<char>(static_cast<int>(type)));
    buf.append(payload);
    return fnv1a(buf);
}

} // namespace hopy::Hash
