#include "Saver.h"

#include "PdfFile.h"

#include <poppler-qt6.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <cstdio>

namespace pdf {

static Saver::Result writeBeside(const PdfFile &file, const QString &target)
{
    if (!file.isOpen())
        return Saver::Result::NothingOpen;
    const QFileInfo info(target);
    if (!QFileInfo(info.absolutePath()).isWritable() || (info.exists() && !info.isWritable()))
        return Saver::Result::CannotWrite;

    QTemporaryFile temp(QDir(info.absolutePath()).filePath(QStringLiteral(".%1.XXXXXX")
                                                               .arg(info.fileName())));
    temp.setAutoRemove(true);
    if (!temp.open())
        return Saver::Result::CannotWrite;
    temp.close();

    auto converter = file.document()->pdfConverter();
    converter->setOutputFileName(temp.fileName());
    converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
    if (!converter->convert())
        return Saver::Result::Failed;

    // Check what was written opens before it replaces anything.
    PdfFile check;
    if (check.open(temp.fileName()) != PdfFile::OpenResult::Opened
        || check.pageCount() != file.pageCount())
        return Saver::Result::Failed;
    check.close();

    if (info.exists())
        QFile::setPermissions(temp.fileName(), QFile::permissions(target));
    else
        QFile::setPermissions(temp.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                   | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    // rename() replaces the target in one step on the same file system.
    if (std::rename(QFile::encodeName(temp.fileName()).constData(),
                    QFile::encodeName(info.absoluteFilePath()).constData()) != 0)
        return Saver::Result::CannotWrite;
    temp.setAutoRemove(false);
    return Saver::Result::Saved;
}

Saver::Result Saver::save(PdfFile &file)
{
    const QString path = file.path();
    const Result result = writeBeside(file, path);
    if (result == Result::Saved && file.open(path) != PdfFile::OpenResult::Opened)
        return Result::Failed;
    return result;
}

Saver::Result Saver::saveCopy(const PdfFile &file, const QString &target)
{
    if (QFileInfo(target).absoluteFilePath() == file.path())
        return Result::CannotWrite;
    return writeBeside(file, target);
}

} // namespace pdf
