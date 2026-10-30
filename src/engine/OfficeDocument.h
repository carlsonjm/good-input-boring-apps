#pragma once

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSizeF>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>

struct DocumentLink;

// One open file, as the engine sees it. Every position here is in twips
// (1/1440 of an inch), the engine's own unit; the view turns them into pixels.
// What the engine reports (cursor, selection, styles in force, size) arrives as
// properties; what a person does goes in through the invokable methods.
class OfficeDocument : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY statusChanged)
    Q_PROPERTY(Kind kind READ kind NOTIFY statusChanged)
    Q_PROPERTY(QString title READ title NOTIFY sourceChanged)
    Q_PROPERTY(QSizeF size READ size NOTIFY sizeChanged)
    // Where each page sits in a text document, in twips; empty for the others.
    Q_PROPERTY(QVariantList pages READ pages NOTIFY sizeChanged)
    Q_PROPERTY(int parts READ parts NOTIFY partsChanged)
    Q_PROPERTY(int currentPart READ currentPart NOTIFY partsChanged)
    Q_PROPERTY(QStringList partNames READ partNames NOTIFY partsChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(bool autosave READ autosave WRITE setAutosave NOTIFY autosaveChanged)
    Q_PROPERTY(QString saveError READ saveError NOTIFY savingChanged)
    Q_PROPERTY(QRectF cursorRect READ cursorRect NOTIFY cursorChanged)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY cursorChanged)
    Q_PROPERTY(QVariantList selectionRects READ selectionRects NOTIFY selectionChanged)
    Q_PROPERTY(QRectF selectionStart READ selectionStart NOTIFY selectionChanged)
    Q_PROPERTY(QRectF selectionEnd READ selectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QRectF cellCursor READ cellCursor NOTIFY cellChanged)
    Q_PROPERTY(QString cellAddress READ cellAddress NOTIFY cellChanged)
    Q_PROPERTY(QString cellFormula READ cellFormula NOTIFY cellChanged)
    Q_PROPERTY(QVariantMap states READ states NOTIFY statesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY statesChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY statesChanged)
    // The shape, picture or text box chosen on a slide, in twips; empty if none.
    Q_PROPERTY(QRectF objectRect READ objectRect NOTIFY objectChanged)
    Q_PROPERTY(bool hasObject READ hasObject NOTIFY objectChanged)
    // Every place the last search found, in twips, and how many there were.
    Q_PROPERTY(QVariantList searchRects READ searchRects NOTIFY searchChanged)
    Q_PROPERTY(int searchCount READ searchCount NOTIFY searchChanged)

public:
    enum Status { Empty, Loading, Ready, Error };
    Q_ENUM(Status)
    enum Kind { Unknown, Text, Spreadsheet, Presentation, Drawing };
    Q_ENUM(Kind)

    explicit OfficeDocument(QObject *parent = nullptr);
    ~OfficeDocument() override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    Status status() const { return m_status; }
    QString errorString() const { return m_error; }
    Kind kind() const { return m_kind; }
    QString title() const;
    QSizeF size() const { return m_size; }
    QVariantList pages() const;
    int parts() const { return m_parts; }
    int currentPart() const { return m_currentPart; }
    QStringList partNames() const { return m_partNames; }
    bool modified() const { return m_modified; }
    bool saving() const { return m_saving; }
    bool autosave() const { return m_autosave; }
    void setAutosave(bool on);
    QString saveError() const { return m_saveError; }
    QRectF cursorRect() const { return m_cursor; }
    bool cursorVisible() const { return m_cursorVisible; }
    QVariantList selectionRects() const;
    QRectF selectionStart() const { return m_selectionStart; }
    QRectF selectionEnd() const { return m_selectionEnd; }
    bool hasSelection() const { return !m_selection.isEmpty(); }
    QRectF cellCursor() const { return m_cellCursor; }
    QString cellAddress() const { return m_cellAddress; }
    QString cellFormula() const { return m_cellFormula; }
    QVariantMap states() const { return m_states; }
    bool canUndo() const;
    QVariantList searchRects() const;
    QRectF objectRect() const { return m_object; }
    bool hasObject() const { return !m_object.isEmpty(); }
    int searchCount() const { return m_searchCount; }
    bool canRedo() const;

    // A tap, double tap (a word) or triple tap (a sentence) at a place.
    Q_INVOKABLE void tap(qreal x, qreal y, int count = 1);
    // Pointer press, move and release, for selecting by dragging.
    Q_INVOKABLE void press(qreal x, qreal y, int count = 1, int modifiers = 0);
    Q_INVOKABLE void move(qreal x, qreal y, int modifiers = 0);
    Q_INVOKABLE void release(qreal x, qreal y, int count = 1, int modifiers = 0);
    // Moves one end of the selection to a place, as its handle is dragged.
    Q_INVOKABLE void moveSelectionEnd(bool end, qreal x, qreal y);
    Q_INVOKABLE void clearSelection();

    // Text typed or committed by an input method.
    Q_INVOKABLE void typeText(const QString &text);
    // A key the engine knows by its own code (Key below), with modifier bits.
    Q_INVOKABLE void pressKey(int key, int modifiers = 0);
    // Any of LibreOffice's commands, such as .uno:Bold; arguments are name to value.
    Q_INVOKABLE void command(const QString &uno, const QVariantMap &arguments = {});
    Q_INVOKABLE void undo() { command(QStringLiteral(".uno:Undo")); }
    Q_INVOKABLE void redo() { command(QStringLiteral(".uno:Redo")); }
    Q_INVOKABLE void selectAll() { command(QStringLiteral(".uno:SelectAll")); }

    // Blocking reads, for the clipboard and for tests.
    Q_INVOKABLE QString selectedText();
    Q_INVOKABLE QString commandValues(const QString &command);
    Q_INVOKABLE bool pasteText(const QString &text);

    Q_INVOKABLE void setPart(int part);

    // Find and replace, whole document, ignoring case. findAll marks every
    // place and returns how many; findNext selects the next (or the one
    // before); replace changes the selected one and moves on.
    Q_INVOKABLE int findAll(const QString &text);
    Q_INVOKABLE void findNext(const QString &text, bool backward = false);
    Q_INVOKABLE void replace(const QString &text, const QString &with);
    Q_INVOKABLE int replaceAll(const QString &text, const QString &with);
    Q_INVOKABLE void clearSearch();

    // The fonts the engine can use, by family name, sorted.
    Q_INVOKABLE QStringList fontNames();
    // Puts the page number in the footer's middle or corner, or the header's
    // corner ("bottom-middle", "bottom-corner", "top-corner"), and returns the
    // cursor to where it was.
    Q_INVOKABLE void addPageNumbers(const QString &where);
    // Writes a PDF of the document to a private temporary folder for the
    // desktop's PDF app to print, and returns where; empty if it failed.
    Q_INVOKABLE QUrl printCopy();

    // Saves over the file it came from, in the file's own format.
    Q_INVOKABLE bool save();
    // Writes a copy somewhere else, such as a PDF; the document stays where it was.
    Q_INVOKABLE bool saveCopy(const QUrl &target, const QString &format = {});
    // Moves the document to a new file and keeps editing it there.
    Q_INVOKABLE bool saveAs(const QUrl &target, const QString &format = {});

    // Blocks until every job asked of the engine so far has run. For tests.
    Q_INVOKABLE void settle(int ms = 0);

    // Draws an area of the current part into a picture of the given size,
    // off the interface thread; done is called back on the interface thread.
    void renderTile(int part, const QRectF &area, const QSize &pixels, QObject *context,
                    std::function<void(QImage)> done);

    // Engine key codes (com.sun.star.awt.Key) and modifier bits.
    enum Key {
        KeyDown = 1024, KeyUp = 1025, KeyLeft = 1026, KeyRight = 1027,
        KeyHome = 1028, KeyEnd = 1029, KeyPageUp = 1030, KeyPageDown = 1031,
        KeyReturn = 1280, KeyEscape = 1281, KeyTab = 1282, KeyBackspace = 1283,
        KeySpace = 1284, KeyDelete = 1286,
    };
    Q_ENUM(Key)
    enum Modifier { Shift = 0x1000, Control = 0x2000, Alt = 0x4000 };
    Q_ENUM(Modifier)

Q_SIGNALS:
    void sourceChanged();
    void statusChanged();
    void sizeChanged();
    void partsChanged();
    void modifiedChanged();
    void savingChanged();
    void autosaveChanged();
    void cursorChanged();
    void selectionChanged();
    void cellChanged();
    void statesChanged();
    void searchChanged();
    void objectChanged();
    // An area must be drawn again; an empty area means everything.
    void invalidated(const QRectF &area, int part);
    void saved();

private:
    friend struct DocumentLink;
    void load();
    void close();
    void engineEvent(int type, const QByteArray &payload);
    void setStatus(Status status, const QString &error = {});
    bool writeTo(const QUrl &target, const QString &format, const QByteArray &options);
    void mouse(int type, qreal x, qreal y, int count, int modifiers);
    void refreshShape();

    QUrl m_source;
    Status m_status = Empty;
    QString m_error;
    Kind m_kind = Unknown;
    QSizeF m_size;
    QList<QRectF> m_pages;
    int m_parts = 0;
    int m_currentPart = 0;
    QStringList m_partNames;
    bool m_modified = false;
    bool m_saving = false;
    bool m_autosave = true;
    QString m_saveError;
    QRectF m_cursor;
    bool m_cursorVisible = false;
    QList<QRectF> m_selection;
    QRectF m_selectionStart;
    QRectF m_selectionEnd;
    QRectF m_cellCursor;
    QString m_cellAddress;
    QString m_cellFormula;
    QVariantMap m_states;
    QList<QRectF> m_searchRects;
    QRectF m_object;
    int m_searchCount = 0;
    QTimer m_autosaveTimer;
    std::shared_ptr<DocumentLink> m_link;
};
