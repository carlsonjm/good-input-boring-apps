#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace Poppler {
class FormField;
class Page;
}

namespace pdf {

class PdfFile;

// The fields of a PDF form, in the order a person fills them: page by page,
// top to bottom, left to right. Changes are held in the open document and
// written by Saver.
class FormFields
{
public:
    enum class Kind { Text, Check, Radio, Choice, Signature, Other };

    struct Field {
        int id = -1;      // the PDF's own field id, stable while the file is open
        int page = -1;
        Kind kind = Kind::Other;
        QString label;    // the PDF's own description, else its name
        QRectF rect;      // normalised to the page
        bool readOnly = false;
        bool multiLine = false;
        QString text;     // Text: its words; Choice: the chosen option
        bool checked = false;
        QStringList options;
    };

    explicit FormFields(const PdfFile &file);
    ~FormFields();

    bool isEmpty() const { return m_fields.empty(); }
    int count() const { return int(m_fields.size()); }
    const Field &at(int index) const { return m_fields.at(std::size_t(index)).field; }
    int indexOf(int id) const;

    bool setText(int index, const QString &text);
    bool setChecked(int index, bool checked);
    bool setChoice(int index, const QString &option);

    // A field a person can type or pick in that is still empty. Tick boxes
    // never count: left unticked is an answer. Poppler's Qt layer does not
    // say which fields a form marks required, so every empty one counts.
    bool isMissing(int index) const;
    int stillNeeded() const;
    // The next field a person can fill after `index`, going round to the
    // start; -1 when there is none. `index` -1 starts at the top.
    int next(int index) const;
    int previous(int index) const;
    // The first field still empty, or -1.
    int firstMissing() const;

    bool changed() const { return m_changed; }

private:
    struct Entry {
        Field field;
        std::unique_ptr<Poppler::FormField> handle;
    };
    bool fillable(int index) const;
    void refresh(Entry &entry);

    std::vector<Entry> m_fields;
    bool m_changed = false;
};

} // namespace pdf
