#include "FormFields.h"

#include "PdfFile.h"

#include <poppler-form.h>
#include <poppler-qt6.h>

#include <algorithm>

namespace pdf {

static FormFields::Kind kindOf(const Poppler::FormField &field)
{
    switch (field.type()) {
    case Poppler::FormField::FormText:
        return FormFields::Kind::Text;
    case Poppler::FormField::FormChoice:
        return FormFields::Kind::Choice;
    case Poppler::FormField::FormSignature:
        return FormFields::Kind::Signature;
    case Poppler::FormField::FormButton: {
        const auto &button = static_cast<const Poppler::FormFieldButton &>(field);
        if (button.buttonType() == Poppler::FormFieldButton::CheckBox)
            return FormFields::Kind::Check;
        if (button.buttonType() == Poppler::FormFieldButton::Radio)
            return FormFields::Kind::Radio;
        return FormFields::Kind::Other;
    }
    }
    return FormFields::Kind::Other;
}

FormFields::FormFields(const PdfFile &file)
{
    for (int i = 0; i < file.pageCount(); ++i) {
        const auto page = file.page(i);
        if (!page)
            continue;
        for (auto &handle : page->formFields()) {
            if (!handle->isVisible())
                continue;
            Entry entry;
            entry.field.id = handle->id();
            entry.field.page = i;
            entry.field.kind = kindOf(*handle);
            entry.field.label = handle->uiName().trimmed();
            if (entry.field.label.isEmpty())
                entry.field.label = handle->name();
            entry.field.rect = handle->rect();
            entry.field.readOnly = handle->isReadOnly();
            if (entry.field.kind == Kind::Text) {
                auto &text = static_cast<Poppler::FormFieldText &>(*handle);
                entry.field.multiLine = text.textType() == Poppler::FormFieldText::Multiline;
            }
            if (entry.field.kind == Kind::Choice)
                entry.field.options = static_cast<Poppler::FormFieldChoice &>(*handle).choices();
            entry.handle = std::move(handle);
            refresh(entry);
            m_fields.push_back(std::move(entry));
        }
    }
    // Reading order: page, then rows top to bottom, then left to right. Fields
    // whose tops lie within half a field's height count as one row.
    std::stable_sort(m_fields.begin(), m_fields.end(), [](const Entry &a, const Entry &b) {
        if (a.field.page != b.field.page)
            return a.field.page < b.field.page;
        const double slack = std::min(a.field.rect.height(), b.field.rect.height()) / 2;
        if (std::abs(a.field.rect.top() - b.field.rect.top()) > slack)
            return a.field.rect.top() < b.field.rect.top();
        return a.field.rect.left() < b.field.rect.left();
    });

}

FormFields::~FormFields() = default;

void FormFields::refresh(Entry &entry)
{
    Field &f = entry.field;
    switch (f.kind) {
    case Kind::Text:
        f.text = static_cast<Poppler::FormFieldText &>(*entry.handle).text();
        break;
    case Kind::Check:
    case Kind::Radio:
        f.checked = static_cast<Poppler::FormFieldButton &>(*entry.handle).state();
        break;
    case Kind::Choice: {
        auto &choice = static_cast<Poppler::FormFieldChoice &>(*entry.handle);
        const auto chosen = choice.currentChoices();
        f.text = chosen.isEmpty() ? choice.editChoice()
                                  : f.options.value(chosen.first());
        break;
    }
    default:
        break;
    }
}

int FormFields::indexOf(int id) const
{
    for (int i = 0; i < count(); ++i)
        if (at(i).id == id)
            return i;
    return -1;
}

bool FormFields::setText(int index, const QString &text)
{
    if (index < 0 || index >= count())
        return false;
    Entry &entry = m_fields[std::size_t(index)];
    if (entry.field.kind != Kind::Text || entry.field.readOnly)
        return false;
    auto &field = static_cast<Poppler::FormFieldText &>(*entry.handle);
    QString value = text;
    if (!entry.field.multiLine)
        value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (field.maximumLength() > 0)
        value.truncate(field.maximumLength());
    if (value == entry.field.text)
        return true;
    field.setText(value);
    refresh(entry);
    m_changed = true;
    return true;
}

bool FormFields::setChecked(int index, bool checked)
{
    if (index < 0 || index >= count())
        return false;
    Entry &entry = m_fields[std::size_t(index)];
    if ((entry.field.kind != Kind::Check && entry.field.kind != Kind::Radio) || entry.field.readOnly)
        return false;
    if (entry.field.checked == checked)
        return true;
    static_cast<Poppler::FormFieldButton &>(*entry.handle).setState(checked);
    // A radio button turns its siblings off; read the whole group back.
    for (Entry &other : m_fields)
        if (other.field.kind == Kind::Check || other.field.kind == Kind::Radio)
            refresh(other);
    m_changed = true;
    return true;
}

bool FormFields::setChoice(int index, const QString &option)
{
    if (index < 0 || index >= count())
        return false;
    Entry &entry = m_fields[std::size_t(index)];
    if (entry.field.kind != Kind::Choice || entry.field.readOnly)
        return false;
    auto &choice = static_cast<Poppler::FormFieldChoice &>(*entry.handle);
    const int at = entry.field.options.indexOf(option);
    if (at >= 0)
        choice.setCurrentChoices({at});
    else if (choice.isEditable())
        choice.setEditChoice(option);
    else
        return false;
    refresh(entry);
    m_changed = true;
    return true;
}

bool FormFields::fillable(int index) const
{
    const Field &f = at(index);
    return !f.readOnly && f.kind != Kind::Other && f.kind != Kind::Signature;
}

bool FormFields::isMissing(int index) const
{
    const Field &f = at(index);
    if (!fillable(index))
        return false;
    return (f.kind == Kind::Text || f.kind == Kind::Choice) && f.text.trimmed().isEmpty();
}

int FormFields::stillNeeded() const
{
    int n = 0;
    for (int i = 0; i < count(); ++i)
        if (isMissing(i))
            ++n;
    return n;
}

int FormFields::next(int index) const
{
    for (int step = 1; step <= count(); ++step) {
        const int i = ((index < 0 ? -1 : index) + step) % count();
        if (fillable(i))
            return i;
    }
    return -1;
}

int FormFields::previous(int index) const
{
    for (int step = 1; step <= count(); ++step) {
        const int i = ((index < 0 ? 0 : index) - step + 2 * count()) % count();
        if (fillable(i))
            return i;
    }
    return -1;
}

int FormFields::firstMissing() const
{
    for (int i = 0; i < count(); ++i)
        if (isMissing(i))
            return i;
    return -1;
}

} // namespace pdf
