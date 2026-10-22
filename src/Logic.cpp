#include "Logic.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QVersionNumber>

namespace Logic {

int findShift(const QImage& a, const QImage& b)
{
    if (a.size() != b.size() || a.height() < 20) return -1;
    const QImage prev = a.convertToFormat(QImage::Format_RGB32);
    const QImage cur = b.convertToFormat(QImage::Format_RGB32);
    const int w = prev.width(), h = prev.height();
    const int x0 = w > 120 ? 8 : 0;
    const int x1 = w > 120 ? w - 24 : w; // 忽略滚动条
    auto rowInfo = [&](const QImage& img, QList<quint64>& hashes, QList<bool>& uniform) {
        hashes.resize(h);
        uniform.resize(h);
        for (int y = 0; y < h; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            quint64 hsh = 1469598103934665603ULL;
            bool uni = true;
            for (int x = x0; x < x1; ++x) {
                hsh = (hsh ^ (row[x] & 0xFFFFFF)) * 1099511628211ULL;
                if (uni && row[x] != row[x0]) uni = false;
            }
            hashes[y] = hsh;
            uniform[y] = uni;
        }
    };
    QList<quint64> hp, hc;
    QList<bool> up, uc;
    rowInfo(prev, hp, up);
    rowInfo(cur, hc, uc);
    int best = -1;
    double bestRatio = 0;
    for (int dy = 0; dy < h - h / 5; ++dy) {
        int total = 0, match = 0;
        for (int y = 0; y + dy < h; ++y) {
            if (up[y + dy] && uc[y]) continue;
            ++total;
            if (hp[y + dy] == hc[y]) ++match;
        }
        if (total < 8) continue;
        const double ratio = double(match) / total;
        if (ratio > bestRatio + 1e-9) {
            bestRatio = ratio;
            best = dy;
        }
    }
    return bestRatio >= 0.85 ? best : -1;
}

QString expandPattern(const QString& pattern, const QDateTime& now)
{
    QString out = pattern;
    static const QRegularExpression re(QStringLiteral("\\{([^}]*)\\}"));
    auto it = re.globalMatch(pattern);
    QList<QPair<QString, QString>> reps;
    while (it.hasNext()) {
        const auto m = it.next();
        reps.append({m.captured(0), now.toString(m.captured(1))});
    }
    for (const auto& r : reps) out.replace(r.first, r.second);
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|]"));
    out.replace(bad, QStringLiteral("_"));
    out = out.trimmed();
    return out.isEmpty() ? now.toString(QStringLiteral("'QingYing_'yyyyMMdd_HHmmss")) : out;
}

QString formatColor(const QColor& c, const QString& f)
{
    if (f == QLatin1String("hexlower")) return c.name().toLower();
    if (f == QLatin1String("rgb")) return QStringLiteral("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    if (f == QLatin1String("rgbplain")) return QStringLiteral("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue());
    return c.name().toUpper();
}

bool isNewerVersion(const QString& remoteTag, const QString& current)
{
    auto split = [](QString s, QVersionNumber* v, QString* pre) {
        s = s.trimmed();
        if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V'))) s.remove(0, 1);
        const int plus = s.indexOf(QLatin1Char('+'));
        if (plus >= 0) s.truncate(plus); // 构建元数据不参与比较
        const int dash = s.indexOf(QLatin1Char('-'));
        *pre = dash >= 0 ? s.mid(dash + 1) : QString();
        qsizetype suffix = 0;
        *v = QVersionNumber::fromString(dash >= 0 ? s.left(dash) : s, &suffix).normalized();
    };
    QVersionNumber rv, cv;
    QString rp, cp;
    split(remoteTag, &rv, &rp);
    split(current, &cv, &cp);
    if (rv.isNull()) return false;
    const int c = QVersionNumber::compare(rv, cv);
    if (c != 0) return c > 0;
    if (rp.isEmpty() != cp.isEmpty()) return rp.isEmpty(); // 正式版 > 预发布
    return QString::compare(rp, cp) > 0;
}

QString expectedSha256(const QString& sums, const QString& fileName)
{
    for (const QString& line : sums.split(QLatin1Char('\n'))) {
        const QStringList parts = line.simplified().split(QLatin1Char(' '));
        if (parts.size() < 2) continue;
        QString name = parts.last();
        if (name.startsWith(QLatin1Char('*'))) name.remove(0, 1); // sha256sum 二进制模式
        if (name == fileName) {
            const QString h = parts.first().toLower();
            static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
            return hex.match(h).hasMatch() ? h : QString();
        }
    }
    return {};
}

QString sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString jsonValue(const QByteArray& json, const QString& path)
{
    QJsonValue v = QJsonDocument::fromJson(json).object();
    if (path.trimmed().isEmpty()) return {};
    for (const QString& key : path.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        bool isIndex = false;
        const int idx = key.toInt(&isIndex);
        if (isIndex && v.isArray()) v = v.toArray().at(idx);
        else if (v.isObject()) v = v.toObject().value(key);
        else return {};
    }
    return v.isString() ? v.toString() : QString();
}

bool mostlyCjk(const QString& text)
{
    int cjk = 0, latin = 0;
    for (QChar ch : text) {
        const uint u = ch.unicode();
        if ((u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFF00 && u <= 0xFFEF)
            || (u >= 0x3000 && u <= 0x303F))
            ++cjk;
        else if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
            ++latin;
    }
    if (cjk == 0 && latin == 0) return false;
    return cjk >= latin;
}
}
