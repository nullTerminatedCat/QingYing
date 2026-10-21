#include "Secret.h"

#include <QByteArray>
#include <windows.h>
#include <dpapi.h>

namespace Secret {
QString protect(const QString& plain)
{
    if (plain.isEmpty()) return {};
    QByteArray in = plain.toUtf8();
    DATA_BLOB src{DWORD(in.size()), reinterpret_cast<BYTE*>(in.data())};
    DATA_BLOB out{};
    if (!CryptProtectData(&src, L"QingYing", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    const QByteArray enc(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    SecureZeroMemory(in.data(), in.size());
    return QString::fromLatin1(enc.toBase64());
}

QString unprotect(const QString& base64)
{
    if (base64.isEmpty()) return {};
    QByteArray enc = QByteArray::fromBase64(base64.toLatin1());
    DATA_BLOB src{DWORD(enc.size()), reinterpret_cast<BYTE*>(enc.data())};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&src, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    const QString plain = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return plain;
}
}
