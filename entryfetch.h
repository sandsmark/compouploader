#ifndef ENTRYFETCH_H
#define ENTRYFETCH_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QRegularExpression>

#define COMPOS_PATH "/home/sandsmark/tg/entries18/"

struct DownloadJob {
    QUrl url;
    QString filePath;
};

struct Entry {
    QUrl url;
    QString fileExtension;
    QString filetype;
    QString compoName;
    QString author;
    QString entryName;

    bool isValid() {
        if (!url.isValid()) {
            qWarning() << "invalid url" << url;
            return false;
        }
        if (fileExtension.isEmpty()) {
            qWarning() << "empty file extension";
            return false;
        }
        if (filetype.isEmpty()) {
            qWarning() << "empty file type";
            return false;
        }
        if (compoName.isEmpty()) {
            qWarning() << "empty compo name";
            return false;
        }
        if (author.isEmpty()) {
            qWarning() << "empty author";
            return false;
        }
        if (entryName.isEmpty()) {
            qWarning() << "empty entry name";
            return false;
        }
        return true;
    }

    QString filePath() {
        QString ret(entryName + " by " + author);
        ret.replace(QRegularExpression("\\W"), " ");
        ret = ret.simplified();
        ret.replace(" ", "_");
        ret += "." + fileExtension;
//        ret.replace("/", "_");
        return COMPOS_PATH + compoName + "/" + ret;

//        return compoName + "/" + entryName.replace(" ", ".") + ".2017." + compoName + "-" + author.replace(" ", ".") + "." + fileExtension;
    }
};

struct Compo {
    QString name;
    QString id;
    QString genre;
    bool isValid() { return !name.isEmpty() && !id.isEmpty() && !genre.isEmpty(); }
};

class EntryFetch : public QObject
{
    Q_OBJECT
public:
    explicit EntryFetch(QObject *parent = 0);
    virtual ~EntryFetch();

signals:

public slots:
    void fetchCompos();

private slots:
    void onComposFetched();
    void fetchNextCompo();
    void onCompoFetched(QNetworkReply *entryReply, const Compo compo);
    void fetchNextEntry();

private:
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_entriesReply;
    QList<Compo> m_compos;
    QList<Entry> m_entries;
//    QMap<QString, Compo> m_compos;
    QMultiMap<QString, QString> m_extensions;
};

#endif // ENTRYFETCH_H
