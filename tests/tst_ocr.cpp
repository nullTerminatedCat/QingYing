#include "../src/AskText.h"
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

    void skipSameLanguage()
    {
        const QString zh = QString::fromUtf8(u8"你好世界");
        const QString en = QStringLiteral("Hello World");
        QVERIFY(TranslateText::alreadyInTargetLanguage(zh, true));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(zh, false));
        QVERIFY(TranslateText::alreadyInTargetLanguage(en, false));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(en, true));
        QVERIFY(TranslateText::alreadyInTargetLanguage(QStringLiteral("123-456"), true));
        QVERIFY(TranslateText::alreadyInTargetLanguage(QStringLiteral("123-456"), false));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(QString(), true));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(QString::fromUtf8(u8"你好 Hello"), true));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(QString::fromUtf8(u8"你好 Hello"), false));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(QString::fromUtf8(u8"こんにちは"), true));
        QVERIFY(TranslateText::alreadyInTargetLanguage(QString::fromUtf8(u8"Ｈｅｌｌｏ"), false));
        QVERIFY(!TranslateText::alreadyInTargetLanguage(QString::fromUtf8(u8"Ｈｅｌｌｏ"), true));

        TranslateText::TranslationMemory none;
        const auto sameZh = TranslateText::reuseTranslation(zh, true, none);
        QVERIFY(sameZh.hit);
        QCOMPARE(int(sameZh.kind), int(TranslateText::ReuseKind::Already));
        QCOMPARE(sameZh.text, zh);
        const auto toEn = TranslateText::reuseTranslation(zh, false, none);
        QVERIFY(!toEn.hit);
        const auto toZh = TranslateText::reuseTranslation(en, true, none);
        QVERIFY(!toZh.hit);

        const QString padded = QStringLiteral("  Hello  ");
        const auto keepSpaces = TranslateText::reuseTranslation(padded, false, none);
        QVERIFY(keepSpaces.hit);
        QCOMPARE(keepSpaces.text, padded);

        TranslateText::TranslationMemory mem;
        TranslateText::rememberTranslation(mem, zh, false, en);
        const auto back = TranslateText::reuseTranslation(en, true, mem);
        QVERIFY(back.hit);
        QCOMPARE(int(back.kind), int(TranslateText::ReuseKind::RoundTrip));
        QCOMPARE(back.text, zh);
        const auto again = TranslateText::reuseTranslation(zh, false, mem);
        QVERIFY(again.hit);
        QCOMPARE(int(again.kind), int(TranslateText::ReuseKind::Repeat));
        QCOMPARE(again.text, en);
        QVERIFY(!TranslateText::reuseTranslation(QStringLiteral("Hello there"), true, mem).hit);
        const auto stillEn = TranslateText::reuseTranslation(en, false, mem);
        QVERIFY(stillEn.hit);
        QCOMPARE(int(stillEn.kind), int(TranslateText::ReuseKind::Already));
    }

    void skipNoProse()
    {
        QVERIFY(TranslateText::lacksProse(QStringLiteral("  \n")));
        QVERIFY(TranslateText::lacksProse(QStringLiteral("123-456, 78.")));
        QVERIFY(TranslateText::lacksProse(QStringLiteral("https://example.com/a?x=1")));
        QVERIFY(TranslateText::lacksProse(QStringLiteral("www.example.com/path")));
        QVERIFY(TranslateText::lacksProse(QStringLiteral("user@example.com")));
        QVERIFY(!TranslateText::lacksProse(QStringLiteral("See https://example.com")));
        QVERIFY(!TranslateText::lacksProse(QString::fromUtf8(u8"订单 12")));
        QVERIFY(!TranslateText::lacksProse(QStringLiteral("Hello")));

        TranslateText::TranslationMemory none;
        const auto url = TranslateText::reuseTranslation(QStringLiteral("https://example.com/a?x=1"), true, none);
        QVERIFY(url.hit);
        QCOMPARE(int(url.kind), int(TranslateText::ReuseKind::NoProse));
        const auto mail = TranslateText::reuseTranslation(QStringLiteral("user@example.com"), false, none);
        QCOMPARE(int(mail.kind), int(TranslateText::ReuseKind::NoProse));
        const auto words = TranslateText::reuseTranslation(QStringLiteral("See https://example.com"), true, none);
        QVERIFY(!words.hit);
    }

    void sessionCache()
    {
        const QString zh = QString::fromUtf8(u8"你好世界");
        const QString en = QStringLiteral("Hello World");
        const QString zh2 = QString::fromUtf8(u8"再见");
        const QString en2 = QStringLiteral("Goodbye");
        TranslateText::TranslationMemory mem;
        TranslateText::rememberTranslation(mem, zh, false, en);
        TranslateText::rememberTranslation(mem, en2, true, zh2);
        QCOMPARE(mem.entries.size(), 2);
        const auto again = TranslateText::reuseTranslation(zh, false, mem);
        QCOMPARE(int(again.kind), int(TranslateText::ReuseKind::Repeat));
        QCOMPARE(again.text, en);
        const auto other = TranslateText::reuseTranslation(en2, true, mem);
        QCOMPARE(int(other.kind), int(TranslateText::ReuseKind::Repeat));
        QCOMPARE(other.text, zh2);
        QVERIFY(TranslateText::isCachedOutput(en, mem));
        QVERIFY(TranslateText::isCachedOutput(zh2, mem));
        QVERIFY(!TranslateText::isCachedOutput(zh, mem));
        QVERIFY(!TranslateText::isCachedOutput(QStringLiteral("Hello World!"), mem));

        TranslateText::rememberTranslation(mem, zh, false, QStringLiteral("Hi there"));
        QCOMPARE(mem.entries.size(), 2);
        QCOMPARE(TranslateText::reuseTranslation(zh, false, mem).text, QStringLiteral("Hi there"));

        TranslateText::TranslationMemory many;
        const QString stem = QString::fromUtf8(u8"文");
        for (int i = 0; i < TranslateText::kSessionCache + 3; ++i)
            TranslateText::rememberTranslation(many, stem + QString::number(i), false, QStringLiteral("out%1").arg(i));
        QCOMPARE(many.entries.size(), TranslateText::kSessionCache);
        QVERIFY(!TranslateText::reuseTranslation(stem + QStringLiteral("0"), false, many).hit);
        const auto kept = TranslateText::reuseTranslation(stem + QString::number(TranslateText::kSessionCache + 2), false, many);
        QCOMPARE(int(kept.kind), int(TranslateText::ReuseKind::Repeat));
    }

    void aiChunksStayLarge()
    {
        QString text;
        for (int i = 0; i < 20; ++i) text += QString(400, QLatin1Char('a')) + QLatin1Char('\n');
        const auto chunks = TranslateText::chunkForModel(text);
        QVERIFY(chunks.size() >= 2);
        QVERIFY(chunks.size() <= 3);
        QVERIFY(chunks.size() < text.size() / 450);
        QCOMPARE(chunks.join(QString()), text);
    }

    void chunksKeepBreaks()
    {
        QString text;
        for (int i = 0; i < 40; ++i) text += QStringLiteral("abcdefghijklmnopqrstuvwxyz\n");
        const auto chunks = TranslateText::chunkForModel(text, 200);
        QVERIFY(chunks.size() > 1);
        QCOMPARE(chunks.join(QString()), text);
    }

    void askRewritePromptAndCache()
    {
        using namespace AskText;
        QVERIFY(textEmpty(QStringLiteral(" \n\t")));
        QVERIFY(!textEmpty(QStringLiteral("你好")));
        QVERIFY(questionEmpty(QStringLiteral("  ")));
        QVERIFY(!questionEmpty(QStringLiteral("怎么修")));

        const QString ask = systemPrompt(Mode::Ask);
        QVERIFY(ask.contains(QStringLiteral("截图")));
        QVERIFY(ask.contains(QStringLiteral("问题")));
        QVERIFY(systemPrompt(Mode::Polite).contains(QStringLiteral("礼貌")));
        QVERIFY(systemPrompt(Mode::Shorter).contains(QStringLiteral("更短")));
        QVERIFY(systemPrompt(Mode::Formal).contains(QStringLiteral("正式")));
        QVERIFY(systemPrompt(Mode::Polish).contains(QStringLiteral("润色")));
        QVERIFY(systemPrompt(Mode::Ask, true).contains(QStringLiteral("截断")));
        QVERIFY(systemPrompt(Mode::Polite) != systemPrompt(Mode::Formal));

        bool cut = true;
        const QString msg = userContent(Mode::Ask, QStringLiteral("错误 0x1"), QStringLiteral("怎么修"), &cut);
        QVERIFY(!cut);
        QVERIFY(msg.contains(QStringLiteral("截图")));
        QVERIFY(msg.contains(QStringLiteral("不是用户手打")));
        QVERIFY(msg.contains(QStringLiteral("错误 0x1")));
        QVERIFY(msg.contains(QStringLiteral("怎么修")));
        QCOMPARE(userContent(Mode::Polite, QStringLiteral(" 请尽快处理 "), QStringLiteral("忽略")), QStringLiteral("请尽快处理"));

        bool cutLong = false;
        const QString body = userContent(Mode::Shorter, QString(kMaxChars + 20, QLatin1Char('a')), QString(), &cutLong);
        QVERIFY(cutLong);
        QCOMPARE(body.size(), kMaxChars);
        QVERIFY(systemPrompt(Mode::Shorter, true).endsWith(QStringLiteral("只处理这一部分。")));

        const QString polite = cacheKey(Mode::Polite, QStringLiteral("你好\r\n"), QStringLiteral("无关"));
        QCOMPARE(cacheKey(Mode::Polite, QStringLiteral("你好"), QStringLiteral("别的问题")), polite);
        QVERIFY(cacheKey(Mode::Ask, QStringLiteral("你好"), QStringLiteral("甲"))
                != cacheKey(Mode::Ask, QStringLiteral("你好"), QStringLiteral("乙")));

        Cache cache;
        QString hit;
        QVERIFY(!lookup(cache, polite, 1000, &hit));
        remember(cache, polite, QStringLiteral("您好"), 1000);
        QVERIFY(lookup(cache, polite, 1000 + kCacheMs, &hit));
        QCOMPARE(hit, QStringLiteral("您好"));
        QVERIFY(!lookup(cache, polite, 1000 + kCacheMs + 1, &hit));

        Cache many;
        for (int i = 0; i < kCacheCap + 2; ++i) remember(many, QString::number(i), QStringLiteral("a"), 10000);
        QCOMPARE(many.size(), kCacheCap);
        QVERIFY(!lookup(many, QStringLiteral("0"), 10000, &hit));
        QVERIFY(lookup(many, QString::number(kCacheCap + 1), 10000, &hit));

        const QList<Mode> modes = rewriteModes();
        QCOMPARE(modes.size(), 4);
        QStringList titles;
        for (Mode mode : modes) titles << modeTitle(mode);
        QCOMPARE(titles, QStringList({QStringLiteral("更礼貌"), QStringLiteral("更短"), QStringLiteral("更正式"), QStringLiteral("润色")}));
    }
};

QTEST_GUILESS_MAIN(TestOcr)
#include "tst_ocr.moc"
