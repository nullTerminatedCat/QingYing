#include "Logic.h"
#include "ScrollStitcher.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QtMath>

namespace Logic {

int findShift(const QImage& a, const QImage& b)
{
    return ScrollStitcher::match(a, b).shift;
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
    auto hex2 = [](int v) { return QStringLiteral("%1").arg(v, 2, 16, QLatin1Char('0')); };
    if (f == QLatin1String("hexlower")) return c.name().toLower();
    if (f == QLatin1String("hexa"))
        return (QStringLiteral("#") + hex2(c.red()) + hex2(c.green()) + hex2(c.blue()) + hex2(c.alpha())).toUpper();
    if (f == QLatin1String("rgb"))
        return QStringLiteral("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    if (f == QLatin1String("rgba")) {
        const qreal a = c.alphaF();
        const int prec = (a == 0.0 || a == 1.0) ? 0 : 2;
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(a, 0, 'f', prec);
    }
    if (f == QLatin1String("rgbplain"))
        return QStringLiteral("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue());
    if (f == QLatin1String("hsl")) {
        int h = 0, s = 0, l = 0, a = 0;
        c.getHsl(&h, &s, &l, &a);
        if (h < 0) h = 0;
        return QStringLiteral("hsl(%1, %2%, %3%)")
            .arg(h).arg(qRound(s * 100.0 / 255.0)).arg(qRound(l * 100.0 / 255.0));
    }
    if (f == QLatin1String("hsv")) {
        int h = 0, s = 0, v = 0, a = 0;
        c.getHsv(&h, &s, &v, &a);
        if (h < 0) h = 0;
        return QStringLiteral("hsv(%1, %2%, %3%)")
            .arg(h).arg(qRound(s * 100.0 / 255.0)).arg(qRound(v * 100.0 / 255.0));
    }
    if (f == QLatin1String("bgr"))
        return QStringLiteral("0x") + (hex2(c.blue()) + hex2(c.green()) + hex2(c.red())).toUpper();
    return c.name().toUpper();
}

QStringList colorFormatIds()
{
    return {
        QStringLiteral("hex"),
        QStringLiteral("hexlower"),
        QStringLiteral("hexa"),
        QStringLiteral("rgb"),
        QStringLiteral("rgba"),
        QStringLiteral("rgbplain"),
        QStringLiteral("hsl"),
        QStringLiteral("hsv"),
        QStringLiteral("bgr"),
    };
}

QString colorFormatLabel(const QString& f)
{
    if (f == QLatin1String("hex")) return QStringLiteral("HEX");
    if (f == QLatin1String("hexlower")) return QStringLiteral("HEX");
    if (f == QLatin1String("hexa")) return QStringLiteral("HEXA");
    if (f == QLatin1String("rgb")) return QStringLiteral("RGB");
    if (f == QLatin1String("rgba")) return QStringLiteral("RGBA");
    if (f == QLatin1String("rgbplain")) return QStringLiteral("RGB");
    if (f == QLatin1String("hsl")) return QStringLiteral("HSL");
    if (f == QLatin1String("hsv")) return QStringLiteral("HSV");
    if (f == QLatin1String("bgr")) return QStringLiteral("BGR");
    return QStringLiteral("HEX");
}

QString nextColorFormat(const QString& current, bool reverse)
{
    const QStringList ids = colorFormatIds();
    int i = ids.indexOf(current);
    if (i < 0) i = 0;
    const int n = ids.size();
    i = reverse ? (i - 1 + n) % n : (i + 1) % n;
    return ids.at(i);
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


QString releaseTagFromUrl(const QUrl& url)
{
    static const QRegularExpression re(QStringLiteral(R"(/releases/tag/(v?[\w.-]+)/?$)"));
    const QRegularExpressionMatch m = re.match(url.path());
    return m.hasMatch() ? m.captured(1) : QString();
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
