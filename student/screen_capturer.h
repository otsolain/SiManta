#pragma once

#include <QObject>
#include <QByteArray>

namespace LabMonitor {

class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturer(QObject* parent = nullptr);
    ~ScreenCapturer() override;
    void setQuality(int quality);
    void setScale(double scale);

    int quality() const { return m_quality; }
    double scale() const { return m_scale; }
    QByteArray capture();
    static bool isAvailable() { return true; }

signals:
    void captureError(const QString& error);

private:
    int    m_quality = 60;
    double m_scale   = 0.5;
};

}
