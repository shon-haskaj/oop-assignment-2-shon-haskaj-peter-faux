#include "storagemanager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

static const char *kWatchlistFile = "watchlist.json";
static const char *kSettingsFile  = "settings.json";

StorageManager::StorageManager(QObject *parent)
    : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!base.isEmpty()) {
        m_storageRoot = base + "/PaperTrader";
    } else {
        m_storageRoot = QDir::homePath() + "/.papertrader";
    }
}

QString StorageManager::ensureStorageDir() const
{
    QDir dir(m_storageRoot);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

QString StorageManager::filePath(const QString &name) const
{
    return ensureStorageDir() + QLatin1Char('/') + name;
}

QStringList StorageManager::loadWatchlist() const
{
    QFile f(filePath(kWatchlistFile));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QStringList result;
    if (doc.isArray()) {
        for (const QJsonValue &val : doc.array()) {
            if (val.isString()) {
                result.append(val.toString());
            }
        }
    }
    return result;
}

bool StorageManager::saveWatchlist(const QStringList &symbols)
{
    QFile f(filePath(kWatchlistFile));
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonArray arr;
    for (const QString &sym : symbols) {
        arr.append(sym);
    }
    const QJsonDocument doc(arr);
    f.write(doc.toJson(QJsonDocument::Compact));
    return true;
}

QJsonObject StorageManager::loadSettings() const
{
    QFile f(filePath(kSettingsFile));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject()) {
        return doc.object();
    }
    return {};
}

bool StorageManager::saveSettings(const QJsonObject &settings)
{
    QFile f(filePath(kSettingsFile));
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QJsonDocument doc(settings);
    f.write(doc.toJson(QJsonDocument::Compact));
    return true;
}
