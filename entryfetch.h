#ifndef ENTRYFETCH_H
#define ENTRYFETCH_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>

#define COMPOS_PATH "/home/sandsmark/tg/entries/"

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
        QString ret(compoName + "/" + entryName + "_by_" + author + "." + fileExtension);
        ret.replace(" ", "_");
        ret.replace("/", "_");
        return COMPOS_PATH + ret;

//        return compoName + "/" + entryName.replace(" ", ".") + ".2017." + compoName + "-" + author.replace(" ", ".") + "." + fileExtension;
    }
};

struct Compo {
    QString name;
    QString id;
    bool isValid() { return !name.isEmpty() && !id.isEmpty(); }
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
    void onCompoFetched();
    void fetchNextEntry();

private:
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_entriesReply;
    QList<Compo> m_compos;
    QList<Entry> m_entries;
    QMultiMap<QString, QString> m_extensions;
};

#endif // ENTRYFETCH_H
