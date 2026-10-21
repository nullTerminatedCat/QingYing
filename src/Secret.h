#pragma once
#include <QString>

// Windows DPAPI（CryptProtectData，当前用户范围）加密，返回 Base64；绝不明文存储
namespace Secret {
QString protect(const QString& plain);
QString unprotect(const QString& base64);
}
