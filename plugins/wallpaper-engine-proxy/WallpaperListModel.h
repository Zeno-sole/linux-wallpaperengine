#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QString>
#include <QUrl>
#include <vector>

struct WallpaperEntry {
    QString name;
    QString path;
    QString thumbnail;
    QString type;  // "scene", "video", "web", "image"
    QString source; // "steam" or "local"
};

class WallpaperListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ThumbnailRole,
        TypeRole,
        SourceRole
    };

    explicit WallpaperListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_entries.size(); }

    Q_INVOKABLE void scanSteamWorkshop(const QString& path);
    Q_INVOKABLE void scanLocalDirectory(const QString& path);
    Q_INVOKABLE void refresh();

signals:
    void countChanged();

private:
    void scanDirectory(const QString& path, const QString& source);
    void clearEntries();

    std::vector<WallpaperEntry> m_entries;
    QFileSystemWatcher m_watcher;
    QStringList m_searchPaths;
};
