#include "textdocument.h"

#include <QFile>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextStream>
#include <spdlog/spdlog.h>

namespace Core {

/*!
 * \qmltype TextDocument
 * \brief Document object for text files.
 * \instantiates Core::TextDocument
 * \inqmlmodule Script
 * \since 4.0
 * \inherits Document
 */

/*!
 * \qmlproperty int TextDocument::column
 * This read-only property holds the column of the cursor position.
 * Be careful the column is 1-based, so the column before the first character is 1.
 */
/*!
 * \qmlproperty int TextDocument::line
 * This read-only property holds the line of the cursor position.
 * Be careful the line is 1-based, so the first line of the document is 1,
 */
/*!
 * \qmlproperty int TextDocument::lineCount
 * This read-only property holds the number of lines in the document.
 */
/*!
 * \qmlproperty int TextDocument::position
 * This property holds the absolute position of the cursor inside the text document.
 */
/*!
 * \qmlproperty string TextDocument::text
 * This property holds the text of the document.
 */
/*!
 * \qmlproperty string TextDocument::selectedText
 * This property holds the selected text of the document.
 */
/*!
 * \qmlproperty LineEnding TextDocument::lineEnding
 * This property holds the line ending for the document. It can be one of those choices:
 *
 * - `TextDocument.LFLineEnding`: '\n' character
 * - `TextDocument.CRLFLineEnding`: '\r\n' characters
 * - `TextDocument.NativeLineEnding`: LF on Linux and Mac, CRLF on Windows
 *
 * Native is the default for new documents.
 */
/*!
 * \qmlproperty string TextDocument::currentLine
 * This read-only property return the line under the current position.
 */
/*!
 * \qmlproperty string TextDocument::currentWord
 * This read-only property return the word under the current position.
 */

TextDocument::TextDocument(QObject *parent)
    : TextDocument(Document::Type::Text, parent)
{
}

TextDocument::~TextDocument() { }

TextDocument::TextDocument(Type type, QObject *parent)
    : Document(type, parent)
    , m_document(new QPlainTextEdit())
{
    m_document->hide();
    connect(m_document.get(), &QPlainTextEdit::textChanged, this, &TextDocument::textChanged);
    connect(m_document.get(), &QPlainTextEdit::selectionChanged, this, &TextDocument::selectionChanged);
    connect(m_document.get(), &QPlainTextEdit::cursorPositionChanged, this, &TextDocument::positionChanged);
}

bool TextDocument::doSave(const QString &fileName)
{
    Q_ASSERT(!fileName.isEmpty());

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setErrorString(file.errorString());
        spdlog::error("Can't save file {}: {}", fileName.toStdString(), errorString().toStdString());
        return false;
    }

    if (m_utf8Bom)
        file.write("\xef\xbb\xbf", 3);

    QString plainText = m_document->toPlainText();
    if (m_lineEnding == CRLFLineEnding)
        plainText.replace('\n', "\r\n");

    QTextStream stream(&file);
    stream << plainText;
    return true;
}

bool TextDocument::doLoad(const QString &fileName)
{
    Q_ASSERT(!fileName.isEmpty());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        setErrorString(file.errorString());
        spdlog::warn("Can't load file {}: {}", fileName.toStdString(), errorString().toStdString());
        return false;
    }

    QByteArray data = file.readAll();
    detectFormat(data);
    QTextStream stream(data);
    const QString text = stream.readAll();

    // This will replace '\r\n' with '\n'
    setText(text);
    return true;
}

// This function is copied from TextFileFormat::detect from Qt Creator.
void TextDocument::detectFormat(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    const int bytesRead = data.size();
    const auto buf = reinterpret_cast<const unsigned char *>(data.constData());
    // code taken from qtextstream
    if (bytesRead >= 3 && ((buf[0] == 0xef && buf[1] == 0xbb) && buf[2] == 0xbf))
        m_utf8Bom = true;

    // end code taken from qtextstream
    const int newLinePos = data.indexOf('\n');
    if (newLinePos == -1)
        setLineEnding(NativeLineEnding);
    else if (newLinePos == 0)
        setLineEnding(LFLineEnding);
    else
        setLineEnding(data.at(newLinePos - 1) == '\r' ? CRLFLineEnding : LFLineEnding);
}

int TextDocument::column() const
{
    const QTextCursor cursor = m_document->textCursor();
    return cursor.positionInBlock() + 1;
}

int TextDocument::line() const
{
    const QTextCursor cursor = m_document->textCursor();
    return cursor.blockNumber() + 1;
}

int TextDocument::lineCount() const
{
    return m_document->document()->lineCount();
}

int TextDocument::position() const
{
    return m_document->textCursor().position();
}

void TextDocument::setPosition(int newPosition)
{
    if (position() == newPosition)
        return;
    auto cursor = m_document->textCursor();
    cursor.setPosition(newPosition);
    m_document->setTextCursor(cursor);
    emit positionChanged();
}

void TextDocument::convertPosition(int pos, int *line, int *column) const
{
    Q_ASSERT(line && column);
    QTextBlock block = m_document->document()->findBlock(pos);
    if (!block.isValid()) {
        (*line) = -1;
        (*column) = -1;
    } else {
        // line and column are both 1-based
        (*line) = block.blockNumber() + 1;
        (*column) = pos - block.position() + 1;
    }
}

QString TextDocument::text() const
{
    return m_document->toPlainText();
}

void TextDocument::setText(const QString &newText)
{
    setHasChanged(true);
    m_document->setPlainText(newText);
}

QString TextDocument::currentLine() const
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    return cursor.selectedText();
}

QString TextDocument::currentWord() const
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::StartOfWord);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    return cursor.selectedText();
}

QString TextDocument::selectedText() const
{
    // Replace \u2029 with \n
    return m_document->textCursor().selectedText().replace(QChar(8233), "\n");
}

TextDocument::LineEnding TextDocument::lineEnding() const
{
    return m_lineEnding;
}

bool TextDocument::hasUtf8Bom() const
{
    return m_utf8Bom;
}

/*!
 * \qmlmethod TextDocument::undo()
 * Undo the last action.
 */
void TextDocument::undo()
{
    m_document->undo();
}

/*!
 * \qmlmethod TextDocument::redo()
 * Redo the last action.
 */
void TextDocument::redo()
{
    m_document->redo();
}

void TextDocument::movePosition(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode, int count)
{
    for (int i = 0; i < count; ++i) {
        m_document->moveCursor(operation, mode);
        m_document->setTextCursor(m_document->textCursor());
    }
}

/*!
 * \qmlmethod TextDocument::gotoLine( int line, int column = 1)
 * Goes to the given `line` and `column` in the editor. Lines and columns are 1-based.
 * \sa TextDocument::line
 * \sa TextDocument::column
 */
void TextDocument::gotoLine(int line, int column)
{
    // Internally, columns are 0-based, while 1-based on the API
    column = column - 1;
    const int blockNumber = qMin(line, m_document->document()->blockCount()) - 1;
    const QTextBlock &block = m_document->document()->findBlockByNumber(blockNumber);
    if (block.isValid()) {
        QTextCursor cursor(block);
        if (column > 0)
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);

        m_document->setTextCursor(cursor);
    }
}

/*!
 * \qmlmethod TextDocument::gotoLineStart()
 * Goes to the start of the line.
 */
void TextDocument::gotoLineStart()
{
    movePosition(QTextCursor::StartOfLine);
}

/*!
 * \qmlmethod TextDocument::gotoLineEnd()
 * Goes to the end of the line.
 */
void TextDocument::gotoLineEnd()
{
    movePosition(QTextCursor::EndOfLine);
}

/*!
 * \qmlmethod TextDocument::gotoWordStart()
 * Goes to the start of the word under the cursor.
 */
void TextDocument::gotoWordStart()
{
    movePosition(QTextCursor::StartOfWord);
}

/*!
 * \qmlmethod TextDocument::gotoWordEnd()
 * Goes to the end of the word under the cursor.
 */
void TextDocument::gotoWordEnd()
{
    movePosition(QTextCursor::EndOfWord);
}

/*!
 * \qmlmethod TextDocument::gotoNextLine( int count = 1)
 * Goes to the next line, repeat the operation `count` times.
 */
void TextDocument::gotoNextLine(int count)
{
    movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoPreviousLine( int count = 1)
 * Goes to the previous line, repeat the operation `count` times.
 */
void TextDocument::gotoPreviousLine(int count)
{
    movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoPreviousCharacter( int count = 1)
 * Goes to the previous character, repeat the operation `count` times.
 */
void TextDocument::gotoPreviousCharacter(int count)
{
    movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoNextCharacter( int count = 1)
 * Goes to the next character, repeat the operation `count` times.
 */
void TextDocument::gotoNextCharacter(int count)
{
    movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoPreviousWord( int count = 1)
 * Goes to the previous word, repeat the operation `count` times.
 */
void TextDocument::gotoPreviousWord(int count)
{
    movePosition(QTextCursor::PreviousWord, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoNextWord( int count = 1)
 * Goes to the next word, repeat the operation `count` times.
 */
void TextDocument::gotoNextWord(int count)
{
    movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor, count);
}

/*!
 * \qmlmethod TextDocument::gotoDocumentStart()
 * Goes to the document start.
 */
void TextDocument::gotoDocumentStart()
{
    movePosition(QTextCursor::Start);
}

/*!
 * \qmlmethod TextDocument::gotoDocumentEnd()
 * Goes to the document end.
 */
void TextDocument::gotoDocumentEnd()
{
    movePosition(QTextCursor::End);
}

/*!
 * \qmlmethod TextDocument::unselect()
 * Clears the current selection.
 */
void TextDocument::unselect()
{
    QTextCursor cursor = m_document->textCursor();
    cursor.clearSelection();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod bool TextDocument::hasSelection()
 * Returns true if the editor has a selection.
 */
bool TextDocument::hasSelection()
{
    return m_document->textCursor().hasSelection();
}

/*!
 * \qmlmethod TextDocument::selectAll()
 * Selects all the text.
 */
void TextDocument::selectAll()
{
    m_document->selectAll();
}

/*!
 * \qmlmethod TextDocument::selectTo( int pos)
 * Selects the text from the current position to `pos`.
 */
void TextDocument::selectTo(int pos)
{
    QTextCursor cursor = m_document->textCursor();
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::selectLineStart( int count = 1)
 * Selects the text from the current position to the start of the line.
 */
void TextDocument::selectLineStart()
{
    movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
}

/*!
 * \qmlmethod TextDocument::selectLineEnd()
 * Selects the text from the current position to the end of the line.
 */
void TextDocument::selectLineEnd()
{
    movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
}

/*!
 * \qmlmethod TextDocument::selectWordStart()
 * Selects the text from the current position to the start of the word.
 */
void TextDocument::selectWordStart()
{
    movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
}

/*!
 * \qmlmethod TextDocument::selectWordEnd()
 * Selects the text from the current position to the end of the word.
 */
void TextDocument::selectWordEnd()
{
    movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
}

/*!
 * \qmlmethod TextDocument::selectNextLine( int count = 1)
 * Selects the next line, repeat the operation \a count times.
 */
void TextDocument::selectNextLine(int count)
{
    movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::selectPreviousLine( int count = 1)
 * Selectes the previous line, repeat the operation `count` times.
 */
void TextDocument::selectPreviousLine(int count)
{
    movePosition(QTextCursor::Up, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::selectPreviousCharacter( int count = 1)
 * Selects the previous character, repeat the operation `count` times.
 */
void TextDocument::selectPreviousCharacter(int count)
{
    movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::selectNextCharacter( int count = 1)
 * Selects the next character, repeat the operation `count` times.
 */
void TextDocument::selectNextCharacter(int count)
{
    movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::selectPreviousWord( int count = 1)
 * Selects the previous word, repeat the operation `count` times.
 */
void TextDocument::selectPreviousWord(int count)
{
    movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::selectNextWord( int count = 1)
 * Selects the next word, repeat the operation `count` times.
 */
void TextDocument::selectNextWord(int count)
{
    movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor, count);
}

/*!
 * \qmlmethod TextDocument::copy()
 * Copies the selected text.
 */
void TextDocument::copy()
{
    m_document->copy();
}

/*!
 * \qmlmethod TextDocument::paste()
 * Pastes text in the clipboard.
 */
void TextDocument::paste()
{
    m_document->paste();
}

/*!
 * \qmlmethod TextDocument::cut()
 * Cuts the selected text.
 */
void TextDocument::cut()
{
    m_document->cut();
}

/*!
 * \qmlmethod TextEditor::remove( int length)
 * Remove `length` character from the current position.
 */
void TextDocument::remove(int length)
{
    QTextCursor cursor = m_document->textCursor();
    cursor.setPosition(cursor.position() + length, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextEditor::insert( string text)
 * Inserts the string `text` at the current position.
 */
void TextDocument::insert(const QString &text)
{
    m_document->insertPlainText(text);
}

/*!
 * \qmlmethod TextEditor::replace( int length, string text)
 * Replaces `length` characters from the current position with the string `text`.
 */
void TextDocument::replace(int length, const QString &text)
{
    QTextCursor cursor = m_document->textCursor();
    cursor.setPosition(cursor.position() + length, QTextCursor::KeepAnchor);
    cursor.insertText(text);
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteSelection()
 * Deletes the current selection, does nothing if no text is selected.
 */
void TextDocument::deleteSelection()
{
    m_document->textCursor().removeSelectedText();
}

/*!
 * \qmlmethod TextDocument::deleteRegion( int from, int to)
 * Deletes the text between `from` and `to` positions.
 */
void TextDocument::deleteRegion(int from, int to)
{
    QTextCursor cursor(m_document->document());
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteEndOfLine()
 * Deletes from the cursor position to the end of the line.
 */
void TextDocument::deleteEndOfLine()
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteStartOfLine()
 * Deletes from the cursor position to the start of the line.
 */
void TextDocument::deleteStartOfLine()
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteEndOfWord()
 * Deletes from the cursor position to the end of the word.
 */
void TextDocument::deleteEndOfWord()
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteStartOfWord()
 * Deletes from the cursor position to the start of the word.
 */
void TextDocument::deleteStartOfWord()
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deletePreviousCharacter(int count)
 * Deletes the previous `count` characters.
 */
void TextDocument::deletePreviousCharacter(int count)
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, count);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

/*!
 * \qmlmethod TextDocument::deleteNextCharacter(int count)
 * Deletes the next `count` characters.
 */
void TextDocument::deleteNextCharacter(int count)
{
    QTextCursor cursor = m_document->textCursor();
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, count);
    cursor.removeSelectedText();
    m_document->setTextCursor(cursor);
}

void TextDocument::setLineEnding(LineEnding newLineEnding)
{
    if (m_lineEnding == newLineEnding)
        return;
    m_lineEnding = newLineEnding;
    emit lineEndingChanged();
}

} // namespace Core
