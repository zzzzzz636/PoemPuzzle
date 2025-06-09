//poemrepository.h
#ifndef POEMREPOSITORY_H
#define POEMREPOSITORY_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>

struct Poem {
    QString  title;
    int      size;
    QStringList lines;
};

class PoemRepository {
public:
    static PoemRepository& instance();

    bool load(const QString& jsonPath);
    const QVector<Poem>& poems() const { return m_poems; }
    Poem poemByTitle(const QString& t) const;

private:
    PoemRepository() = default;
    QVector<Poem> m_poems;
    QHash<QString,int> m_index;      // title → idx
};

#endif // POEMREPOSITORY_H
