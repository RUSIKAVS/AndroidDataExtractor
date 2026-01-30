#include "AndroidPartition.hpp"
#include <QStorageInfo>

QString AndroidPartition::humanReadableSize() const
{
    qint64 bytes = size;
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;

    while (bytes >= 1024 && unitIndex < units.size() - 1) {
        bytes /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
}
