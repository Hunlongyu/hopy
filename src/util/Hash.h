#pragma once
#include <QByteArray>
#include "storage/Record.h"

namespace hopy::Hash {
quint64 fnv1a(const QByteArray& data);
quint64 contentHash(ContentType type, const QByteArray& payload);
} // namespace hopy::Hash
