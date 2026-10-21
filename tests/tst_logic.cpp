#include "../src/Logic.h"

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
};

QTEST_GUILESS_MAIN(TestLogic)
#include "tst_logic.moc"
