#include <QCoreApplication>

#include "entryfetch.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    EntryFetch entries;
    entries.fetchCompos();

    return a.exec();
}
