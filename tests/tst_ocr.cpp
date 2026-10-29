#include "../src/OcrPost.h"
#include "../src/TranslateText.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

class TestOcr : public QObject {
    Q_OBJECT
private slots:
    void detInputSize()
    {
        const auto small = OcrPost::detInputSize(100, 50);
        QCOMPARE(small.width, 96);
        QCOMPARE(small.height, 64);
        const auto big = OcrPost::detInputSize(2000, 1000, 960);
        QCOMPARE(big.width, 960);
        QCOMPARE(big.height, 480);
        QVERIFY(big.width % 32 == 0);
        QVERIFY(big.height % 32 == 0);
    }

    void detectFilledRect()
    {
        const int w = 120, h = 60;
        QVector<float> prob(w * h, 0.05f);
        for (int y = 16; y < 36; ++y)
            for (int x = 20; x < 90; ++x) prob[y * w + x] = 0.95f;
        const auto boxes = OcrPost::detectBoxes(prob.constData(), w, h, w, h);
        QCOMPARE(boxes.size(), 1);
        const auto& q = boxes[0].quad;
        float cx = 0, cy = 0;
        for (const auto& p : q.p) {
            cx += p.x;
            cy += p.y;
        }
        cx *= 0.25f;
        cy *= 0.25f;
        QVERIFY(cx > 45 && cx < 65);
        QVERIFY(cy > 20 && cy < 34);
        QVERIFY(boxes[0].score > 0.7f);
        const float width = std::hypot(q.p[1].x - q.p[0].x, q.p[1].y - q.p[0].y);
        QVERIFY(width > 70);
    }

    void ctcAndDict()
    {
        const auto dict = OcrPost::dictFromUtf8(QByteArray("你\n好\n \n"));
        QCOMPARE(dict.size(), 3);
        QCOMPARE(dict[2], QStringLiteral(" "));
        // blank, 你, 你, blank, 好
        const float rows[] = {
            0.9f, 0.1f, 0.0f, 0.0f, 0.1f, 0.8f, 0.1f, 0.0f, 0.2f, 0.7f, 0.1f, 0.0f, 0.8f, 0.1f, 0.1f, 0.0f, 0.1f, 0.1f, 0.8f, 0.0f,
        };
        QCOMPARE(OcrPost::ctcGreedy(rows, 5, 4, dict), QStringLiteral("你好"));
    }

    void joinLines()
    {
        QList<OcrPost::LinePiece> pieces = {
            {QRectF(0, 0, 40, 16), QStringLiteral("Hello")},
            {QRectF(70, 1, 30, 16), QStringLiteral("World")},
            {QRectF(0, 40, 50, 16), QStringLiteral("下一行")},
        };
        QCOMPARE(OcrPost::joinLines(pieces), QStringLiteral("Hello World\n下一行"));
        QList<OcrPost::LinePiece> mixed = {{QRectF(0, 0, 80, 20), QString::fromUtf8("青影Hello")}};
        QCOMPARE(OcrPost::joinLines(mixed), QString::fromUtf8("青影 Hello"));
    }

    void warpIdentity()
    {
        QImage img(12, 8, QImage::Format_RGB888);
        img.fill(Qt::white);
        img.setPixel(3, 2, qRgb(200, 10, 10));
        OcrPost::Quad q;
        q.p = {OcrPost::Vec2{0, 0}, {float(img.width()), 0}, {float(img.width()), float(img.height())}, {0, float(img.height())}};
        const QImage out = OcrPost::warpQuad(img, q);
        QCOMPARE(out.size(), img.size());
        const QRgb px = out.pixel(3, 2);
        QVERIFY(qRed(px) > 150);
        QVERIFY(qGreen(px) < 40);
    }

    void chatUrl()
    {
        QCOMPARE(TranslateText::chatCompletionsUrl(QStringLiteral("https://api.openai.com/v1")).toString(),
                 QStringLiteral("https://api.openai.com/v1/chat/completions"));
        QCOMPARE(TranslateText::chatCompletionsUrl(QStringLiteral("https://api.openai.com/v1/")).toString(),
                 QStringLiteral("https://api.openai.com/v1/chat/completions"));
        QCOMPARE(TranslateText::chatCompletionsUrl(QStringLiteral("http://127.0.0.1:11434/v1/chat/completions")).toString(),
                 QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"));
        QVERIFY(!TranslateText::chatCompletionsUrl(QStringLiteral("ftp://example.com/v1")).isValid());
        QVERIFY(!TranslateText::chatCompletionsUrl(QString()).isValid());
    }

    void parseCompletion()
    {
        QString err;
        const QByteArray ok = R"({"choices":[{"message":{"content":"```text\n你好\n```"}}]})";
        QCOMPARE(TranslateText::parseChatCompletion(ok, &err), QStringLiteral("你好"));
        const QByteArray parts = R"({"choices":[{"message":{"content":[{"type":"text","text":"Hi"}]}}]})";
        QCOMPARE(TranslateText::parseChatCompletion(parts, &err), QStringLiteral("Hi"));
        const QByteArray bad = R"({"error":{"message":"bad key unit-test-token"}})";
        QVERIFY(TranslateText::parseChatCompletion(bad, &err).isEmpty());
        QCOMPARE(TranslateText::redactSecret(err, QStringLiteral("unit-test-token")), QStringLiteral("bad key ***"));
    }

    void chunksKeepBreaks()
    {
        QString text;
        for (int i = 0; i < 40; ++i) text += QStringLiteral("abcdefghijklmnopqrstuvwxyz\n");
        const auto chunks = TranslateText::chunkForModel(text, 200);
        QVERIFY(chunks.size() > 1);
        QCOMPARE(chunks.join(QString()), text);
    }
};

QTEST_GUILESS_MAIN(TestOcr)
#include "tst_ocr.moc"
