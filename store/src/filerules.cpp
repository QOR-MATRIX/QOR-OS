// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "filerules.h"

#include "package.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>
#include <QtCore/QVarLengthArray>
#include <QtCore/QtEndian>
#include <QtGui/QTextDocument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace qor::store {

namespace {

QString extensionOf(const QString &path)
{
    const QString name = path.mid(path.lastIndexOf(u'/') + 1);
    const qsizetype dot = name.lastIndexOf(u'.');
    return dot < 0 ? QString() : name.mid(dot + 1).toLower();
}

bool startsWithRiff(const QByteArray &data, const char *form)
{
    return data.startsWith("RIFF") && data.size() >= 12 && data.mid(8, 4) == form;
}

// Whether a file of a known binary type starts as one. `known` is false for a type not listed here.
bool hasSignature(const QString &extension, const QByteArray &data, bool *known)
{
    *known = true;
    if (extension == u"png"_s)
        return data.startsWith(QByteArrayView("\x89PNG\r\n\x1a\n"));
    if (extension == u"jpg"_s || extension == u"jpeg"_s)
        return data.startsWith(QByteArrayView("\xff\xd8\xff"));
    if (extension == u"webp"_s)
        return startsWithRiff(data, "WEBP");
    if (extension == u"hdr"_s)
        return data.startsWith("#?");
    if (extension == u"ktx"_s)
        return data.startsWith(QByteArrayView("\xabKTX 11\xbb\r\n\x1a\n"));
    if (extension == u"ktx2"_s)
        return data.startsWith(QByteArrayView("\xabKTX 20\xbb\r\n\x1a\n"));
    if (extension == u"wav"_s)
        return startsWithRiff(data, "WAVE");
    if (extension == u"ogg"_s)
        return data.startsWith("OggS");
    if (extension == u"flac"_s)
        return data.startsWith("fLaC");
    if (extension == u"mp3"_s)
        return data.startsWith("ID3")
               || (data.size() >= 2 && quint8(data.at(0)) == 0xff && (quint8(data.at(1)) & 0xe0) == 0xe0);
    *known = false;
    return false;
}

// What a system or a program loader would start. A model's buffer has no signature of its own to check, so a bin is
// looked at for these.
bool looksLikeAProgram(const QByteArray &data)
{
    static const QByteArrayView signatures[] = { "MZ",
                                                 "\x7f"
                                                 "ELF",
                                                 "#!",
                                                 "PK\x03\x04",
                                                 "\xfe\xed\xfa\xce",
                                                 "\xfe\xed\xfa\xcf",
                                                 "\xce\xfa\xed\xfe",
                                                 "\xcf\xfa\xed\xfe",
                                                 "\xca\xfe\xba\xbe" };
    for (const QByteArrayView signature : signatures)
        if (data.startsWith(signature))
            return true;
    return false;
}

// UTF-8, and no control characters but the tab and the line and page breaks text has. Refuses the start of a program as
// well. Fills `text` with what it read, so that a file is decoded once.
bool isPlainText(const QByteArray &data, QString *text)
{
    if (data.startsWith("#!"))
        return false;
    // Stateless: a stateful decoder takes a character cut off at the end for one still to come, and calls that no
    // error.
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    *text = QString(decoder.decode(data));
    if (decoder.hasError())
        return false;
    for (const QChar c : std::as_const(*text)) {
        const char16_t unit = c.unicode();
        if ((unit < 0x20 && unit != u'\t' && unit != u'\n' && unit != u'\r' && unit != u'\f')
            || (unit >= 0x7f && unit <= 0x9f))
            return false;
    }
    return true;
}

// A name a browser acts on and Qt does not. Qt's own are asked of Qt, in isElementName.
bool isBrowserElementName(QStringView name)
{
    static const QSet<QString> names { u"abbr"_s,     u"applet"_s,    u"audio"_s,   u"base"_s,     u"bgsound"_s,
                                       u"button"_s,   u"canvas"_s,    u"del"_s,     u"details"_s,  u"dialog"_s,
                                       u"embed"_s,    u"form"_s,      u"frame"_s,   u"frameset"_s, u"iframe"_s,
                                       u"input"_s,    u"ins"_s,       u"isindex"_s, u"keygen"_s,   u"link"_s,
                                       u"listing"_s,  u"marquee"_s,   u"math"_s,    u"noscript"_s, u"object"_s,
                                       u"picture"_s,  u"plaintext"_s, u"q"_s,       u"script"_s,   u"select"_s,
                                       u"source"_s,   u"strike"_s,    u"summary"_s, u"svg"_s,      u"template"_s,
                                       u"textarea"_s, u"track"_s,     u"video"_s,   u"xmp"_s };
    return names.contains(name.toString());
}

// Whether a tag's name, in lower case, means something to Qt's rich text or to a browser. Qt is asked, so its table is
// the one in the kit and not a copy that drifts: <name> at the start of a text is rich exactly when it names an
// element.
bool isElementName(QStringView name)
{
    QVarLengthArray<char16_t, 32> tag;
    tag.append(u'<');
    tag.append(name.utf16(), name.size());
    tag.append(u'>');
    return Qt::mightBeRichText(QStringView(tag.constData(), tag.size())) || isBrowserElementName(name);
}

// Whether Qt's own parser would give any tag an attribute with a value. It reads the text as QTextHtmlParser does
// (parse, parseTag, parseWord, parseAttributes), one tag after another, so that a quoted word inside a tag, which may
// hold a `>`, is one word, as it is to Qt: <x ">" style="..."> carries a style. It keeps nothing but where it is, and
// passes each character once. An entity is not decoded, since none can hold what ends a word or a tag.
bool qtGivesATagAValue(QStringView text)
{
    const qsizetype size = text.size();
    qsizetype pos = 0;
    const auto at = [&](qsizetype i) { return i < size ? text[i] : QChar(); };
    const auto eatSpace = [&] {
        while (pos < size && text[pos].isSpace() && text[pos] != QChar::ParagraphSeparator)
            ++pos;
    };
    // QTextHtmlParser::parseWord. Whether the word has anything in it.
    const auto parseWord = [&] {
        const qsizetype start = pos;
        if (at(pos) == u'"') {
            ++pos;
            qsizetype length = 0;
            while (pos < size) {
                if (text[pos++] == u'"')
                    break;
                ++length;
            }
            return length > 0;
        }
        if (at(pos) == u'\'') {
            ++pos;
            qsizetype length = 0;
            while (pos < size) {
                const QChar c = text[pos++];
                // Qt lets a backslash keep a single quote in the word.
                if (c == u'\'' && text[pos - 2] != u'\\')
                    break;
                ++length;
            }
            return length > 0;
        }
        while (pos < size) {
            const QChar c = text[pos];
            if (c == u'>' || (c == u'/' && at(pos + 1) == u'>') || c == u'<' || c == u'=' || c.isSpace())
                break;
            ++pos;
        }
        return pos > start;
    };

    while (pos < size) {
        if (text[pos++] != u'<')
            continue;
        eatSpace();
        if (at(pos) == u'!')
            return true; // a comment or a declaration, markup of its own
        if (at(pos) == u'/') {
            ++pos;
            parseWord();
        } else {
            parseWord();
            // Qt needs a space after the name before it reads any attribute.
            if (pos < size && text[pos].isSpace()) {
                while (pos < size) {
                    eatSpace();
                    if (at(pos) == u'>' || at(pos) == u'/' || !parseWord())
                        break;
                    eatSpace();
                    if (at(pos) == u'=')
                        return true;
                }
            }
        }
        // Qt then skips to the `>` that ends the tag, quotes or not.
        while (pos < size && text[pos] != u'>')
            ++pos;
        ++pos;
    }
    return false;
}

// Whether a text holds markup that a Qt Text, or a browser it reached, would act on. One pass over the text, with no
// pattern to backtrack and nothing that can fail open: it either finds a tag or reaches the end.
//
// Qt::mightBeRichText says whether a Text set to guess would treat the whole of it as rich. It looks only at the first
// line, so a tag further down is inert until that line turns rich on. Those are refused as well, because what a text is
// shown by is not known here, and one later change to the first line would wake them all.
//
// This reads a tag as Qt's parser does (QTextHtmlParser::parseTag and parseWord): spaces of any kind may come after the
// `<` and after a `/`, a name may be quoted, and a name holding an entity is decoded. A closing `>` is not required,
// since Qt's parser takes the tag either way. It stops a name at a `/` as well, as browsers do, for <img/src=x>.
//
// A tag is markup if Qt or a browser knows its name, and whatever its name if it carries a value: an `=` before the `>`
// that ends it, or before the end of the text when nothing does. Qt applies `style` to a tag it does not know, and a
// style can fetch a picture; a browser runs an event handler on any tag. What is left is what licences hold: plain
// words in angle brackets, such as the GPL's <year> or <one line to give the program's name...>, an address or a link.
// A `>` inside quotes does not end a tag for Qt, so Qt's own reading is asked as well (qtGivesATagAValue); the stretch
// below ends a tag at any `>`, as a browser ends a tag whose name holds a quote.
bool hasMarkup(QStringView text)
{
    if (Qt::mightBeRichText(text) || qtGivesATagAValue(text))
        return true;

    // Longer than any element's name: "blockquote" for Qt, "frameset" and "plaintext" for browsers.
    constexpr qsizetype kLongestName = 16;
    const qsizetype size = text.size();
    // Every `<` before the same `>` opens a tag that ends there, so each stretch up to a `>` is read once, for that `>`
    // and the last `=` before it, and a text of nothing but `<` stays one pass.
    qsizetype tagEnd = -1;
    qsizetype lastValue = -1;
    for (qsizetype open = text.indexOf(u'<'); open >= 0; open = text.indexOf(u'<', open + 1)) {
        if (open > tagEnd) {
            lastValue = -1;
            for (tagEnd = open; tagEnd < size && text[tagEnd] != u'>'; ++tagEnd)
                if (text[tagEnd] == u'=')
                    lastValue = tagEnd;
        }
        qsizetype at = open + 1;
        while (at < size && text[at].isSpace())
            ++at;
        if (at == size)
            return false;
        if (text[at] == u'!' || text[at] == u'?') // a comment, a doctype, an XML declaration
            return true;
        if (text[at] == u'/') {
            ++at;
            while (at < size && text[at].isSpace())
                ++at;
            if (at == size)
                return false;
        }
        QChar quote;
        if (text[at] == u'"' || text[at] == u'\'')
            quote = text[at++];

        QVarLengthArray<char16_t, kLongestName + 1> name;
        for (; at < size && name.size() <= kLongestName; ++at) {
            const QChar c = text[at];
            if (quote.isNull() ? (c.isSpace() || c == u'>' || c == u'/' || c == u'<' || c == u'=') : c == quote)
                break;
            if (c == u'&') // Qt decodes an entity in a name, so <&#105;mg> is <img>
                return true;
            name.append(c.toLower().unicode());
        }
        if (name.isEmpty())
            continue;
        if (name.size() <= kLongestName && isElementName(QStringView(name.constData(), name.size())))
            return true;
        if (lastValue >= at) // a value after the name, whatever the name
            return true;
    }
    return false;
}

// What a model may carry inside itself. Pictures and buffers: no SVG, script or page, which a model has no use for.
bool isPassiveData(const QString &uri)
{
    static const QStringList kinds { u"data:application/octet-stream;base64,"_s,
                                     u"data:application/gltf-buffer;base64,"_s,
                                     u"data:image/png;base64,"_s,
                                     u"data:image/jpeg;base64,"_s,
                                     u"data:image/webp;base64,"_s,
                                     u"data:image/ktx2;base64,"_s };
    return std::any_of(kinds.cbegin(), kinds.cend(), [&uri](const QString &kind) { return uri.startsWith(kind); });
}

// Every value under a key called uri, and how many such keys there are of any type.
void collectUris(const QJsonValue &value, QStringList *uris, qsizetype *keys)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == u"uri"_s) {
                ++*keys;
                if (it.value().isString())
                    uris->append(it.value().toString());
            }
            collectUris(it.value(), uris, keys);
        }
    } else if (value.isArray()) {
        for (const QJsonValue &element : value.toArray())
            collectUris(element, uris, keys);
    }
}

// How many keys called uri a JSON text has, however they are written. Qt's parser keeps only the last of duplicate keys
// and another reader may take the first, so the parsed count falling short of this means one is hidden. The text has
// already parsed, so this need not check its shape: one pass over the strings.
qsizetype countUriKeys(QByteArrayView json)
{
    qsizetype count = 0;
    const qsizetype size = json.size();
    for (qsizetype at = 0; at < size; ++at) {
        if (json[at] != '"')
            continue;
        // Only the first few characters are kept, enough to tell "uri", with its letters perhaps escaped.
        QVarLengthArray<char, 4> word;
        for (++at; at < size && json[at] != '"'; ++at) {
            char c = json[at];
            if (c == '\\' && at + 1 < size) {
                ++at;
                c = json[at];
                if (c == 'u' && at + 4 < size) {
                    const QByteArray hex = json.sliced(at + 1, 4).toByteArray();
                    bool ok = false;
                    const uint unit = hex.toUInt(&ok, 16);
                    at += 4;
                    c = ok && unit < 0x80 ? char(unit) : '\0';
                }
            }
            if (word.size() < 4)
                word.append(c);
        }
        qsizetype next = at + 1;
        while (next < size && (json[next] == ' ' || json[next] == '\t' || json[next] == '\n' || json[next] == '\r'))
            ++next;
        if (next < size && json[next] == ':' && QByteArrayView(word.constData(), word.size()) == QByteArrayView("uri"))
            ++count;
    }
    return count;
}

// The JSON at the head of a model: all of a .gltf, the first chunk of a .glb. Empty if it is not one.
QByteArray modelJsonText(const QString &extension, const QByteArray &data)
{
    if (extension != u"glb"_s)
        return data;
    constexpr qsizetype kHeader = 12;
    constexpr qsizetype kChunkHeader = 8;
    if (data.size() < kHeader + kChunkHeader || !data.startsWith("glTF"))
        return {};
    const char *bytes = data.constData();
    const quint32 version = qFromLittleEndian<quint32>(bytes + 4);
    const quint32 length = qFromLittleEndian<quint32>(bytes + 8);
    const quint32 chunk = qFromLittleEndian<quint32>(bytes + kHeader);
    if (version != 2 || length != quint64(data.size()) || chunk > quint64(data.size() - kHeader - kChunkHeader)
        || QByteArrayView(bytes + kHeader + 4, 4) != QByteArrayView("JSON"))
        return {};
    return data.mid(kHeader + kChunkHeader, chunk);
}

// Every file a model names must be beside it in the package. Adds them to `named`.
QString refuseModel(const QString &path, const QString &extension, const QMap<QString, QByteArray> &files,
                    QSet<QString> *named)
{
    const QByteArray json = modelJsonText(extension, files.value(path));
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject() || !document.object().contains(u"asset"_s))
        return u"%1 is not a model it can read."_s.arg(path);
    QStringList uris;
    qsizetype keys = 0;
    collectUris(document.object(), &uris, &keys);
    if (keys != countUriKeys(json))
        return u"%1 names one thing twice, and two readers would not agree on which."_s.arg(path);
    if (qsizetype(uris.size()) != keys)
        return u"%1 names a file by something other than text."_s.arg(path);
    const QString folder = path.left(path.lastIndexOf(u'/') + 1);
    for (const QString &uri : std::as_const(uris)) {
        if (uri.startsWith(u"data:"_s)) {
            if (!isPassiveData(uri))
                return u"%1 carries data of a kind a model may not."_s.arg(path);
            continue;
        }
        // isSafePackagePath takes no scheme, no percent sign and no climbing, so there is no second reading of this.
        if (!isSafePackagePath(uri) || !files.contains(folder + uri))
            return u"%1 names a file outside the package."_s.arg(path);
        named->insert(folder + uri);
    }
    return {};
}

} // namespace

bool isPreviewName(const QString &path)
{
    static const QSet<QString> pictures { u"png"_s, u"jpg"_s, u"jpeg"_s, u"webp"_s };
    return pictures.contains(extensionOf(path));
}

QString refusePreviewFile(const QString &path, const QByteArray &data)
{
    bool known = false;
    if (!isPreviewName(path) || !hasSignature(extensionOf(path), data, &known) || !known)
        return u"The preview is not a picture."_s;
    return {};
}

QString refusePackageFiles(const QString &kind, const QMap<QString, QByteArray> &files)
{
    const bool isGame = kind == u"game"_s;

    QSet<QString> named;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString extension = extensionOf(it.key());
        if (extension == u"gltf"_s || extension == u"glb"_s) {
            if (const QString why = refuseModel(it.key(), extension, files, &named); !why.isEmpty())
                return why;
        }
    }

    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString &path = it.key();
        const QByteArray &data = it.value();
        const QString extension = extensionOf(path);
        bool known = false;
        const bool signedLikeIt = hasSignature(extension, data, &known);
        if (known) {
            if (!signedLikeIt)
                return u"%1 is not what its name says."_s.arg(path);
            continue;
        }
        if (extension == u"gltf"_s || extension == u"glb"_s)
            continue; // read above
        if (extension == u"bin"_s) {
            if (!named.contains(path))
                return u"%1 is not named by a model in the package."_s.arg(path);
            if (looksLikeAProgram(data))
                return u"%1 is a program."_s.arg(path);
            continue;
        }
        if (extension == u"txt"_s) {
            QString text;
            if (!isPlainText(data, &text))
                return u"%1 is not plain text."_s.arg(path);
            if (hasMarkup(text))
                return u"%1 holds markup."_s.arg(path);
            continue;
        }
        if (isGame && (extension == u"qml"_s || extension == u"mjs"_s || extension == u"js"_s)) {
            QString text;
            if (!isPlainText(data, &text))
                return u"%1 is not plain text."_s.arg(path);
            continue;
        }
        if (isGame && (extension == u"json"_s || extension == u"tmj"_s || extension == u"tsj"_s)) {
            if (QJsonDocument::fromJson(data).isNull())
                return u"%1 is not JSON."_s.arg(path);
            continue;
        }
        return u"A package of kind %1 may not hold %2."_s.arg(kind, path);
    }
    return {};
}

} // namespace qor::store
