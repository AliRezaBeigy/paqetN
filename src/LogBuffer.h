#pragma once

#include <QObject>
#include <QStringList>

class LogBuffer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fullText READ fullText NOTIFY logAppended)
public:
    explicit LogBuffer(QObject *parent = nullptr);
    static constexpr int maxLines = 2000;

    QString fullText() const { return m_lines.join(QLatin1Char('\n')); }
    void append(const QString &line);
    void clear();

signals:
    void logAppended();

private:
    QStringList m_lines;
};
