// SPDX-FileCopyrightText: 2019 Yury Gubich <blue@macaw.me>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppLogger.h"
#include "QXmppUploadRequestManager.h"

#include "util.h"
#include <QByteArray>
#include <QMimeDatabase>
#include <QObject>

class TestHelper : public QObject
{
    Q_OBJECT

public:
    TestHelper(bool expectedEvent, bool expectedError);
    ~TestHelper();

public slots:
    void onSlotReceived(const QXmppHttpUploadSlotIq &slot);
    void onRequestFailed(const QXmppHttpUploadRequestIq &request);

private:
    bool expectedEvent;
    bool expectedError;
    bool event;
    bool error;
};

TestHelper::TestHelper(bool p_expectedEvent, bool p_expectedError) : QObject(),
                                                                     expectedEvent(p_expectedEvent),
                                                                     expectedError(p_expectedError),
                                                                     event(false),
                                                                     error(false)
{
}

TestHelper::~TestHelper()
{
    QCOMPARE(event, expectedEvent);
    QCOMPARE(error, expectedError);
}

void TestHelper::onRequestFailed(const QXmppHttpUploadRequestIq &)
{
    event = true;
    error = true;
}

void TestHelper::onSlotReceived(const QXmppHttpUploadSlotIq &)
{
    event = true;
    error = false;
}

class tst_QXmppHttpUploadManager : public QObject
{
    Q_OBJECT

protected slots:
    void onLoggerMessage(QXmppLogger::MessageType type, const QString &text) const;

private slots:
    void initTestCase();

    void testDiscoveryService_data();
    void testDiscoveryService();

    void testHandleStanza_data();
    void testHandleStanza();

    void testSending_data();
    void testSending();

    void testUploadService();

private:
    QXmppUploadRequestManager *manager;
    QXmppClient client;
    QXmppDiscoveryManager *discovery;
    QString uploadServiceName;
    qint64 maxFileSize;

    QMimeType lastMimeType;
    QString lastFileName;
    qint64 lastFileSize;
};

void tst_QXmppHttpUploadManager::onLoggerMessage(QXmppLogger::MessageType type, const QString &text) const
{
    QCOMPARE(type, QXmppLogger::SentMessage);

    QDomDocument doc;
    QVERIFY(doc.setContent(text, true));
    QDomElement element = doc.documentElement();

    QXmppHttpUploadRequestIq iq;
    iq.parse(element);

    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.to(), uploadServiceName);
    QCOMPARE(iq.fileName(), lastFileName);
    QCOMPARE(iq.size(), lastFileSize);
    QCOMPARE(iq.contentType(), lastMimeType);
}

void tst_QXmppHttpUploadManager::initTestCase()
{
    uploadServiceName = "upload.montague.tld";
    maxFileSize = 500UL * 1024UL * 1024UL;
    manager = new QXmppUploadRequestManager();
    discovery = client.findExtension<QXmppDiscoveryManager>();
    client.addExtension(manager);
}

void tst_QXmppHttpUploadManager::testHandleStanza_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accepted");
    QTest::addColumn<bool>("event");
    QTest::addColumn<bool>("error");

    QTest::newRow("notAccepted")
        << QByteArray("<message xmlns='jabber:client' "
                      "from='romeo@montague.example' "
                      "to='romeo@montague.example/home' "
                      "type='chat'>"
                      "<received xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client' "
                      "from='juliet@capulet.example/balcony' "
                      "to='romeo@montague.example/garden' "
                      "type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</received>"
                      "</message>")
        << false << false << false;

    QTest::newRow("slotReceived")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='result'>"
                      "<slot xmlns='urn:xmpp:http:upload:0'>"
                      "<put url='https://upload.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg'>"
                      "<header name='Authorization'>Basic Base64String==</header>"
                      "<header name='Cookie'>foo=bar; user=romeo</header>"
                      "</put>"
                      "<get url='https://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg' />"
                      "</slot>"
                      "</iq>")
        << true << true << false;

    QTest::newRow("tooLargeError")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='error'>"
                      "<request xmlns='urn:xmpp:http:upload:0' filename='très cool.jpg' size='23456' content-type='image/jpeg' />"
                      "<error type='modify'>"
                      "<not-acceptable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas' />"
                      "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>File too large. The maximum file size is 20000 bytes</text>"
                      "<file-too-large xmlns='urn:xmpp:http:upload:0'>"
                      "<max-file-size>20000</max-file-size>"
                      "</file-too-large>"
                      "</error>"
                      "</iq>")
        << true << true << true;

    QTest::newRow("quotaReachedError")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='error'>"
                      "<request xmlns='urn:xmpp:http:upload:0' filename='très cool.jpg' size='23456' content-type='image/jpeg' />"
                      "<error type='wait'>"
                      "<resource-constraint xmlns='urn:ietf:params:xml:ns:xmpp-stanzas' />"
                      "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>Quota reached. You can only upload 5 files in 5 minutes</text>"
                      "<retry xmlns='urn:xmpp:http:upload:0' stamp='2017-12-03T23:42:05Z' />"
                      "</error>"
                      "</iq>")
        << true << true << true;
}

void tst_QXmppHttpUploadManager::testHandleStanza()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accepted);
    QFETCH(bool, event);
    QFETCH(bool, error);

    TestHelper helper(event, error);
    connect(manager, &QXmppUploadRequestManager::slotReceived, &helper, &TestHelper::onSlotReceived);
    connect(manager, &QXmppUploadRequestManager::requestFailed, &helper, &TestHelper::onRequestFailed);

    QDomDocument doc;
    QVERIFY(doc.setContent(xml, true));
    QDomElement element = doc.documentElement();

    bool realAccepted = manager->handleStanza(element);

    QCOMPARE(realAccepted, accepted);
}

void tst_QXmppHttpUploadManager::testDiscoveryService_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("discovered");

    QTest::newRow("mixDiscoveryStanzaIq")
        << QByteArray("<iq from='mix.shakespeare.example' id='lx09df27' to='hag66@shakespeare.example/UUID-c8y/1573' type='result'>"
                      "<query xmlns='http://jabber.org/protocol/disco#info'>"
                      "<identity category='conference' name='Shakespearean Chat Service' type='mix '/>"
                      "<feature var='urn:xmpp:mix:core:1' />"
                      "<feature var='urn:xmpp:mix:core:1#searchable' />"
                      "</query>"
                      "</iq>")
        << false;

    QTest::newRow("HTTPUploadDiscoveryStanzaIq")
        << QByteArray("<iq from='" + uploadServiceName.toUtf8() + "' id='step_02' to='romeo@montague.tld/garden' type='result'>"
                                                                  "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                                                  "<identity category='store' type='file' name='HTTP File Upload' />"
                                                                  "<feature var='urn:xmpp:http:upload:0' />"
                                                                  "<x type='result' xmlns='jabber:x:data'>"
                                                                  "<field var='FORM_TYPE' type='hidden'>"
                                                                  "<value>urn:xmpp:http:upload:0</value>"
                                                                  "</field>"
                                                                  "<field var='max-file-size'>"
                                                                  "<value>" +
                      QByteArray::number(maxFileSize) + "</value>"
                                                        "</field>"
                                                        "</x>"
                                                        "</query>"
                                                        "</iq>")
        << true;
}

void tst_QXmppHttpUploadManager::testDiscoveryService()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, discovered);

    QDomDocument doc;
    QVERIFY(doc.setContent(xml, true));
    QDomElement element = doc.documentElement();

    bool accepted = discovery->handleStanza(element);
    QCOMPARE(accepted, true);
    QCOMPARE(manager->serviceFound(), discovered);

    if (manager->serviceFound()) {
        QCOMPARE(manager->uploadServices().at(0).jid(), uploadServiceName);
        QCOMPARE(manager->uploadServices().at(0).sizeLimit(), maxFileSize);
    }
}

void tst_QXmppHttpUploadManager::testSending_data()
{
    QTest::addColumn<QFileInfo>("fileInfo");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<qint64>("fileSize");
    QTest::addColumn<QString>("fileType");

    QTest::newRow("fileInfo")
        << QFileInfo(":/test.svg")
        << "test.svg"
        << 2280LL
        << "image/svg+xml";

    QTest::newRow("fileWithSizeBelowLimit")
        << QFileInfo()
        << "whatever.jpeg"
        << 698547LL
        << "image/jpeg";

    QTest::newRow("fileWithSizeAboveLimit")
        << QFileInfo()
        << "some.pdf"
        << 65896498547LL
        << "application/pdf";

    // there is no size above limit handling in request manager
    // there is also no code that selects an upload service with proper
    // size limit above requesting file size.
    // Is it something to worry about?
}

void tst_QXmppHttpUploadManager::testSending()
{
    QFETCH(QFileInfo, fileInfo);
    QFETCH(QString, fileName);
    QFETCH(qint64, fileSize);
    QFETCH(QString, fileType);

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    client.setLogger(&logger);

    lastMimeType = QMimeDatabase().mimeTypeForName(fileType);
    connect(&logger, &QXmppLogger::message, this, &tst_QXmppHttpUploadManager::onLoggerMessage);

    lastFileName = fileName;
    lastFileSize = fileSize;

    QString returnId;
    if (!fileInfo.baseName().isEmpty()) {
        returnId = manager->requestUploadSlot(fileInfo);
    } else {
        returnId = manager->requestUploadSlot(fileName, fileSize, lastMimeType);
    }

    // The client is not connected, so we never get an ID back (the packet was not sent).
    QVERIFY(returnId.isNull());
}

void tst_QXmppHttpUploadManager::testUploadService()
{
    QXmppUploadService service;
    QCOMPARE(service.sizeLimit(), -1LL);
    QVERIFY(service.jid().isNull());

    service.setSizeLimit(256LL * 1024LL * 1024LL);
    QCOMPARE(service.sizeLimit(), 256LL * 1024LL * 1024LL);

    service.setJid(QStringLiteral("upload.shakespeare.lit"));
    QCOMPARE(service.jid(), QStringLiteral("upload.shakespeare.lit"));
}

QTEST_MAIN(tst_QXmppHttpUploadManager)
#include "tst_qxmpphttpuploadmanager.moc"