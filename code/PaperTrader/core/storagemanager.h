#pragma once
#include <QObject>
#include <QStringList>
#include <QJsonObject>

class StorageManager : public QObject {
    Q_OBJECT
public:
    explicit StorageManager(QObject *parent = nullptr);

    QStringList loadWatchlist() const;
    bool saveWatchlist(const QStringList &symbols);

    QJsonObject loadSettings() const;
    bool saveSettings(const QJsonObject &settings);

    QString storageRoot() const { return m_storageRoot; }

private:
    QString ensureStorageDir() const;
    QString filePath(const QString &name) const;

    QString m_storageRoot;
};
