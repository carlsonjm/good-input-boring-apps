#pragma once

#include "FormFields.h"
#include "Marks.h"
#include "PdfFile.h"
#include "ReadingPlace.h"
#include "Signature.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QUrl>
#include <QVariantList>

#include <memory>

// The one open PDF as the look sees it. Everything is in page-normalised
// numbers, 0 to 1 from a page's top-left corner, so QML only multiplies by
// the size it draws the page at. Page pictures come from PageImages, which
// asks renderPage(); `revision` rises whenever a picture must be drawn again.
class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY opened)
    Q_PROPERTY(QString title READ title NOTIFY opened)
    Q_PROPERTY(QUrl url READ url NOTIFY opened)
    // The file last asked for, opened or not: a locked one is asked again by it.
    Q_PROPERTY(QUrl asked READ asked NOTIFY opened)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY opened)
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool changed READ changed NOTIFY changedChanged)
    Q_PROPERTY(int fieldCount READ fieldCount NOTIFY opened)
    Q_PROPERTY(int stillNeeded READ stillNeeded NOTIFY formChanged)
    Q_PROPERTY(QVariantList searchHits READ searchHits NOTIFY searchHitsChanged)
    Q_PROPERTY(bool hasSignature READ hasSignature NOTIFY signatureChanged)

public:
    // `dataDir` and `settingsFile` empty mean the app's own folders; tests
    // pass their own so they touch nothing of the person's.
    explicit Session(const QString &dataDir = {}, const QString &settingsFile = {},
                     QObject *parent = nullptr);
    ~Session() override;

    // "empty", "open", "missing", "notPdf", "needsPassword", "wrongPassword"
    QString status() const { return m_status; }
    QString title() const;
    QUrl url() const;
    QUrl asked() const { return m_asked; }
    int pageCount() const;
    int page() const { return m_page; }
    void setPage(int page);
    int revision() const { return m_revision; }
    bool changed() const;
    int fieldCount() const;
    int stillNeeded() const;
    QVariantList searchHits() const { return m_searchHits; }
    bool hasSignature() const;

    Q_INVOKABLE bool open(const QUrl &url, const QString &password = {});
    Q_INVOKABLE void close();
    // Height over width, for laying out pages before their pictures arrive.
    Q_INVOKABLE double pageAspect(int page) const;

    // Selection. Each returns {text, rects: [{x, y, w, h}]}; empty text when nothing.
    Q_INVOKABLE QVariantMap wordAt(int page, double x, double y) const;
    Q_INVOKABLE QVariantMap textBetween(int page, double x1, double y1, double x2, double y2) const;
    Q_INVOKABLE void copy(const QString &text) const;

    // Marks. `colour` indexes the four highlight colours.
    Q_INVOKABLE QVariantList highlightColours() const;
    Q_INVOKABLE QString highlight(int page, const QVariantList &rects, int colour);
    Q_INVOKABLE QVariantList highlights(int page) const;
    Q_INVOKABLE bool removeMark(int page, const QString &name);

    // Search. Hits are {page, x, y, w, h}, in reading order.
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE QVariantList pagesWithHits() const; // [{page, count}]

    // Form. Fields are maps of FormFields::Field, `index` is the fill order.
    Q_INVOKABLE QVariantMap field(int index) const;
    Q_INVOKABLE QVariantList fieldsOnPage(int page) const;
    Q_INVOKABLE bool setFieldText(int index, const QString &text);
    Q_INVOKABLE bool setFieldChecked(int index, bool checked);
    Q_INVOKABLE bool setFieldChoice(int index, const QString &option);
    Q_INVOKABLE int nextField(int index) const;
    Q_INVOKABLE int previousField(int index) const;
    Q_INVOKABLE int firstEmptyField() const;

    // Signature. Strokes are lists of {x, y} on a pad `aspect` times as wide as tall.
    Q_INVOKABLE bool saveSignature(const QVariantList &strokes, double aspect);
    Q_INVOKABLE QVariantList signatureStrokes() const;
    Q_INVOKABLE double signatureAspect() const;
    Q_INVOKABLE bool forgetSignature();
    Q_INVOKABLE QString placeSignature(int page, double x, double y, double w, double h);

    // Saving. "saved", "cannotWrite", "failed" or "nothingOpen".
    Q_INVOKABLE QString save();
    Q_INVOKABLE QString saveCopy(const QUrl &target);

    // Called from PageImages on its loading thread.
    QImage renderPage(int page, int widthPx) const;

Q_SIGNALS:
    void opened();
    void pageChanged();
    void revisionChanged();
    void changedChanged();
    void formChanged();
    void searchHitsChanged();
    void signatureChanged();

private:
    void touched(bool formToo);
    void reloadForm();

    mutable QMutex m_lock; // the open document is shared with the picture thread
    pdf::PdfFile m_file;
    std::unique_ptr<pdf::FormFields> m_form;
    std::unique_ptr<pdf::Marks> m_marks;
    pdf::SignatureStore m_signature;
    pdf::ReadingPlace m_places;
    QString m_status = QStringLiteral("empty");
    int m_page = 0;
    int m_revision = 0;
    bool m_changed = false;
    QUrl m_asked;
    QString m_lastQuery;
    QVariantList m_searchHits;
};
