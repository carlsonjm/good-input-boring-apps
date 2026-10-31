#pragma once

#include <QString>

namespace pdf {

class PdfFile;

// Writes an open PDF with every change made to it: form answers, highlights,
// a placed signature. It writes a new file beside the target first and only
// then swaps it in, so a failed save never damages the file a person had.
class Saver
{
public:
    enum class Result { Saved, NothingOpen, CannotWrite, Failed };

    // Saves over the file that is open, keeping its permissions, then opens
    // the saved file again in its place so later changes build on it.
    static Result save(PdfFile &file);
    // Saves the changes to another file and leaves the open one as it was.
    static Result saveCopy(const PdfFile &file, const QString &target);
};

} // namespace pdf
