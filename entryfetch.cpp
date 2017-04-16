#include "entryfetch.h"
#include <QJsonDocument>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>



EntryFetch::EntryFetch(QObject *parent) : QObject(parent)
{

}

EntryFetch::~EntryFetch()
{
    for (const QString &key : m_extensions.keys()) {
        qDebug() << key << m_extensions.values(key);
    }
}

void EntryFetch::fetchCompos()
{
    QUrl entriesUrl("http://unicorn.gathering.org/api/beamer/competitions");
    m_entriesReply = m_nam.get(QNetworkRequest(entriesUrl));
    connect(m_entriesReply, &QNetworkReply::finished, this, &EntryFetch::onComposFetched);
    connect(m_entriesReply, &QNetworkReply::finished, m_entriesReply, &QNetworkReply::deleteLater);
}

static void capitalize(QString &string)
{
    QStringList parts = string.split(' ', QString::SkipEmptyParts);
    for (QString &part : parts) {
        part[0] = part[0].toUpper();
    }

    string = parts.join("");
}

void EntryFetch::onComposFetched()
{
    static const QStringList bannedCompos(QStringList() << "Casemod" << "Hacking" << "ProIntro&SoundDesign" << "TheGatheringProgrammingChampionship");

    if (!m_entriesReply) {
        qWarning() << "no fetch reply";
        return;
    }
    if (m_entriesReply->error() != QNetworkReply::NoError) {
        qWarning() << m_entriesReply->errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(m_entriesReply->readAll());
//    qDebug().noquote() << doc.toJson();
//    qApp->quit();
//    return;

    QJsonArray datas = doc.object()["data"].toArray();
    for (const QJsonValue &val : datas) {
        QJsonObject obj = val.toObject();

        Compo compo;
        compo.name = obj["name"].toString();
        capitalize(compo.name);
        if (bannedCompos.contains(compo.name)) {
            qWarning() << " - Skipping banned compo" << compo.name;
            continue;
        }
        if (obj["genre"].toString() == "gaming") {
            qWarning() << " - Skipping game compo" << compo.name;
            continue;
        }
//        qDebug() << compo.name << obj["genre"].toString();

        compo.id = obj["id"].toString();
        if (!compo.isValid()) {
            qWarning() << "Invalid compo" << val;
            continue;
        }
        m_compos.append(compo);
    }

    fetchNextCompo();
}

void EntryFetch::fetchNextCompo()
{
    if (m_compos.isEmpty()) {
        qWarning() << "No more compos";
        fetchNextEntry();
        return;
    }

    Compo compo = m_compos.takeFirst();
    qDebug() << " Fetching compo" << compo.name << compo.id;
    QUrl entryUrl("http://unicorn.gathering.org/api/beamer/entries/" + compo.id);
    QNetworkReply *entryReply = m_nam.get(QNetworkRequest(entryUrl));
    connect(entryReply, &QNetworkReply::finished, entryReply, &QNetworkReply::deleteLater);
    connect(entryReply, &QNetworkReply::finished, this, &EntryFetch::onCompoFetched);
    connect(entryReply, &QNetworkReply::finished, this, &EntryFetch::fetchNextCompo);
}

bool isCorrectFiletype(const QString &extension, const QString &genre)
{
    static const QStringList videoExtensions(QStringList() << "mov" << "mp4");
    static const QStringList graphicsExtensions(QStringList() << "png" << "jpg");
    static const QStringList musicExtensions(QStringList() << "wav" << "mp3");
    static const QStringList multidisciplineExtensions(QStringList() << "zip" << "rar");
    static const QStringList programmingExtensions(QStringList() << "zip" << "rar" << "html");
//    qDebug() << extension << videoExtensions;

    if (genre == "video") {
        return videoExtensions.contains(extension);
    } else if (genre == "graphics" || genre == "cosplay") {
        return graphicsExtensions.contains(extension);
    } else if (genre == "multidiscipline") {
        return multidisciplineExtensions.contains(extension);
    } else if (genre == "programming") {
        return programmingExtensions.contains(extension);
    } else if (genre == "music") {
        return musicExtensions.contains(extension);
    } else {
        qWarning() << "Unknown genre" << genre;
    }

    return false;

}

static Entry createEntry(const QJsonObject &fileObj)
{
    Entry entry;
    entry.url = QUrl(fileObj["url"].toString());
    entry.fileExtension = fileObj["extension"].toString();
    entry.filetype = fileObj["filetype"].toString();
    return entry;
}

void EntryFetch::onCompoFetched()
{
    QNetworkReply *entryReply = qobject_cast<QNetworkReply*>(sender());
    if (!entryReply) {
        qWarning() << "no fetch reply";
        return;
    }
    if (entryReply->error() != QNetworkReply::NoError) {
        qWarning() << entryReply->errorString();
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(entryReply->readAll());
    QJsonObject compoObject = doc.object();

//    qDebug().noquote() << doc.toJson();
//    return;

    QString compoName = compoObject["title"].toString();
    if (compoName.isEmpty()) {
        qWarning() << "Invalid compo title" << doc.toJson(QJsonDocument::Compact);
        return;
    }

    capitalize(compoName);
    QString compoGenre = compoObject["genre"].toString();
    if (compoGenre.isEmpty()) {
        qWarning() << "Invalid compo genre" << doc.toJson(QJsonDocument::Compact);
        return;
    }
    qDebug() << " Compo" << compoName << "fetched";
    QDir().mkpath(COMPOS_PATH + compoName + "/");

    QJsonArray entries = compoObject["data"].toArray();
    for (const QJsonValue &val : entries) {
        QJsonObject compoEntryObj = val.toObject();


        // Fetch main entry
        QList<Entry> potentials;
        {
            Entry mainEntry = createEntry(compoEntryObj["main_entry"].toObject());
            if (isCorrectFiletype(mainEntry.fileExtension, compoGenre)) {
                potentials.append(mainEntry);
            }
        }


        if (potentials.isEmpty()) {
            for (const QJsonValue &fileVal : compoEntryObj["files"].toArray()) {
                QJsonObject fileObj = fileVal.toObject();
                Entry entry = createEntry(fileObj);
                if (isCorrectFiletype(entry.fileExtension, compoGenre)) {
                    potentials.append(entry);
                }
            }
        }

        if (potentials.length() > 1) {
            qWarning() << "To many potential entries for" << val << compoGenre;
            continue;
        }

        if (potentials.isEmpty()) {
            qWarning() << "No files for entry" << QJsonDocument(val.toObject()).toJson();
            qWarning() << compoName << compoGenre;
            continue;
        }

//        if (potentials.length() > 1) {
//            QMutableListIterator<Entry> it(potentials);
//            while (it.hasNext()) {
//                const Entry &entry = it.next();
//                if (!isCorrectFiletype(entry.fileExtension, compoGenre)) {
//                    qDebug() << "Invalid file type" << entry.filetype << compoGenre;
//                    it.remove();
//                    continue;
//                }
//            }
//        }
//        if (potentials.isEmpty()) {
//            qWarning() << "No valid files for" << val << compoGenre;
//            continue;
//        }
        Entry entry = potentials.first();
        entry.compoName = compoName;
        entry.entryName = compoEntryObj["name"].toString();
        entry.author = compoEntryObj["author"].toString();

        if (!entry.isValid()) {
            qWarning() << "Invalid entry" << val;
            continue;
        }

        m_entries.append(entry);
    }
}

void EntryFetch::fetchNextEntry()
{
    if (m_entries.isEmpty()) {
        qDebug() << "All entries fetched";
        qApp->quit();
        return;
    }
    Entry entry;
    while (!m_entries.isEmpty()) {
        entry = m_entries.takeFirst();
        qDebug() << "checking if exists" << entry.filePath() << QFile::exists(entry.filePath());
        if (QFile::exists(entry.filePath())) {
            qDebug() << "Skipping already downloaded file" << entry.filePath();
            continue;
        } else {
            break;
        }
    }
    if (QFile::exists(entry.filePath())) {
        qWarning() << "File exists" << entry.filePath();
        qApp->quit();
    }

    QNetworkReply *entryReply = m_nam.get(QNetworkRequest(entry.url));
    QFile *file = new QFile(entry.filePath(), entryReply);
    if (!file->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open" << file->fileName();
        entryReply->deleteLater();
        qApp->quit();
        return;
    }
    qDebug() << "fetching entry" << entry.filePath() << "to" << file->fileName();

    connect(entryReply, &QNetworkReply::finished, entryReply, &QNetworkReply::deleteLater);

    connect(entryReply, &QNetworkReply::finished, this, [=]() {
        file->write(entryReply->readAll());
        file->close();
    });
    connect(entryReply, &QNetworkReply::readyRead, this, [=]() {
        file->write(entryReply->readAll());
    });

    connect(entryReply, &QNetworkReply::finished, this, &EntryFetch::fetchNextEntry);
}
