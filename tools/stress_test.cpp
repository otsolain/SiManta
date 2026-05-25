// =============================================================================
//  Simanta Stress Test - Fake Student Simulator
// =============================================================================
//  Spawns N fake student connections to a Simanta teacher running on this
//  machine (or any host) so you can verify the teacher correctly handles
//  large classrooms WITHOUT needing N physical PCs.
//
//  Each fake student:
//    - Opens a TCP connection to teacher
//    - Sends a HELLO with a synthetic hostname like "FakePC-01"
//    - Replies to PINGs with PONGs (so teacher won't time us out)
//    - Periodically sends a small fake JPEG frame so the tile shows up
//    - Reports its connection status to stdout
//
//  Build (from monitor/ folder):
//    cmake -B build_tools tools -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64
//    cmake --build build_tools --config Release
//
//  Run:
//    stress_test.exe --count 20 --host 127.0.0.1
//    stress_test.exe --count 50 --host 192.168.1.10 --interval 2000
//
//  Notes:
//    - Each fake student opens a TCP socket. Windows' default ephemeral port
//      pool is ~16k so 50-100 is fine.
//    - Frames are tiny (a 64x64 grey image) to keep stress test memory low,
//      so thumbnails will look mostly blank in the teacher UI -- that's
//      expected, the goal is to test connection handling, not visuals.
// =============================================================================

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QTextStream>
#include <QRandomGenerator>
#include <QPainter>
#include <cstdint>

// -- Protocol constants (must match common/protocol.h) --
namespace {
constexpr uint16_t MAGIC_BYTES   = 0xABCD;
constexpr int      HEADER_SIZE   = 12;
constexpr int      PING_INTERVAL = 3000;

enum class MsgType : uint8_t {
    HELLO       = 0x01,
    FRAME       = 0x02,
    ACK         = 0x03,
    PING        = 0x04,
    PONG        = 0x05,
    APP_STATUS  = 0x18,
};

QByteArray serializeHeader(MsgType type, uint32_t payloadLen)
{
    QByteArray data;
    data.resize(HEADER_SIZE);
    QDataStream s(&data, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s << static_cast<uint16_t>(MAGIC_BYTES);
    s << static_cast<uint16_t>(static_cast<uint8_t>(type));
    s << payloadLen;
    s << static_cast<uint32_t>(0); // reserved
    return data;
}

QByteArray packet(MsgType type, const QByteArray& payload = {})
{
    QByteArray p = serializeHeader(type, static_cast<uint32_t>(payload.size()));
    if (!payload.isEmpty()) p.append(payload);
    return p;
}
} // namespace

// -- One fake student --
class FakeStudent : public QObject
{
    Q_OBJECT
public:
    FakeStudent(int index, const QString& host, uint16_t port,
                int frameInterval, int simulatedKbps, QObject* parent = nullptr)
        : QObject(parent), m_index(index), m_host(host), m_port(port),
          m_frameInterval(frameInterval), m_simulatedKbps(simulatedKbps)
    {
        m_hostname = QStringLiteral("FakePC-%1").arg(index, 2, 10, QChar('0'));
        m_username = QStringLiteral("student%1").arg(index);

        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::connected,    this, &FakeStudent::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &FakeStudent::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead,    this, &FakeStudent::onReadyRead);
        connect(m_socket, &QTcpSocket::errorOccurred, this,
                [this](QAbstractSocket::SocketError) {
            QTextStream(stdout) << "[" << m_hostname << "] error: "
                                << m_socket->errorString() << "\n";
        });

        m_frameTimer = new QTimer(this);
        m_frameTimer->setInterval(m_frameInterval);
        connect(m_frameTimer, &QTimer::timeout, this, &FakeStudent::sendFrame);

        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        connect(m_reconnectTimer, &QTimer::timeout, this, &FakeStudent::connectNow);

        // Pre-render REALISTIC frames (full 960x540 like a real student
        // running at scale=0.5 of 1920x1080). Each frame is JPEG q60,
        // ~30-70 KB -- the same size a real student PC would emit.
        // We pre-render 3 variants so frames change between sends, which
        // forces the teacher's image decoder to actually do work instead
        // of caching the same buffer.
        for (int i = 0; i < 3; ++i) {
            m_fakeFrames.append(renderRealisticFrame(i));
        }
    }

    void start()
    {
        // Stagger initial connect by 0..1500ms so 50 fake students don't all
        // hit the teacher in the same millisecond.
        int jitter = QRandomGenerator::global()->bounded(1500);
        QTimer::singleShot(jitter, this, &FakeStudent::connectNow);
    }

private slots:
    void connectNow()
    {
        m_socket->abort();
        m_socket->connectToHost(m_host, m_port);
    }

    void onConnected()
    {
        QTextStream(stdout) << "[" << m_hostname << "] connected\n";
        m_reconnectDelay = 1000;
        sendHello();
        m_frameTimer->start();
    }

    void onDisconnected()
    {
        QTextStream(stdout) << "[" << m_hostname << "] disconnected, reconnect in "
                            << m_reconnectDelay << "ms\n";
        m_frameTimer->stop();
        int jitter = QRandomGenerator::global()->bounded(1000);
        m_reconnectTimer->start(m_reconnectDelay + jitter);
        m_reconnectDelay = qMin(m_reconnectDelay * 2, 15000);
    }

    void onReadyRead()
    {
        m_buf.append(m_socket->readAll());
        while (m_buf.size() >= HEADER_SIZE) {
            QDataStream s(m_buf.left(HEADER_SIZE));
            s.setByteOrder(QDataStream::LittleEndian);
            uint16_t magic, msgType;
            uint32_t payloadLen, reserved;
            s >> magic >> msgType >> payloadLen >> reserved;
            if (magic != MAGIC_BYTES) {
                QTextStream(stdout) << "[" << m_hostname << "] bad magic, resyncing\n";
                m_buf.clear();
                m_socket->disconnectFromHost();
                return;
            }
            qint64 total = HEADER_SIZE + qint64(payloadLen);
            if (m_buf.size() < total) return;
            m_buf.remove(0, int(total));

            MsgType t = static_cast<MsgType>(msgType & 0xFF);
            if (t == MsgType::PING) {
                m_socket->write(packet(MsgType::PONG));
            }
            // Other message types (LOCK_SCREEN, MESSAGE, CMD, ...) are
            // intentionally ignored - we just want to look "alive" to the
            // teacher.
        }
    }

    void sendHello()
    {
        QJsonObject o;
        o["hostname"]  = m_hostname;
        o["username"]  = m_username;
        o["os"]        = "Windows 11 (fake)";
        o["screen"]    = "1920x1080";
        o["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);
        m_socket->write(packet(MsgType::HELLO, payload));
    }

    void sendFrame()
    {
        if (m_socket->state() != QAbstractSocket::ConnectedState) return;
        // Mirror real student's adaptive backpressure: skip frame if the
        // socket write buffer is already > 1 MB.
        if (m_socket->bytesToWrite() > 1 * 1024 * 1024) return;

        // Cycle through pre-rendered frames so the teacher decoder actually
        // re-decodes each time (not just blits a cached pixmap).
        const QByteArray& frame = m_fakeFrames[m_frameIdx];
        m_frameIdx = (m_frameIdx + 1) % m_fakeFrames.size();

        // -- Optional bandwidth throttle (simulates slow per-student link) --
        // When --kbps > 0 is set, wait long enough between writes that this
        // student does not exceed the configured bandwidth. Useful to model
        // a phone hotspot or weak WiFi where each student gets ~200 kbps.
        if (m_simulatedKbps > 0) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 minIntervalMs = (qint64(frame.size()) * 8 * 1000)
                                   / (qint64(m_simulatedKbps) * 1000);
            if (m_lastSendMs > 0 && (now - m_lastSendMs) < minIntervalMs) {
                return; // skip this frame to honor bandwidth budget
            }
            m_lastSendMs = now;
        }

        m_socket->write(packet(MsgType::FRAME, frame));
    }

    QByteArray renderRealisticFrame(int variant)
    {
        // Full 960x540 image (matches real student running at scale=0.5 of
        // 1920x1080). JPEG q60 produces ~30-70 KB which is the realistic
        // production frame size.
        const int W = 960, H = 540;
        QImage img(W, H, QImage::Format_RGB32);

        // Gradient background -- gives the JPEG encoder real entropy so
        // the output size is representative (a flat color compresses to
        // 1-2 KB which is unrealistically small).
        QLinearGradient grad(0, 0, W, H);
        grad.setColorAt(0.0, QColor::fromHsv((m_index * 23) % 360, 180, 220));
        grad.setColorAt(1.0, QColor::fromHsv((m_index * 23 + 60) % 360, 180, 120));
        QPainter p(&img);
        p.fillRect(img.rect(), grad);

        // Sprinkle some shapes that change between variants so consecutive
        // frames are different (forces teacher to actually decode).
        QRandomGenerator rng(m_index * 1000 + variant);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 40; ++i) {
            p.setBrush(QColor(rng.bounded(256), rng.bounded(256),
                              rng.bounded(256), 180));
            p.drawEllipse(rng.bounded(W), rng.bounded(H),
                          rng.bounded(20, 80), rng.bounded(20, 80));
        }

        // Big index number so we can identify this fake student in the
        // teacher UI.
        p.setPen(Qt::white);
        QFont f("Arial", 180, QFont::Bold);
        p.setFont(f);
        p.drawText(img.rect(), Qt::AlignCenter,
                   QStringLiteral("%1").arg(m_index, 2, 10, QChar('0')));
        p.end();

        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "JPEG", 60);
        return bytes;
    }

private:
    int        m_index;
    QString    m_host;
    uint16_t   m_port;
    int        m_frameInterval;
    int        m_simulatedKbps;     // 0 = unlimited, otherwise per-student bandwidth budget
    QString    m_hostname;
    QString    m_username;
    QTcpSocket* m_socket;
    QTimer*    m_frameTimer;
    QTimer*    m_reconnectTimer;
    int        m_reconnectDelay = 1000;
    QByteArray m_buf;
    QList<QByteArray> m_fakeFrames;
    int        m_frameIdx = 0;
    qint64     m_lastSendMs = 0;
};

#include "stress_test.moc"

int main(int argc, char* argv[])
{
    // Use QGuiApplication (not QCoreApplication) because we use QImage +
    // QPainter + QFont to render fake thumbnails. Those need a QGuiApp
    // because QFontDatabase pulls fonts from the OS via the GUI plugin.
    QGuiApplication app(argc, argv);
    app.setApplicationName("Simanta-StressTest");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Simanta stress tester - spawns N fake student connections.");
    parser.addHelpOption();

    QCommandLineOption countOpt(
        {"n", "count"},
        "Number of fake students to spawn", "count", "20");
    QCommandLineOption hostOpt(
        {"H", "host"},
        "Teacher host IP (default: 127.0.0.1)", "host", "127.0.0.1");
    QCommandLineOption portOpt(
        {"p", "port"},
        "Teacher TCP port (default: 5400)", "port", "5400");
    QCommandLineOption intervalOpt(
        {"i", "interval"},
        "Frame send interval in ms per student (default: 1500)", "ms", "1500");
    QCommandLineOption kbpsOpt(
        {"k", "kbps"},
        "Per-student bandwidth budget in kbps (0 = unlimited). "
        "Use 200-500 to simulate phone hotspot per-student bandwidth, "
        "1500-3000 to simulate WiFi school per-student. (default: 0)",
        "kbps", "0");
    parser.addOption(countOpt);
    parser.addOption(hostOpt);
    parser.addOption(portOpt);
    parser.addOption(intervalOpt);
    parser.addOption(kbpsOpt);
    parser.process(app);

    int count   = parser.value(countOpt).toInt();
    QString host = parser.value(hostOpt);
    uint16_t port = static_cast<uint16_t>(parser.value(portOpt).toUShort());
    int interval = parser.value(intervalOpt).toInt();
    int kbps     = parser.value(kbpsOpt).toInt();

    if (count <= 0 || count > 200) {
        QTextStream(stderr) << "count must be between 1 and 200\n";
        return 1;
    }

    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "  Simanta Stress Test\n";
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "  Spawning " << count << " fake students\n";
    QTextStream(stdout) << "  Target:   " << host << ":" << port << "\n";
    QTextStream(stdout) << "  Frame:    every " << interval << " ms\n";
    QTextStream(stdout) << "  Frame size: ~30-70 KB (realistic, 960x540 JPEG q60)\n";
    if (kbps > 0) {
        QTextStream(stdout) << "  Per-student bandwidth limit: " << kbps << " kbps\n";
    } else {
        QTextStream(stdout) << "  Per-student bandwidth limit: unlimited\n";
    }
    QTextStream(stdout) << "  Stop:     Ctrl+C\n";
    QTextStream(stdout) << "============================================\n\n";

    QList<FakeStudent*> students;
    for (int i = 1; i <= count; ++i) {
        auto* s = new FakeStudent(i, host, port, interval, kbps, &app);
        students.append(s);
        s->start();
    }

    return app.exec();
}
