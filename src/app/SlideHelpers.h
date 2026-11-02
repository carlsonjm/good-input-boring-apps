#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "OfficeDocument.h"

// Slides' helpers: each does one job on a slide and leaves the change waiting
// for Keep or Undo. Fit this slide and Make slides are Codex's, asked through
// Split Rock's broker on the person's tap with only the slide or the notes;
// Tidy the look is the app's own and needs neither. Without Split Rock, or
// without a Codex sign-in, the Codex helpers say so and Tidy still works.
class SlideHelpers : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(OfficeDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    // The broker is on this computer, so Codex's helpers can be offered.
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool codexReady READ codexReady NOTIFY stateChanged)
    // What is under way: "", "fit", "make", "tidy", or "apply" while the
    // slides are being put in the deck.
    Q_PROPERTY(QString working READ working NOTIFY stateChanged)
    // The change waiting for Keep or Undo: line, who ("codex" or "app") and
    // many (more than one slide).
    Q_PROPERTY(QVariantMap result READ result NOTIFY stateChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY stateChanged)

public:
    static constexpr char Service[] = "io.github.carlsonjm.splitrock.Broker";
    static constexpr char Path[] = "/Broker";
    static constexpr char Interface[] = "io.github.carlsonjm.SplitRock.Broker";

    explicit SlideHelpers(QObject *parent = nullptr);

    OfficeDocument *document() const { return m_document; }
    void setDocument(OfficeDocument *document);
    bool available() const { return m_available; }
    bool codexReady() const { return m_codexReady; }
    QString working() const { return m_working; }
    QVariantMap result() const { return m_result; }
    QString problem() const { return m_problem; }

    // Looks again for the broker and Codex's sign-in.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void fit();
    Q_INVOKABLE void tidy();
    Q_INVOKABLE void make(const QString &notes);
    Q_INVOKABLE void keep();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void dismiss();
    // Codex's own sign-in window, opened by Split Rock's Ask.
    Q_INVOKABLE void signIn();
    // The words of a document, for Make slides from a Docs file.
    Q_INVOKABLE QString wordsOf(const QUrl &file);

    // Puts a helper's slides in the deck, as Codex's answer to fit ("fit")
    // or make ("make") does, and leaves them waiting for Keep or Undo.
    void apply(const QString &kind, const QVariantList &slides, const QString &line);

    // The engine's layout for one of Codex's slide layouts.
    static int layoutFor(const QString &kind);
    // The layout a slide's boxes were laid out with, or -1 for none of ours.
    static int layoutOf(const QVariantList &boxes);

Q_SIGNALS:
    void documentChanged();
    void stateChanged();

private Q_SLOTS:
    void answered(uint id, const QVariantMap &result);
    void failed(uint id, const QString &problem);

private:
    void ask(const QString &method, const QVariant &argument, const QString &kind);
    void fill(int part, const QVariantMap &slide);
    void addSlide(int after, const QVariantMap &slide);
    void finish(const QVariantMap &result, const QString &problem = {});

    QPointer<OfficeDocument> m_document;
    bool m_available = false;
    bool m_codexReady = false;
    bool m_listening = false;
    QString m_working;
    QVariantMap m_result;
    QString m_problem;
    uint m_request = 0;
    int m_part = 0;
    int m_mark = -1;
};
