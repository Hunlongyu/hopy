#pragma once
#include <QSqlDatabase>
#include <QString>

namespace hopy {

class Database {
public:
    static Database openInMemory();
    static Database openAt(const QString& path);

    QSqlDatabase connection() const { return QSqlDatabase::database(name_); }
    bool isOpen() const { return connection().isOpen(); }
    bool migrate();

private:
    explicit Database(QString name) : name_(std::move(name)) {}
    QString name_;
};

} // namespace hopy
