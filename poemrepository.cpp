//poemrepository.cpp
#include "poemrepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

PoemRepository& PoemRepository::instance()
{
    static PoemRepository repo;
    return repo;
}

bool PoemRepository::load(const QString& jsonPath)
{
    QFile f(jsonPath);
    if(!f.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if(!doc.isArray()) return false;

    m_poems.clear(); m_index.clear();
    int idx = 0;
    for(const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();
        Poem p;
        p.title = o["title"].toString();
        p.size  = o["size"].toInt();
        for(const QJsonValue& line : o["lines"].toArray())
            p.lines << line.toString();
        m_index.insert(p.title, idx++);
        m_poems  << p;
    }
    return true;
}

Poem PoemRepository::poemByTitle(const QString& t) const
{
    return m_index.contains(t) ? m_poems[m_index.value(t)] : Poem{};
}
