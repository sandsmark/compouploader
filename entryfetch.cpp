#include "entryfetch.h"
#include <QJsonDocument>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QRegularExpression>



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
    QUrl entriesUrl("https://unicorn.gathering.org/api/beamer/competitions");
    m_entriesReply = m_nam.get(QNetworkRequest(entriesUrl));
    connect(m_entriesReply, &QNetworkReply::finished, this, &EntryFetch::onComposFetched);
    connect(m_entriesReply, &QNetworkReply::finished, m_entriesReply, &QNetworkReply::deleteLater);
}

static void fixName(QString &string)
{
    string.remove(QRegularExpression("\\([^\\)]*\\)"));
    string.remove("'");
    string.replace(QRegularExpression("\\W"), " ");
    QStringList parts = string.split(' ', QString::SkipEmptyParts);
    for (QString &part : parts) {
        part[0] = part[0].toUpper();
    }

    string = parts.join("");
}

void EntryFetch::onComposFetched()
{
    static const QSet<QString> bannedCompos({
        "the-gathering-hacking-competition-tghack",
        "the-gathering-programming-championship",
        "google-blocks-konkurranse-i-virtual-reality-",
        "dragon39s-den-game-pitch",
        "vr-demo-konkurranse",
    });
    static const QSet<QString> bannedCategories({
        "gaming",
        "other",
        "community",
    });

    if (!m_entriesReply) {
        qWarning() << "no fetch reply";
        return;
    }
    if (m_entriesReply->error() != QNetworkReply::NoError) {
        qWarning() << m_entriesReply->errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(m_entriesReply->readAll());

    qDebug() << doc.toJson();

    QJsonArray datas = doc.object()["data"].toArray();
    for (const QJsonValue &val : datas) {
        QJsonObject obj = val.toObject();

        Compo compo;
        compo.name = obj["name"].toString();
        fixName(compo.name);
        if (bannedCompos.contains(obj["slug"].toString())) {
            qWarning() << " - Skipping banned compo" << compo.name;
            continue;
        }

        const QString compoGenre = obj["genre"].toString();
        if (bannedCategories.contains(compoGenre)) {
            qWarning() << " - Skipping compo in wrong category" << compo.name << compoGenre;
            continue;
        }

        compo.genre = compoGenre;
        compo.id = obj["id"].toString();
        if (!compo.isValid()) {
            qWarning() << "Invalid compo" << val;
            qApp->quit();
            return;
        }

        qDebug() << "Fetching comp" << compo.name;
        m_compos.append(compo);
    }

    qDebug() << "\n========= Compos listed =========\n";

//    qApp->quit();
//    return;

    fetchNextCompo();
}

void EntryFetch::fetchNextCompo()
{
    if (m_compos.isEmpty()) {
        qDebug() << "\n========= All compos fetched =========\n";
        fetchNextEntry();
        return;
    }

    Compo compo = m_compos.takeFirst();
    qDebug() << " > Fetching compo" << compo.name << compo.id;
    QUrl entryUrl("https://unicorn.gathering.org/api/beamer/results/" + compo.id);
    QNetworkReply *entryReply = m_nam.get(QNetworkRequest(entryUrl));
    connect(entryReply, &QNetworkReply::finished, entryReply, &QNetworkReply::deleteLater);
    connect(entryReply, &QNetworkReply::finished, this, [=]() {
        onCompoFetched(entryReply, compo);
    });
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
        qApp->quit();
        return false;
    }

    return false;

}

void EntryFetch::onCompoFetched(QNetworkReply *entryReply, const Compo compo)
{
    if (entryReply->error() != QNetworkReply::NoError) {
        qWarning() << entryReply->errorString();
        qApp->quit();
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(entryReply->readAll());
    const QJsonObject replyObject = doc.object();
    const QJsonObject compoObject = replyObject["data"].toObject();

    QString compoName = compo.name;
    if (compoName.isEmpty()) {
        qWarning() << "Invalid compo title";
        qDebug() << doc.toJson();
        qApp->quit();
        return;
    }

    QString compoGenre = compo.genre;
    if (compoGenre.isEmpty()) {
        qWarning() << "Invalid compo genre";
        qDebug() << doc.toJson();
        qApp->quit();
        return;
    }
    qDebug() << " < Compo" << compoName << "fetched";
    QDir().mkpath(COMPOS_PATH + compoName + "/");

    QJsonArray entries = compoObject["results"].toArray();
    for (const QJsonValue &val : entries) {
        const QJsonObject compoEntryObj = val.toObject();
        const QJsonArray filesArray = compoEntryObj["files"].toArray();
        if (filesArray.isEmpty()) {
            qWarning() << "no files";
            qDebug() << QJsonDocument(compoEntryObj).toJson();
            qApp->quit();
            return;
        }
        const QJsonObject fileObj = filesArray.first().toObject();

        Entry entry;
        entry.url = QUrl(fileObj["url"].toString());
        entry.fileExtension = fileObj["extension"].toString();
        entry.filetype = fileObj["filetype"].toString();

        entry.compoName = compoName;
        entry.entryName = compoEntryObj["title"].toString();
        entry.author = compoEntryObj["author"].toString();

        if (!entry.isValid()) {
            qWarning() << "Invalid entry";
            qDebug() << QJsonDocument(compoEntryObj).toJson();
            qApp->quit();
            continue;
        }

        m_entries.append(entry);
    }
}

void EntryFetch::fetchNextEntry()
{
    if (m_entries.isEmpty()) {
        qDebug() << "========= All entries fetched =========";
        qApp->quit();
        return;
    }
    Entry entry;
    while (!m_entries.isEmpty()) {
        entry = m_entries.takeFirst();
        if (QFile::exists(entry.filePath())) {
            continue;
        } else {
            break;
        }
    }
    if (QFile::exists(entry.filePath())) {
        qWarning() << "File exists" << entry.filePath();
        qApp->quit();
        return;
    }

    QNetworkReply *entryReply = m_nam.get(QNetworkRequest(entry.url));
    QFile *file = new QFile(entry.filePath(), entryReply);
    if (!file->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open" << file->fileName();
        entryReply->deleteLater();
        qApp->quit();
        return;
    }
    qDebug() << " Fetching entry" << entry.filePath() << "to" << file->fileName();

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
