#ifndef ZIPEXTRACTOR_H
#define ZIPEXTRACTOR_H

#include <QString>
#include <QDir>
#include <QDebug>
#include <JlCompress.h>

class ZipExtractor
{
public:
    static bool extractFile(const QString &zipPath, const QString &destDir, QString &errorMsg)
    {
        qDebug() << "[ZipExtractor] Extracting:" << zipPath << "to" << destDir;

        // Ensure destination directory exists
        QDir dir(destDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Use JlCompress to extract all files
        QStringList extractedFiles = JlCompress::extractDir(zipPath, destDir);

        if (extractedFiles.isEmpty()) {
            errorMsg = "Failed to extract ZIP file. The file may be corrupted or in an unsupported format.";
            qWarning() << "[ZipExtractor]" << errorMsg;
            return false;
        }

        qDebug() << "[ZipExtractor] Successfully extracted" << extractedFiles.size() << "files";

        // Filter out example files if needed
        int skippedCount = 0;
        for (const QString &file : extractedFiles) {
            if (file.contains("/example/", Qt::CaseInsensitive) ||
                file.contains("\\example\\", Qt::CaseInsensitive)) {
                QFile::remove(file);
                skippedCount++;
                qDebug() << "[ZipExtractor] Removed example file:" << file;
            }
        }

        if (skippedCount > 0) {
            qDebug() << "[ZipExtractor] Removed" << skippedCount << "example files";
        }

        return true;
    }
};

#endif // ZIPEXTRACTOR_H
