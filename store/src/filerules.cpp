// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "filerules.h"

#include "package.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>
#include <QtCore/QtEndian>

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

// UTF-8, and no control characters but the ones text has. Refuses the start of a program as well.
bool isPlainText(const QByteArray &data)
{
    if (data.startsWith("#!"))
        return false;
    for (const char c : data) {
        const auto byte = quint8(c);
        if ((byte < 0x20 && byte != '\t' && byte != '\n' && byte != '\r' && byte != '\f') || byte == 0x7f)
            return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded = decoder.decode(data);
    return !decoder.hasError();
}

// A tag a browser or Qt's rich text would act on. Licences do hold angle brackets as plain words (the GPL's
// <one line to give the program's name>, or <https://www.gnu.org/licenses/>), so these are the names that count.
bool hasMarkup(const QByteArray &data)
{
    static const QRegularExpression tag(
        u"<\\s*/?\\s*(a|abbr|address|audio|b|base|big|blockquote|body|br|button|center|cite|code|dd|del|dfn|div|dl|dt|"
        "em|embed|font|form|h[1-6]|head|hr|html|i|iframe|img|input|ins|kbd|li|link|meta|nobr|object|ol|p|pre|q|qt|s|"
        "samp|"
        "script|small|span|strike|strong|style|sub|sup|svg|table|tbody|td|tfoot|th|thead|title|tr|tt|u|ul|var|video)"
        "(\\s[^<>]*)?/?>|<!--|<!doctype|<\\?xml"_s,
        QRegularExpression::CaseInsensitiveOption);
    return tag.match(QString::fromUtf8(data)).hasMatch();
}

void collectUris(const QJsonValue &value, QStringList *uris)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == u"uri"_s && it.value().isString())
                uris->append(it.value().toString());
            else
                collectUris(it.value(), uris);
        }
    } else if (value.isArray()) {
        for (const QJsonValue &element : value.toArray())
            collectUris(element, uris);
    }
}

// The JSON at the head of a model: all of a .gltf, the first chunk of a .glb. Null if it is not one.
QJsonObject modelJson(const QString &extension, const QByteArray &data)
{
    QByteArray json = data;
    if (extension == u"glb"_s) {
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
        json = data.mid(kHeader + kChunkHeader, chunk);
    }
    const QJsonDocument document = QJsonDocument::fromJson(json);
    return document.isObject() && document.object().contains(u"asset"_s) ? document.object() : QJsonObject();
}

// Every file a model names must be beside it in the package. Adds them to `named`.
QString refuseModel(const QString &path, const QString &extension, const QMap<QString, QByteArray> &files,
                    QSet<QString> *named)
{
    const QJsonObject model = modelJson(extension, files.value(path));
    if (model.isEmpty())
        return u"%1 is not a model it can read."_s.arg(path);
    QStringList uris;
    collectUris(model, &uris);
    const QString folder = path.left(path.lastIndexOf(u'/') + 1);
    for (const QString &uri : std::as_const(uris)) {
        if (uri.startsWith(u"data:application/"_s) || uri.startsWith(u"data:image/"_s))
            continue;
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
            if (!isPlainText(data))
                return u"%1 is not plain text."_s.arg(path);
            if (hasMarkup(data))
                return u"%1 holds markup."_s.arg(path);
            continue;
        }
        if (isGame && (extension == u"qml"_s || extension == u"mjs"_s || extension == u"js"_s)) {
            if (!isPlainText(data))
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
