#include "../src/Logic.h"
#include "../src/CursorBlend.h"
#include "../src/GifPalette.h"
#include <cmath>

#include <QTest>

class TestLogic : public QObject {
    Q_OBJECT
private:
    static QImage texture(int w, int h)
    {
        QImage img(w, h, QImage::Format_RGB32);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) img.setPixel(x, y, qRgb((x * 7 + y * 13) % 251, (y * y) % 253, (x ^ y) & 255));
        return img;
    }

private slots:
    void findShift_data()
    {
        QTest::addColumn<int>("shift");
        for (int s : {1, 5, 37, 120, 300}) QTest::addRow("%d", s) << s;
    }
    void findShift()
    {
        QFETCH(int, shift);
        const QImage tall = texture(300, 900);
        QCOMPARE(Logic::findShift(tall.copy(0, 0, 300, 400), tall.copy(0, shift, 300, 400)), shift);
    }
    void findShiftEdgeCases()
    {
        const QImage a = texture(300, 400);
        QCOMPARE(Logic::findShift(a, a), 0);
        QCOMPARE(Logic::findShift(a, a.copy(0, 0, 300, 399)), -1); // 尺寸不同
        QCOMPARE(Logic::findShift(a.copy(0, 0, 300, 10), a.copy(0, 0, 300, 10)), -1); // 太矮
        QImage noise(300, 400, QImage::Format_RGB32);
        for (int y = 0; y < 400; ++y)
            for (int x = 0; x < 300; ++x) noise.setPixel(x, y, qRgb((x * 31 + y * 17 + x * y) % 256, (x * y * 3) % 256, (x + 5 * y) % 256));
        QCOMPARE(Logic::findShift(a, noise), -1); // 不相关画面
        QImage flat(300, 400, QImage::Format_RGB32);
        flat.fill(Qt::white);
        QCOMPARE(Logic::findShift(flat, flat), -1); // 纯色无法判断
    }

    void expandPattern()
    {
        const QDateTime t(QDate(2026, 10, 21), QTime(13, 5, 9));
        QCOMPARE(Logic::expandPattern(QStringLiteral("QingYing_{yyyyMMdd_HHmmss}"), t), QStringLiteral("QingYing_20261021_130509"));
        QCOMPARE(Logic::expandPattern(QStringLiteral("{yyyy}/{MM}"), t), QStringLiteral("2026_10"));
        QCOMPARE(Logic::expandPattern(QStringLiteral("a:b*c?\"<>|"), t), QStringLiteral("a_b_c_____"));
        QCOMPARE(Logic::expandPattern(QStringLiteral("  "), t), QStringLiteral("QingYing_20261021_130509"));
        QCOMPARE(Logic::expandPattern(QStringLiteral("截图 {HH-mm}"), t), QStringLiteral("截图 13-05"));
        QCOMPARE(Logic::expandPattern(QStringLiteral("no braces"), t), QStringLiteral("no braces"));
    }

    void formatColor()
    {
        const QColor c(0x4F, 0x6B, 0xFF);
        QCOMPARE(Logic::formatColor(c, QStringLiteral("hex")), QStringLiteral("#4F6BFF"));
        QCOMPARE(Logic::formatColor(c, QStringLiteral("hexlower")), QStringLiteral("#4f6bff"));
        QCOMPARE(Logic::formatColor(c, QStringLiteral("rgb")), QStringLiteral("rgb(79, 107, 255)"));
        QCOMPARE(Logic::formatColor(c, QStringLiteral("rgbplain")), QStringLiteral("79, 107, 255"));
        QCOMPARE(Logic::formatColor(c, QStringLiteral("unknown")), QStringLiteral("#4F6BFF"));
    }

    void isNewerVersion_data()
    {
        QTest::addColumn<QString>("remote");
        QTest::addColumn<QString>("current");
        QTest::addColumn<bool>("newer");
        QTest::newRow("patch") << "v0.4.1" << "0.4.0" << true;
        QTest::newRow("same") << "v0.4.0" << "0.4.0" << false;
        QTest::newRow("older") << "v0.3.0" << "0.4.0" << false;
        QTest::newRow("numeric not lexical") << "v0.10.0" << "0.9.9" << true;
        QTest::newRow("trailing zero") << "v1.0" << "1.0.0" << false;
        QTest::newRow("prerelease lower") << "v0.5.0-beta.1" << "0.5.0" << false;
        QTest::newRow("release beats pre") << "v0.5.0" << "0.5.0-beta.1" << true;
        QTest::newRow("pre newer core") << "v0.5.0-rc.1" << "0.4.1" << true;
        QTest::newRow("build metadata") << "v0.4.0+ci.5" << "0.4.0" << false;
        QTest::newRow("garbage") << "nightly" << "0.4.0" << false;
        QTest::newRow("major") << "V2.0.0" << "1.9.9" << true;
        QTest::newRow("from042") << "v0.5.1" << "0.4.2" << true;
        QTest::newRow("from042to052") << "v0.5.2" << "0.4.2" << true;
    }
    void isNewerVersion()
    {
        QFETCH(QString, remote);
        QFETCH(QString, current);
        QFETCH(bool, newer);
        QCOMPARE(Logic::isNewerVersion(remote, current), newer);
    }

    void sha256()
    {
        QCOMPARE(Logic::sha256Hex("abc"), QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        const QString h = Logic::sha256Hex("installer");
        const QString sums = QStringLiteral("%1  QingYing-0.4.1-windows-x64-setup.exe\r\n"
                                            "%2 *QingYing-0.4.1-windows-x64-portable.zip\n")
                                 .arg(h.toUpper(), Logic::sha256Hex("zip"));
        QCOMPARE(Logic::expectedSha256(sums, QStringLiteral("QingYing-0.4.1-windows-x64-setup.exe")), h);
        QCOMPARE(Logic::expectedSha256(sums, QStringLiteral("QingYing-0.4.1-windows-x64-portable.zip")), Logic::sha256Hex("zip"));
        QVERIFY(Logic::expectedSha256(sums, QStringLiteral("missing.exe")).isEmpty());
        QVERIFY(Logic::expectedSha256(QStringLiteral("nothex  a.exe"), QStringLiteral("a.exe")).isEmpty());
        QVERIFY(Logic::expectedSha256(QString(), QStringLiteral("a.exe")).isEmpty());
        QVERIFY(Logic::sha256Hex("installer!") != Logic::expectedSha256(sums, QStringLiteral("QingYing-0.4.1-windows-x64-setup.exe")));
    }

    void jsonValue()
    {
        const QByteArray j = R"({"data":{"url":"https://a/b.png","list":[{"u":"x"},{"u":"y"}]},"n":3,"link":"top"})";
        QCOMPARE(Logic::jsonValue(j, QStringLiteral("data.url")), QStringLiteral("https://a/b.png"));
        QCOMPARE(Logic::jsonValue(j, QStringLiteral("data.list.1.u")), QStringLiteral("y"));
        QCOMPARE(Logic::jsonValue(j, QStringLiteral("link")), QStringLiteral("top"));
        QVERIFY(Logic::jsonValue(j, QStringLiteral("n")).isEmpty());
        QVERIFY(Logic::jsonValue(j, QStringLiteral("data.missing")).isEmpty());
        QVERIFY(Logic::jsonValue(j, QStringLiteral("")).isEmpty());
        QVERIFY(Logic::jsonValue("not json", QStringLiteral("a")).isEmpty());
    }
    // ---- GIF 调色板 ----
    void gifFewColorsExact()
    {
        std::vector<uint32_t> s = {0xFF0000, 0x00FF00, 0x0000FF, 0xFF0000};
        const GifQuant::Palette p = GifQuant::medianCut(s);
        QCOMPARE(p.size, 4); // 透明 + 3
        GifQuant::Mapper m(p);
        const int i = m.nearest(0, 255, 0);
        QCOMPARE(int(p.rgb[i * 3 + 1]), 255);
        QCOMPARE(int(p.rgb[i * 3]), 0);
    }
    void gifManyColorsBounded()
    {
        std::vector<uint32_t> s;
        for (int r = 0; r < 256; r += 4)
            for (int g = 0; g < 256; g += 8)
                for (int b = 0; b < 256; b += 16) s.push_back((uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b));
        const GifQuant::Palette p = GifQuant::medianCut(s);
        QVERIFY(p.size <= 256);
        QVERIFY(p.size > 200);
        // 平均量化误差应很小
        GifQuant::Mapper m(p);
        double err = 0;
        for (uint32_t c : s) {
            const int r = c >> 16, g = (c >> 8) & 255, b = c & 255, i = m.nearest(r, g, b);
            err += std::abs(r - p.rgb[i * 3]) + std::abs(g - p.rgb[i * 3 + 1]) + std::abs(b - p.rgb[i * 3 + 2]);
        }
        QVERIFY2(err / s.size() < 40, qPrintable(QString::number(err / s.size())));
    }
    void gifEmptySamples() { QCOMPARE(GifQuant::medianCut({}).size, 2); }
    void gifDeltaTransparent()
    {
        const int w = 8, h = 4;
        std::vector<uint8_t> a(w * h * 4, 0), b, out(w * h * 4);
        for (int i = 0; i < w * h; ++i) a[i * 4] = uint8_t(i * 8);
        b = a;
        b[5 * 4 + 1] = 200; // 只改一个像素
        std::vector<uint32_t> s;
        GifQuant::sample(a.data(), w, h, 1000, s);
        GifQuant::sample(b.data(), w, h, 1000, s);
        GifQuant::Mapper m(GifQuant::medianCut(s));
        QCOMPARE(GifQuant::quantize(a.data(), nullptr, w, h, m, false, out.data()), w * h);
        QCOMPARE(GifQuant::quantize(b.data(), a.data(), w, h, m, false, out.data()), 1);
        QCOMPARE(int(out[0 * 4 + 3]), GifQuant::kTransparent);
        QVERIFY(out[5 * 4 + 3] != GifQuant::kTransparent);
    }
    void gifDitherReducesBanding()
    {
        // 黑白两色调色板量化 50% 灰：不抖动全部变成同一色，抖动后平均亮度接近 128
        const int w = 64, h = 64;
        std::vector<uint8_t> img(w * h * 4, 128), out(w * h * 4);
        GifQuant::Mapper m(GifQuant::medianCut({0x000000, 0xFFFFFF}));
        GifQuant::quantize(img.data(), nullptr, w, h, m, false, out.data());
        double flat = 0;
        for (int i = 0; i < w * h; ++i) flat += out[i * 4];
        GifQuant::quantize(img.data(), nullptr, w, h, m, true, out.data());
        double dith = 0;
        for (int i = 0; i < w * h; ++i) dith += out[i * 4];
        flat /= w * h;
        dith /= w * h;
        QVERIFY(std::abs(flat - 128) > 100);
        QVERIFY2(std::abs(dith - 128) < 12, qPrintable(QString::number(dith)));
    }

    // ---- 指针合成 ----
    void cursorColorAlpha()
    {
        uint8_t dst[4 * 4 * 4] = {};
        const uint8_t shape[2 * 2 * 4] = {255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 128, 10, 10, 10, 255};
        CursorBlend::draw(dst, 16, 4, 4, CursorBlend::Color, shape, 2, 2, 8, 1, 1);
        QCOMPARE(int(dst[(1 * 4 + 1) * 4]), 255);          // 不透明：覆盖
        QCOMPARE(int(dst[(1 * 4 + 2) * 4 + 1]), 0);        // 全透明：不变
        QCOMPARE(int(dst[(2 * 4 + 1) * 4 + 2]), 128);      // 半透明：混合
        QCOMPARE(int(dst[0]), 0);                           // 形状外：不变
    }
    void cursorClipped()
    {
        uint8_t dst[2 * 2 * 4] = {};
        const uint8_t shape[2 * 2 * 4] = {9, 9, 9, 255, 9, 9, 9, 255, 9, 9, 9, 255, 9, 9, 9, 255};
        CursorBlend::draw(dst, 8, 2, 2, CursorBlend::Color, shape, 2, 2, 8, -1, -1); // 只有右下角落在画面内
        QCOMPARE(int(dst[0]), 9);
        QCOMPARE(int(dst[4]), 0);
        CursorBlend::draw(dst, 8, 2, 2, CursorBlend::Color, shape, 2, 2, 8, 5, 5); // 完全在外
    }
    void cursorMonochrome()
    {
        // 8x1 形状：AND 行 0b11110000，XOR 行 0b10101010 → 像素 0 反色、1 不变、4 白、5 黑
        uint8_t dst[8 * 4];
        for (int i = 0; i < 8; ++i) dst[i * 4] = dst[i * 4 + 1] = dst[i * 4 + 2] = 100;
        const uint8_t shape[2] = {0xF0, 0xAA};
        CursorBlend::draw(dst, 32, 8, 1, CursorBlend::Monochrome, shape, 8, 2, 1, 0, 0);
        QCOMPARE(int(dst[0]), 155);
        QCOMPARE(int(dst[4]), 100);
        QCOMPARE(int(dst[16]), 255);
        QCOMPARE(int(dst[20]), 0);
    }
    void cursorMasked()
    {
        uint8_t dst[2 * 4] = {100, 100, 100, 0, 100, 100, 100, 0};
        const uint8_t shape[2 * 4] = {7, 8, 9, 0, 0xFF, 0xFF, 0xFF, 0xFF};
        CursorBlend::draw(dst, 8, 2, 1, CursorBlend::MaskedColor, shape, 2, 1, 8, 0, 0);
        QCOMPARE(int(dst[0]), 7);   // 掩码 0：替换
        QCOMPARE(int(dst[4]), 155); // 掩码 FF：异或
    }



    void tagFromReleaseUrl()
    {
        QCOMPARE(Logic::releaseTagFromUrl(QUrl(QStringLiteral("https://github.com/nullTerminatedCat/QingYing/releases/tag/v0.5.1"))),
                 QStringLiteral("v0.5.1"));
        QCOMPARE(Logic::releaseTagFromUrl(QUrl(QStringLiteral("https://github.com/a/b/releases/tag/0.5.2/"))),
                 QStringLiteral("0.5.2"));
        QVERIFY(Logic::releaseTagFromUrl(QUrl(QStringLiteral("https://github.com/a/b/releases"))).isEmpty());
        QVERIFY(Logic::releaseTagFromUrl(QUrl(QStringLiteral("https://example.com/tag/v1"))).isEmpty());
    }

    void mostlyCjk()
    {
        // 用 UTF-8 字面量，避免源文件编码导致中文损坏；用例保证 CJK/拉丁数量不暧昧
        QVERIFY(Logic::mostlyCjk(QString::fromUtf8(u8"你好世界")));
        QVERIFY(Logic::mostlyCjk(QString::fromUtf8(u8"简体中文测试文本")));
        QVERIFY(Logic::mostlyCjk(QString::fromUtf8(u8"中文主体内容 with ok")));
        QVERIFY(!Logic::mostlyCjk(QStringLiteral("Hello World")));
        QVERIFY(!Logic::mostlyCjk(QStringLiteral("ABC 123")));
        QVERIFY(!Logic::mostlyCjk(QString()));
        QVERIFY(!Logic::mostlyCjk(QString::fromUtf8(u8"Hello 你好"))); // 5 latin vs 2 cjk
    }
};

QTEST_GUILESS_MAIN(TestLogic)
#include "tst_logic.moc"
