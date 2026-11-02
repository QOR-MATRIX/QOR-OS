// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "gamebuild.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtEndian>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "confinement.h"
#include "projectworkspace.h"
#include "store.h"

namespace qq {

namespace {

const char *const manifestName = "game.json";
/// What was built before is read only for how many times, so this is more than a real one needs.
constexpr qint64 largestManifestRead = 64 * 1024 * 1024;
constexpr int longestTitle = 80;
/// The most of one file a build looks through for what it names. A model holds its buffers as text, so it may be large.
constexpr qint64 largestCodeLookedThrough = 16 * 1024 * 1024;
constexpr qint64 largestModelLookedThrough = 1024 * 1024 * 1024;
/// The longest name a model or a map can give a file: no path is longer, and a model's data: URI, which names none, can be
/// most of a gigabyte. A longer one that is not data: is refused, not cut short and looked at.
constexpr qsizetype longestNameKept = 4096;
/// Logic plays a sound or places a prefab by a path from the scene playing it (Logic.qml's play() and spawn()), so a string in
/// code is also read from the folders the game's scenes are in: at most this many of them, the first in order.
constexpr int mostSceneFoldersTried = 8;
/// How many of what was left out are listed, however many there are.
constexpr int mostLeftOutListed = 100;

QString tr(const char *text)
{
    return GameBuild::tr(text);
}

QVariantMap refused(const QString &problem)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("problem"), problem}};
}

/// Open a plain file under `root` for reading: false if it cannot be read, or is not what the walk saw. On POSIX it goes down
/// from `root` a name at a time, following no link at any step and waiting on no pipe, and is looked at through what was opened,
/// so nothing swapped in after the walk is followed. Windows has no such open here: see gamebuild.h for what that leaves.
bool openPlain(QFile &file, const QString &root, const QString &relative)
{
#ifdef Q_OS_UNIX
    int folder = ::open(QFile::encodeName(root).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const QStringList parts = relative.split(QLatin1Char('/'));
    for (qsizetype i = 0; folder >= 0 && i + 1 < parts.size(); ++i) {
        const int next = ::openat(folder, QFile::encodeName(parts.at(i)).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(folder);
        folder = next;
    }
    if (folder < 0)
        return false;
    const int opened = ::openat(folder, QFile::encodeName(parts.last()).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    ::close(folder);
    if (opened < 0)
        return false;
    struct stat seen{};
    if (::fstat(opened, &seen) != 0 || !S_ISREG(seen.st_mode) || !file.open(opened, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(opened);
        return false;
    }
    return true;
#else
    const QFileInfo info(QDir(root).filePath(relative));
    if (info.isSymLink() || info.isJunction() || !info.isFile())
        return false;
    file.setFileName(info.filePath());
    return file.open(QIODevice::ReadOnly);
#endif
}

/// Whether a folder can be listed. QDir says nothing when it cannot, which would read as a folder with nothing in it.
bool canList(const QString &folder)
{
    std::error_code error;
    const std::filesystem::directory_iterator listing(QFileInfo(folder).filesystemFilePath(), error);
    return !error;
}

/// What Qt reads in place of a source file, beside it, as code.
bool isCompiledCode(const QString &name)
{
    for (const QLatin1StringView suffix : {QLatin1StringView(".qmlc"), QLatin1StringView(".jsc"), QLatin1StringView(".mjsc")}) {
        if (name.endsWith(suffix, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

struct Found {
    QStringList files;
    QStringList leftOut;
    int leftOutCount = 0;
    QString problem;
};

/// Every file under `relative`, or the first thing that keeps the game from being whole, in `problem`.
void walk(const QString &root, const QString &relative, int mostFiles, Found &found)
{
    const QString here = QDir(root).filePath(relative);
    if (!canList(here)) {
        found.problem = tr("%1 could not be read, so the game would be missing what is in it.").arg(relative);
        return;
    }
    const QFileInfoList entries = QDir(here).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString path = relative + QLatin1Char('/') + entry.fileName();
        if (isCompiledCode(entry.fileName())) {
            found.problem = tr("%1 is compiled code, which a game may not carry: Qt would run it in place of its source. "
                               "Delete it, then build.").arg(path);
            return;
        }
        if (entry.fileName().startsWith(QLatin1Char('.'))) {
            if (found.leftOut.size() < mostLeftOutListed)
                found.leftOut << path;
            ++found.leftOutCount;
            continue;
        }
        if (entry.isSymLink() || entry.isJunction()) {
            found.problem = tr("%1 is a link. A game carries its own files: copy the file there, then build.").arg(path);
            return;
        }
        if (!ProjectWorkspace::plainRelative(path)) {
            found.problem = tr("%1 has a name a game cannot carry: rename it, then build.").arg(path);
            return;
        }
        if (entry.isDir()) {
            walk(root, path, mostFiles, found);
            if (!found.problem.isEmpty())
                return;
        } else if (entry.isFile()) {
            if (found.files.size() >= mostFiles) {
                found.problem = tr("This project has more than %1 files, too many for one game.").arg(mostFiles);
                return;
            }
            found.files << path;
        } else {
            found.problem = tr("%1 is neither a file nor a folder.").arg(path);
            return;
        }
    }
}

/// Copy a file, counting from what is read and not from what the file claims, since a file can grow. "" or what went wrong.
QString copyCounted(const QString &root, const QString &relative, const QString &to, qint64 mostBytes, qint64 &bytes, QByteArray &sha256)
{
    QFile in;
    if (!openPlain(in, root, relative))
        return tr("%1 could not be read, or is no longer a plain file.").arg(relative);
    if (!QDir().mkpath(QFileInfo(to).absolutePath()))
        return tr("The folder for %1 could not be made.").arg(to);
    QFile out(to);
    if (!out.open(QIODevice::NewOnly))
        return tr("%1 could not be written.").arg(to);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(1 << 20, Qt::Uninitialized);
    while (true) {
        const qint64 got = in.read(buffer.data(), buffer.size());
        if (got < 0)
            return tr("%1 could not be read.").arg(relative);
        if (got == 0)
            break;
        bytes += got;
        if (bytes > mostBytes)
            return tr("This project is more than %1 MB, too large for one game.").arg(mostBytes / (1024 * 1024));
        hash.addData(QByteArrayView(buffer.constData(), got));
        if (out.write(buffer.constData(), got) != got)
            return tr("%1 could not be written: %2").arg(to, out.errorString());
    }
    if (!out.flush())
        return tr("%1 could not be written: %2").arg(to, out.errorString());
    sha256 = hash.result().toHex();
    return {};
}

// ── what a game's files name ──

/// What a game holds, as its files name it.
struct Held {
    QSet<QString> files;
    QSet<QString> folders;
    /// The folders the game's scenes are in, "scenes" first if it holds one, and the folders its .obj models are in, each at
    /// most mostSceneFoldersTried, the first in order.
    QStringList sceneFolders;
    QStringList objFolders;

    static QString key(const QString &path)
    {
#ifdef Q_OS_WIN
        return path.toLower();
#else
        return path;
#endif
    }
    explicit Held(const QStringList &all)
    {
        QSet<QString> scenes;
        QSet<QString> objs;
        for (const QString &file : all) {
            files.insert(key(file));
            for (qsizetype slash = file.lastIndexOf(QLatin1Char('/')); slash > 0; slash = file.lastIndexOf(QLatin1Char('/'), slash - 1))
                folders.insert(key(file.left(slash)));
            const QString folder = file.left(qMax(qsizetype(0), file.lastIndexOf(QLatin1Char('/'))));
            if (file.startsWith(QStringLiteral("scenes/")) && file.endsWith(QStringLiteral(".qml"), Qt::CaseInsensitive))
                scenes.insert(folder);
            if (file.endsWith(QStringLiteral(".obj"), Qt::CaseInsensitive))
                objs.insert(folder);
        }
        const auto firstOf = [](const QSet<QString> &set) {
            QStringList list(set.cbegin(), set.cend());
            list.sort();  // "scenes" sorts before any folder in it
            if (list.size() > mostSceneFoldersTried)
                list.resize(mostSceneFoldersTried);
            return list;
        };
        sceneFolders = firstOf(scenes);
        objFolders = firstOf(objs);
    }
};

/// Where `named`, read from the folder `from` of the game, leads inside the game ("" for the game's own folder), or nothing if
/// it climbs out.
std::optional<QString> inGame(const QString &from, const QString &named)
{
    QStringList at = from.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList parts = named.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QStringLiteral("."))
            continue;
        if (part == QStringLiteral("..")) {
            if (at.isEmpty())
                return std::nullopt;
            at.removeLast();
            continue;
        }
        at << part;
    }
    return at.join(QLatin1Char('/'));
}

int hexDigit(char c)
{
    return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
}

/// The value of the string opening at `at`, which ends past it, with its escapes read as JavaScript and JSON read them: \xHH,
/// \uHHHH, \u{H...}, a legacy octal \NNN, and a backslash before a line's end, which joins two lines. At most `most` bytes of
/// it are kept; `cut`, if given, says whether there was more. A string in quotes ends at its line's end, so a quote in
/// something this does not read as code (a pattern, say) costs one line and no more. In back quotes, what comes before the
/// first ${, which is what can be known of it.
QByteArray readString(QByteArrayView text, qsizetype &at, qsizetype most, bool *cut = nullptr)
{
    const char quote = text[at++];
    QByteArray value;
    bool keeping = true;
    if (cut)
        *cut = false;
    const auto keep = [&](char c) {
        if (!keeping)
            return;
        if (value.size() < most)
            value += c;
        else if (cut)
            *cut = true;
    };
    // Only what could be part of a path that climbs matters: anything else stands as something that is not one of those.
    const auto keepCode = [&](qint64 code) { keep(code > 0 && code < 0x80 ? char(code) : '?'); };
    const auto hex = [&](qsizetype digits) {
        qint64 code = 0;
        qsizetype read = 0;
        for (; read < digits && at + read < text.size(); ++read) {
            const int digit = hexDigit(text[at + read]);
            if (digit < 0)
                break;
            code = code * 16 + digit;
        }
        at += read;
        keepCode(read == digits ? code : 0);
    };
    while (at < text.size()) {
        const char c = text[at++];
        if (c == quote || (c == '\n' && quote != '`'))
            return value;
        if (c == '$' && quote == '`' && at < text.size() && text[at] == '{')
            keeping = false;
        if (c != '\\') {
            keep(c);
            continue;
        }
        if (at >= text.size())
            return value;
        const char escaped = text[at++];
        if (escaped == 'x') {
            hex(2);
        } else if (escaped == 'u' && at < text.size() && text[at] == '{') {
            // ES2015's code point, \u{2e}: as many digits as are given, up to the last code point there is.
            qint64 code = 0;
            qsizetype end = at + 1;
            while (end < text.size() && hexDigit(text[end]) >= 0 && code <= 0x10FFFF)
                code = code * 16 + hexDigit(text[end++]);
            const bool whole = end < text.size() && text[end] == '}' && end > at + 1 && code <= 0x10FFFF;
            at = whole ? end + 1 : end;
            keepCode(whole ? code : 0);
        } else if (escaped == 'u') {
            hex(4);
        } else if (escaped >= '0' && escaped <= '7') {
            // \56 is a dot to JavaScript that is not strict: at most three digits, and no more than \377.
            int code = escaped - '0';
            const int digits = escaped <= '3' ? 3 : 2;
            for (int read = 1; read < digits && at < text.size() && text[at] >= '0' && text[at] <= '7'; ++read)
                code = code * 8 + (text[at++] - '0');
            keepCode(code);
        } else if (escaped == '\r') {
            if (at < text.size() && text[at] == '\n')
                ++at;
        } else if (escaped == '\xE2' && at + 1 < text.size() && text[at] == '\x80' && (text[at + 1] == '\xA8' || text[at + 1] == '\xA9')) {
            at += 2;  // a line or paragraph separator after a backslash joins lines too
        } else if (escaped != '\n') {
            keep(escaped);
        }
    }
    return value;
}

QString saysItIsOnlyWords(const QString &reason, bool words)
{
    return words ? reason + QLatin1Char(' ')
                       + tr("If that is only words and not a file, write them another way: a build cannot tell them apart.")
                 : reason;
}

QString leadsOut(const QString &file, const QString &named, bool words)
{
    return saysItIsOnlyWords(tr("%1 names %2, which leads out of the game. A game carries its own files: copy it into the project's "
                                "assets, name it there, then build.").arg(file, named), words);
}

QString notHeld(const QString &file, const QString &named, bool words)
{
    return saysItIsOnlyWords(tr("%1 names %2, which this game would not hold. A game holds the project's scenes, logic, prefabs and "
                                "assets, and nothing named with a dot: move it there, then build.").arg(file, named), words);
}

/// A string in code that is a full path: not "/" or "/5" or "and/or", which are words, nor anything with a space in it.
bool isFullPath(const QString &text)
{
    if (text.contains(QLatin1Char(' ')))
        return false;
    if (text.size() > 3 && text.at(0).isLetter() && text.at(1) == QLatin1Char(':') && text.at(2) == QLatin1Char('/'))
        return true;
    if (text.size() < 2 || text.at(0) != QLatin1Char('/'))
        return false;
    if (text.startsWith(QStringLiteral("//")))
        return text.size() > 2;
    const qsizetype slash = text.indexOf(QLatin1Char('/'), 1);
    return (slash > 0 && slash + 1 < text.size()) || text.indexOf(QLatin1Char('.')) > 0;
}

bool climbs(const QString &text)
{
    return text.startsWith(QStringLiteral("../")) || text.startsWith(QStringLiteral("./../")) || text.contains(QStringLiteral("/../"))
           || text.endsWith(QStringLiteral("/.."));
}

/// Where `named` leads, read from `from`: into what the game holds, into the game but to nothing it holds, or out of it.
enum class Leads { Held, NotHeld, Out };

Leads whereItLeads(const QString &from, const QString &named, const Held &held)
{
    const std::optional<QString> place = inGame(from, named);
    if (!place)
        return Leads::Out;
    const QString key = Held::key(*place);
    return key.isEmpty() || held.files.contains(key) || held.folders.contains(key) ? Leads::Held : Leads::NotHeld;
}

/// What is wrong with a string in code, as read: "" for a string that names nothing it should not. An import is a path for
/// certain, read from the file's own folder, as QML reads it. Anything else only if it looks like one that leads somewhere it
/// should not, from the file's folder and from every folder a scene is in, since that is where play() and spawn() read it.
QString problemWithString(const QString &file, const QString &from, const QByteArray &string, bool isImport, const Held &held)
{
    if (string.isEmpty())
        return {};
    const QString shown = QString::fromUtf8(string);
    QString named = string.contains('%') ? QUrl::fromPercentEncoding(string) : shown;
    named.replace(QLatin1Char('\\'), QLatin1Char('/'));

    const bool words = !isImport;
    if (named.size() > 5 && named.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive) && !named.contains(QLatin1Char(' ')))
        return leadsOut(file, shown, words);
    if (isFullPath(named))
        return leadsOut(file, shown, words);
    if (isImport) {
        if (named.startsWith(QStringLiteral("qrc:")))
            return {};
        const qsizetype colon = named.indexOf(QLatin1Char(':'));
        if (colon > 0 && colon < named.indexOf(QLatin1Char('/')))
            return leadsOut(file, shown, words);
    } else if (!climbs(named)) {
        return {};
    }
    Leads leads = whereItLeads(from, named, held);
    for (qsizetype i = 0; !isImport && leads != Leads::Held && i < held.sceneFolders.size(); ++i)
        leads = std::min(leads, whereItLeads(held.sceneFolders.at(i), named, held));
    switch (leads) {
    case Leads::Held: return {};
    case Leads::NotHeld: return notHeld(file, shown, words);
    case Leads::Out: break;
    }
    return leadsOut(file, shown, words);
}

bool isWordCharacter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
}

/// Every string in a .qml, .mjs or .js, comments passed over, in one pass that goes back over nothing. It knows an import by the
/// word before the string, as `import "x"`, `.import "x"`, `from "x"` and `import("x")` have it.
QString problemInCode(const QString &file, const QString &from, QByteArrayView text, const Held &held)
{
    QByteArrayView word;
    qsizetype at = 0;
    while (at < text.size()) {
        const char c = text[at];
        if (c == '/' && at + 1 < text.size() && text[at + 1] == '/') {
            at = text.indexOf('\n', at);
            if (at < 0)
                return {};
        } else if (c == '/' && at + 1 < text.size() && text[at + 1] == '*') {
            at = text.indexOf(QByteArrayView("*/"), at + 2);
            if (at < 0)
                return {};
            at += 2;
        } else if (c == '"' || c == '\'' || c == '`') {
            const bool isImport = word == QByteArrayView("import") || word == QByteArrayView("from");
            // Whole: a file is at most largestCodeLookedThrough, so nothing is cut short to be looked at.
            const QByteArray string = readString(text, at, text.size());
            word = {};
            if (const QString problem = problemWithString(file, from, string, isImport, held); !problem.isEmpty())
                return problem;
        } else if (isWordCharacter(c)) {
            const qsizetype start = at;
            while (at < text.size() && isWordCharacter(text[at]))
                ++at;
            word = text.sliced(start, at - start);
        } else {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && !(c == '(' && word == QByteArrayView("import")))
                word = {};
            ++at;
        }
    }
    return {};
}

/// What is wrong with a file a model or a map names: "" for one in the game, or held in the model itself. It is read from each
/// of `bases` in turn, and is in the game if any of them leads to it.
QString problemWithUri(const QString &file, const QStringList &bases, const QByteArray &uri, const Held &held)
{
    const QString shown = QString::fromUtf8(uri);
    if (shown.isEmpty() || shown.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive))
        return {};
    QString named = QUrl::fromPercentEncoding(uri);
    named.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const qsizetype colon = named.indexOf(QLatin1Char(':'));
    const qsizetype slash = named.indexOf(QLatin1Char('/'));
    if (named.startsWith(QLatin1Char('/')) || (colon >= 0 && (slash < 0 || colon < slash)))
        return leadsOut(file, shown, false);
    Leads leads = Leads::Out;
    for (const QString &base : bases) {
        const std::optional<QString> place = inGame(base, named);
        leads = std::min(leads, !place ? Leads::Out : held.files.contains(Held::key(*place)) ? Leads::Held : Leads::NotHeld);
        if (leads == Leads::Held)
            return {};
    }
    return leads == Leads::NotHeld ? notHeld(file, shown, false) : leadsOut(file, shown, false);
}

QString problemWithUri(const QString &file, const QString &from, const QByteArray &uri, const Held &held)
{
    return problemWithUri(file, QStringList{from}, uri, held);
}

/// Every string under one of `keys` in JSON, in one pass, however deep and however many: not parsed, so two of one name are
/// both looked at, whichever of them another reader would believe. A name longer than any path is refused, unless it is a
/// data: URI, which names no file. Where `onlyText`, a name that is not a string is refused too.
QString problemInJson(const QString &file, const QString &from, QByteArrayView json, const Held &held,
                      std::initializer_list<QByteArrayView> keys, bool onlyText)
{
    // Longer than any of the keys looked for, so one cut to this is none of them.
    constexpr qsizetype longestKeyKept = 8;
    const auto skipSpace = [&json](qsizetype at) {
        while (at < json.size() && (json[at] == ' ' || json[at] == '\t' || json[at] == '\n' || json[at] == '\r'))
            ++at;
        return at;
    };
    qsizetype at = 0;
    while ((at = json.indexOf('"', at)) >= 0) {
        const QByteArray key = readString(json, at, longestKeyKept);
        qsizetype next = skipSpace(at);
        if (next >= json.size() || json[next] != ':')
            continue;
        at = next + 1;
        if (std::find(keys.begin(), keys.end(), QByteArrayView(key)) == keys.end())
            continue;
        next = skipSpace(at);
        if (next >= json.size())
            return {};
        if (json[next] != '"') {
            if (onlyText)
                return tr("%1 names a file by something other than text, which a game cannot be sure of.").arg(file);
            continue;
        }
        at = next;
        bool cut = false;
        const QByteArray name = readString(json, at, longestNameKept, &cut);
        if (cut && name.left(5).toLower() != "data:")
            return tr("%1 names a file by a name longer than %2 characters, longer than any path, so a build cannot tell where "
                      "it leads.").arg(file).arg(longestNameKept);
        if (const QString problem = problemWithUri(file, from, name, held); !problem.isEmpty())
            return problem;
    }
    return {};
}

/// A .glb's JSON is its first chunk.
QString problemInGlb(const QString &file, const QString &from, QByteArrayView data, const Held &held)
{
    constexpr qsizetype header = 12;
    constexpr qsizetype chunkHeader = 8;
    if (data.size() < header + chunkHeader || !data.startsWith(QByteArrayView("glTF")))
        return {};
    if (data.sliced(header + 4, 4) != QByteArrayView("JSON"))
        return {};
    const quint32 chunk = qFromLittleEndian<quint32>(data.data() + header);
    if (chunk > quint64(data.size() - header - chunkHeader))
        return tr("%1 says it holds more than it does, so it is not a whole model.").arg(file);
    return problemInJson(file, from, data.sliced(header + chunkHeader, chunk), held, {QByteArrayView("uri")}, true);
}

/// A Tiled tileset kept as XML (.tsx), which TileMap reads too: the source of each element, its image among them.
QString problemInTsx(const QString &file, const QString &from, QByteArrayView data, const Held &held)
{
    QByteArray raw = QByteArray::fromRawData(data.data(), data.size());
    QBuffer buffer(&raw);
    if (!buffer.open(QIODevice::ReadOnly))
        return tr("%1 could not be looked through.").arg(file);
    QXmlStreamReader xml(&buffer);
    while (!xml.atEnd()) {
        if (!xml.readNextStartElement())
            continue;
        for (const QXmlStreamAttribute &attribute : xml.attributes()) {
            if (attribute.name() != QStringLiteral("source"))
                continue;
            if (const QString problem = problemWithUri(file, from, attribute.value().toUtf8(), held); !problem.isEmpty())
                return problem;
        }
    }
    // What is not XML, TileMap does not read, so it names nothing.
    return {};
}

/// The lines of a model kept as text, each without what ends it.
template<typename Each>
QString eachLine(QByteArrayView text, Each each)
{
    qsizetype at = 0;
    while (at < text.size()) {
        qsizetype end = text.indexOf('\n', at);
        if (end < 0)
            end = text.size();
        if (const QString problem = each(text.sliced(at, end - at)); !problem.isEmpty())
            return problem;
        at = end + 1;
    }
    return {};
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v' || c == '\0';
}

QByteArrayView trimmed(QByteArrayView text)
{
    qsizetype start = 0;
    qsizetype end = text.size();
    while (start < end && isSpace(text[start]))
        ++start;
    while (end > start && isSpace(text[end - 1]))
        --end;
    return text.sliced(start, end - start);
}

/// A line's first word, and where what follows it starts.
QByteArrayView firstWord(QByteArrayView line, qsizetype &after)
{
    qsizetype start = 0;
    while (start < line.size() && isSpace(line[start]))
        ++start;
    after = start;
    while (after < line.size() && !isSpace(line[after]))
        ++after;
    return line.sliced(start, after - start);
}

/// An .obj's material libraries, each `mtllib` line naming one, to the line's end, as Assimp reads it, from the model's folder.
QString problemInObj(const QString &file, const QString &from, QByteArrayView text, const Held &held)
{
    return eachLine(text, [&](QByteArrayView line) -> QString {
        qsizetype after = 0;
        if (firstWord(line, after).compare(QByteArrayView("mtllib"), Qt::CaseInsensitive) != 0)
            return {};
        const QByteArrayView name = trimmed(line.sliced(after));
        if (name.size() > longestNameKept)
            return tr("%1 names a file by a name longer than %2 characters, longer than any path, so a build cannot tell where it "
                      "leads.").arg(file).arg(longestNameKept);
        return name.isEmpty() ? QString() : problemWithUri(file, from, name.toByteArray(), held);
    });
}

/// How many words follow an option of an .mtl's picture, as Assimp counts them: it knows an option by how it starts, in this
/// order, and one it does not know takes none.
int optionWords(QByteArrayView option)
{
    const auto is = [option](const char *name) {
        const QByteArrayView known(name);
        return option.size() >= known.size() && option.first(known.size()).compare(known, Qt::CaseInsensitive) == 0;
    };
    for (const char *one : {"-clamp", "-type", "-blendu", "-blendv", "-boost", "-texres", "-bm", "-imfchan"}) {
        if (is(one))
            return 1;
    }
    if (is("-mm"))
        return 2;
    if (is("-o") || is("-s") || is("-t"))
        return 3;
    return 0;
}

/// The pictures an .mtl names: a line of a map_ kind, or bump, disp, decal, refl or norm, gives its options, then the file,
/// to the line's end, as Assimp reads it. Qt Quick 3D reads a model's
/// pictures from the model's folder, which may not be the library's, so a picture is looked for from both: the library's own
/// folder, and each folder an .obj of the game is in.
QString problemInMtl(const QString &file, const QString &from, QByteArrayView text, const Held &held)
{
    QStringList bases{from};
    bases += held.objFolders;
    return eachLine(text, [&](QByteArrayView line) -> QString {
        qsizetype after = 0;
        const QByteArrayView kind = firstWord(line, after);
        const bool picture = kind.size() > 4 && kind.first(4).compare(QByteArrayView("map_"), Qt::CaseInsensitive) == 0;
        if (!picture) {
            bool other = false;
            for (const char *word : {"bump", "disp", "decal", "refl", "norm"})
                other = other || kind.compare(QByteArrayView(word), Qt::CaseInsensitive) == 0;
            if (!other)
                return {};
        }
        QByteArrayView rest = line.sliced(after);
        for (;;) {
            qsizetype next = 0;
            const QByteArrayView option = firstWord(rest, next);
            if (!option.startsWith('-'))
                break;
            const int words = optionWords(option);
            rest = rest.sliced(next);
            for (int i = 0; i < words; ++i) {
                firstWord(rest, next);
                rest = rest.sliced(next);
            }
        }
        const QByteArrayView name = trimmed(rest);
        if (name.size() > longestNameKept)
            return tr("%1 names a file by a name longer than %2 characters, longer than any path, so a build cannot tell where it "
                      "leads.").arg(file).arg(longestNameKept);
        return name.isEmpty() ? QString() : problemWithUri(file, bases, name.toByteArray(), held);
    });
}

enum class Names { Nothing, Code, Gltf, Glb, Map, Tsx, Obj, Mtl };

Names whatItNames(const QString &file)
{
    const QString suffix = QFileInfo(file).suffix().toLower();
    if (suffix == QStringLiteral("qml") || suffix == QStringLiteral("mjs") || suffix == QStringLiteral("js"))
        return Names::Code;
    if (suffix == QStringLiteral("gltf"))
        return Names::Gltf;
    if (suffix == QStringLiteral("glb"))
        return Names::Glb;
    if (suffix == QStringLiteral("tmj") || suffix == QStringLiteral("tsj"))
        return Names::Map;
    if (suffix == QStringLiteral("tsx"))
        return Names::Tsx;
    if (suffix == QStringLiteral("obj"))
        return Names::Obj;
    if (suffix == QStringLiteral("mtl"))
        return Names::Mtl;
    return Names::Nothing;
}

/// Look through the copy of `file` for what it names: "" or the reason it cannot go, which says where and what.
QString problemInCopy(const QString &copy, const QString &file, const Held &held)
{
    const Names kind = whatItNames(file);
    QFile in(copy);
    if (!in.open(QIODevice::ReadOnly))
        return tr("%1 could not be looked through.").arg(file);
    const qint64 size = in.size();
    // Code, a tileset and a material library are small; a model or a map may not be.
    const bool small = kind == Names::Code || kind == Names::Tsx || kind == Names::Mtl;
    const qint64 most = small ? largestCodeLookedThrough : largestModelLookedThrough;
    if (size > most)
        return tr("%1 is more than %2 MB, too large for a build to look through for what it names.").arg(file).arg(most / (1024 * 1024));
    if (size == 0)
        return {};
    const uchar *mapped = in.map(0, size);
    if (!mapped)
        return tr("%1 could not be looked through.").arg(file);
    const QByteArrayView data(reinterpret_cast<const char *>(mapped), size);
    const qsizetype slash = file.lastIndexOf(QLatin1Char('/'));
    const QString from = slash < 0 ? QString() : file.left(slash);
    switch (kind) {
    case Names::Code: return problemInCode(file, from, data, held);
    case Names::Gltf: return problemInJson(file, from, data, held, {QByteArrayView("uri")}, true);
    case Names::Glb: return problemInGlb(file, from, data, held);
    // A Tiled map names its tilesets by source and its pictures by image, each from its own folder (TileMap).
    case Names::Map: return problemInJson(file, from, data, held, {QByteArrayView("image"), QByteArrayView("source")}, false);
    case Names::Tsx: return problemInTsx(file, from, data, held);
    case Names::Obj: return problemInObj(file, from, data, held);
    case Names::Mtl: return problemInMtl(file, from, data, held);
    case Names::Nothing: break;
    }
    return {};
}

/// "orb-run" -> "Orb Run", as the Library names a folder.
QString titleOf(const QString &folder)
{
    QStringList words = QString(folder).replace(QLatin1Char('_'), QLatin1Char(' ')).replace(QLatin1Char('-'), QLatin1Char(' '))
                            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words)
        word[0] = word[0].toUpper();
    // Cut, then trimmed: a cut between two words would leave a space the Library takes off, and the two would differ.
    const QString title = words.join(QLatin1Char(' ')).simplified().left(longestTitle).trimmed();
    return title.isEmpty() ? tr("Untitled") : title;
}

QString platformName()
{
    const QString kernel = QSysInfo::kernelType();
    const QString system = kernel == QStringLiteral("winnt") ? QStringLiteral("windows") : kernel;
    return system + QLatin1Char('-') + QSysInfo::currentCpuArchitecture();
}

/// How many times the game there has been built. Its folder is only this person's, but it is still only read for a number,
/// never through a link, and never from a pipe.
int builtBefore(const QString &game)
{
    QFile file;
    if (!openPlain(file, game, QLatin1String(manifestName)))
        return 0;
    const qint64 build = QJsonDocument::fromJson(file.read(largestManifestRead)).object().value(QStringLiteral("build")).toInteger();
    return build > 0 && build < std::numeric_limits<int>::max() ? int(build) : 0;
}

/// What ends the name of a game's leftovers: QTemporaryDir's six characters, and no more. A game's folder name ends in a hash
/// of 16 characters, so another game's leftovers, whatever its name holds, never end so after this game's name.
const QLatin1StringView leftoverMark("??????");

/// A build that died between its two renames left the game aside, under a name the Library does not list: it goes back, the
/// newest if there is more than one, before anything else can happen to it. A link there is nobody's game.
void putBackWhatDied(const QDir &games, const QString &folder, const QString &target)
{
    const QFileInfo there(target);
    if (there.exists() || there.isSymLink())
        return;
    const QFileInfoList aside = games.entryInfoList({QStringLiteral(".old-%1-%2").arg(folder, leftoverMark)}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo &entry : aside) {
        if (!entry.isSymLink() && !entry.isJunction() && QDir().rename(entry.filePath(), target))
            return;
    }
}

/// What a build that stopped halfway, or a game that was in use when it was replaced, left behind for this game. A link is
/// taken away itself, and what it leads to is left alone.
void clearLeftovers(const QDir &games, const QString &folder)
{
    const QFileInfoList left = games.entryInfoList({QStringLiteral(".building-%1-%2").arg(folder, leftoverMark), QStringLiteral(".old-%1-%2").arg(folder, leftoverMark)},
                                                   QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : left) {
        if (entry.isSymLink() || entry.isJunction() || !entry.isDir())
            QFile::remove(entry.filePath());
        else
            QDir(entry.filePath()).removeRecursively();
    }
}

}  // namespace

GameBuild::GameBuild(QObject *parent) : QObject(parent) {}

GameBuild::~GameBuild()
{
    if (m_thread)
        m_thread->wait();
}

bool GameBuild::start(const QUrl &project, const QUrl &scene)
{
    if (m_thread)
        return false;
    const QString folder = project.toLocalFile();
    const QString file = scene.toLocalFile();
    const QString games = gamesFolder();
    const Limits limits = m_limits;
    QThread *thread = QThread::create([this, folder, file, games, limits] { m_result = make(folder, file, games, limits); });
    thread->setParent(this);
    connect(thread, &QThread::finished, this, [this, thread] {
        m_thread = nullptr;
        thread->deleteLater();
        emit busyChanged();
        emit finished(m_result);
    });
    m_thread = thread;
    thread->start();
    emit busyChanged();
    return true;
}

QString GameBuild::gamesFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("qor-os/games"));
}

QString GameBuild::folderNameFor(const QString &realProject)
{
    return GameStore::gameOf(QUrl::fromLocalFile(realProject));
}

QVariantMap GameBuild::make(const QString &project, const QString &scene, const QString &games, const Limits &limits)
{
    const QString root = project.isEmpty() ? QString() : Confinement::realPath(project);
    if (root.isEmpty() || !QFileInfo(root).isDir())
        return refused(tr("There is no project folder at %1.").arg(project));
    // Asked of the folder this build goes on to copy, and of no other: what is asked about is what is built.
    if (limits.trusted && !limits.trusted(root))
        return refused(tr("%1 is not a project you have said you trust. Open it in QQ Studio and trust it there, then build.").arg(project));

    const QString gamesPath = QDir::cleanPath(games);
    if (!QDir().mkpath(gamesPath))
        return refused(tr("The folder for games, %1, could not be made.").arg(gamesPath));
    if (Confinement::within(Confinement::realPath(gamesPath), root))
        return refused(tr("This project holds the folder games are built into, %1.").arg(gamesPath));

    // One build of a game at a time, for the whole of it: another would take this one's staging folder away, and answer for
    // what was left. On a local disk Qt's lock is the system's own (flock, or a file Windows will not delete while it is open),
    // so it is kept while its holder lives, however old, and taken from a holder that has gone: that is what the tests prove.
    // setStaleLockTime(0) is only for where the system cannot lock (FUSE, some network shares; Qt's QLockFile documentation):
    // there Qt would take a lock older than 30 seconds from a build still running, and a large game takes minutes. With 0 it
    // never goes by age, so a build that died on another machine sharing the folder leaves its lock until it is deleted. No
    // test here has such a filesystem, so that line is unproven.
    const QString name = folderNameFor(root);
    const QDir gamesDir(gamesPath);
    QLockFile lock(gamesDir.filePath(QStringLiteral(".lock-%1").arg(name)));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        return refused(lock.error() == QLockFile::LockFailedError
                           ? tr("This game is already being built, here or in another Studio. When that has finished, build again.")
                           : tr("%1 could not be written to, so this game could not be built.").arg(gamesPath));
    }
    const QString target = gamesDir.filePath(name);
    putBackWhatDied(gamesDir, name, target);
    clearLeftovers(gamesDir, name);

    const QFileInfo existing(target);
    if (existing.isSymLink() || existing.isJunction() || (existing.exists() && !existing.isDir()))
        return refused(tr("%1 is in the way of this game, and is not one.").arg(target));

    // Its first scene: a saved file directly in scenes/, since that is what makes the copy's folder its package (Confinement).
    const QString relative = QDir::isAbsolutePath(scene) ? QDir(root).relativeFilePath(Confinement::realPath(scene)) : scene;
    if (ProjectWorkspace::fileInside(root, relative).isEmpty()
        || !relative.startsWith(QStringLiteral("scenes/")) || relative.count(QLatin1Char('/')) != 1
        || !relative.endsWith(QStringLiteral(".qml")))
        return refused(tr("A game starts in a saved scene in this project's scenes folder, and %1 is not one.").arg(scene));
    if (QFileInfo(relative).fileName().startsWith(QLatin1Char('.')))
        return refused(tr("A game does not carry a file named with a dot, so it cannot start in %1: rename it, then build.").arg(relative));

    Found found;
    for (const QString &folder : {QStringLiteral("scenes"), QStringLiteral("logic"), QStringLiteral("prefabs"), QStringLiteral("assets")}) {
        const QFileInfo top(QDir(root).filePath(folder));
        if (top.isSymLink() || top.isJunction())
            return refused(tr("%1 is a link. A game carries its own files: copy them there, then build.").arg(folder));
        if (top.exists() && !top.isDir())
            return refused(tr("%1 is not a folder.").arg(folder));
        if (top.isDir())
            walk(root, folder, limits.files, found);
        if (!found.problem.isEmpty())
            return refused(found.problem);
    }
    if (limits.step)
        limits.step("walked");

    QTemporaryDir staging(gamesDir.filePath(QStringLiteral(".building-%1-XXXXXX").arg(name)));
    if (!staging.isValid())
        return refused(tr("A game could not be built in %1: %2").arg(gamesPath, staging.errorString()));

    // What names other files first, and looked through as it comes: a build that is refused for that is told before it has
    // copied gigabytes of pictures. It is the copy that is looked through, so what is looked at is what is carried.
    QStringList ordered = found.files;
    std::stable_partition(ordered.begin(), ordered.end(), [](const QString &file) { return whatItNames(file) != Names::Nothing; });
    const Held held(found.files);
    QJsonObject files;
    qint64 bytes = 0;
    for (const QString &file : std::as_const(ordered)) {
        QByteArray sha256;
        const QString copy = staging.filePath(file);
        const QString wrong = copyCounted(root, file, copy, limits.bytes, bytes, sha256);
        if (!wrong.isEmpty())
            return refused(wrong);
        files.insert(file, QString::fromLatin1(sha256));
        if (whatItNames(file) != Names::Nothing) {
            const QString named = problemInCopy(copy, file, held);
            if (!named.isEmpty())
                return refused(named);
        }
    }

    const int build = existing.exists() ? builtBefore(target) + 1 : 1;
    const QString title = titleOf(QFileInfo(root).fileName());

    const QJsonObject manifest{
        {QStringLiteral("format"), manifestFormat},
        {QStringLiteral("kind"), QStringLiteral("game")},
        {QStringLiteral("title"), title},
        {QStringLiteral("scene"), relative},
        {QStringLiteral("build"), build},
        {QStringLiteral("built"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("engine"), QJsonObject{{QStringLiteral("id"), QStringLiteral("qq")},
                                               {QStringLiteral("version"), QStringLiteral(QQ_ENGINE_VERSION)}}},
        {QStringLiteral("platform"), platformName()},
        {QStringLiteral("files"), files},
    };
    QSaveFile written(staging.filePath(QLatin1String(manifestName)));
    if (!written.open(QIODevice::WriteOnly) || written.write(QJsonDocument(manifest).toJson()) < 0 || !written.commit())
        return refused(tr("What this game holds could not be written: %1").arg(written.errorString()));
    if (limits.step)
        limits.step("copied");

    // Into place whole. A game being played from here keeps what it has loaded. What it loads later, by its path (starting over,
    // a prefab spawned, a sound, logic reloaded) is out of its reach on Linux, where its wall holds the old folder, and comes
    // from the new build elsewhere.
    QString old;
    if (existing.exists()) {
        old = gamesDir.filePath(QStringLiteral(".old-%1-%2").arg(name, QFileInfo(staging.path()).fileName().right(6)));
        if (!QDir().rename(target, old))
            return refused(tr("%1 could not be replaced. If it is playing, close it, then build.").arg(title));
        if (limits.step)
            limits.step("moved-aside");
    }
    if (!QDir().rename(staging.path(), target)) {
        if (!old.isEmpty())
            QDir().rename(old, target);
        return refused(tr("%1 could not be put in place in %2.").arg(title, gamesPath));
    }
    staging.setAutoRemove(false);
    if (limits.step)
        limits.step("in-place");
    if (!old.isEmpty())
        QDir(old).removeRecursively();

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("problem"), QString()},
            {QStringLiteral("title"), title},
            {QStringLiteral("folder"), target},
            {QStringLiteral("scene"), QDir(target).filePath(relative)},
            {QStringLiteral("build"), build},
            {QStringLiteral("files"), int(found.files.size())},
            {QStringLiteral("bytes"), bytes},
            {QStringLiteral("leftOut"), found.leftOut},
            {QStringLiteral("leftOutCount"), found.leftOutCount}};
}

}  // namespace qq
