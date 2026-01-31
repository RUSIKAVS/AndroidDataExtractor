#pragma once

#include <functional>

/**
 * @brief Ext4ProgressCallback
 *
 * Используется для обновления GUI прогресса.
 *
 * value: 0..100
 * message: текущий этап
 */
using Ext4ProgressCallback =
    std::function<void(int value, const QString& message)>;
