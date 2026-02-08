#include "LogBuffer.h"

LogBuffer::LogBuffer(QObject *parent) : QObject(parent) {}

void LogBuffer::append(const QString &line) {
    m_lines.append(line);
    while (m_lines.size() > maxLines)
        m_lines.removeFirst();
    emit logAppended();
}

void LogBuffer::clear() {
    if (m_lines.isEmpty()) return;
    m_lines.clear();
    emit logAppended();
}
