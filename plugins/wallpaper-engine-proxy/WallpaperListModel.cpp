#include "WallpaperListModel.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

WallpaperListModel::WallpaperListModel(QObject* parent)
    : QAbstractListModel(parent) {
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString&) { refresh(); });
}

int WallpaperListModel::rowCount(const QModelIndex&) const {
    return m_entries.size();
}

QVariant WallpaperListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};

    const auto& entry = m_entries[index.row()];
    switch (role) {
    case NameRole: return entry.name;
    case PathRole: return entry.path;
    case ThumbnailRole: return entry.thumbnail;
    case TypeRole: return entry.type;
    case SourceRole: return entry.source;
    default: return {};
    }
}

QHash<int, QByteArray> WallpaperListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {ThumbnailRole, "thumbnail"},
        {TypeRole, "type"},
        {SourceRole, "source"}
    };
}

void WallpaperListModel::scanSteamWorkshop(const QString& path) {
    scanDirectory(path, "steam");
}

void WallpaperListModel::scanLocalDirectory(const QString& path) {
    scanDirectory(path, "local");
}

void WallpaperListModel::refresh() {
    beginResetModel();
    clearEntries();
    for (const auto& path : m_searchPaths) {
        scanDirectory(path, QFileInfo(path).fileName());
    }
    endResetModel();
    emit countChanged();
}

void WallpaperListModel::scanDirectory(const QString& path, const QString& source) {
    QDir dir(path);
    if (!dir.exists()) return;

    if (!m_searchPaths.contains(path)) {
        m_searchPaths.append(path);
        m_watcher.addPath(path);
    }

    // Steam workshop: each subdirectory is a wallpaper with project.json
    for (const auto& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString projectJson = dir.filePath(entry + "/project.json");
        if (QFileInfo::exists(projectJson)) {
            WallpaperEntry we;
            we.path = projectJson;
            we.source = source;

            // Read project.json for name and type
            QFile f(projectJson);
            if (f.open(QIODevice::ReadOnly)) {
                auto doc = QJsonDocument::fromJson(f.readAll());
                auto obj = doc.object();
                we.name = obj["title"].toString(entry);
                we.type = obj["type"].toString("scene");
            } else {
                we.name = entry;
                we.type = "scene";
            }

            // Look for thumbnail
            QString thumbDir = dir.filePath(entry);
            for (const auto& thumb : {"preview.jpg", "preview.gif", "thumb.jpg"}) {
                QString thumbPath = thumbDir + "/" + thumb;
                if (QFileInfo::exists(thumbPath)) {
                    we.thumbnail = QUrl::fromLocalFile(thumbPath).toString();
                    break;
                }
            }

            m_entries.push_back(we);
        }
    }

    // Local directory: scan for image/video files directly
    QStringList imageExts = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.gif", "*.webp"};
    QStringList videoExts = {"*.mp4", "*.webm", "*.mkv", "*.avi"};
    QStringList htmlExts = {"*.html", "*.htm"};

    for (const auto& filter : {imageExts, videoExts, htmlExts}) {
        for (const auto& file : dir.entryList(filter, QDir::Files)) {
            WallpaperEntry we;
            we.name = QFileInfo(file).baseName();
            we.path = dir.filePath(file);
            we.source = source;
            we.thumbnail = QUrl::fromLocalFile(we.path).toString();

            if (imageExts.contains("*" + QFileInfo(file).suffix().toLower())) {
                we.type = "image";
            } else if (videoExts.contains("*" + QFileInfo(file).suffix().toLower())) {
                we.type = "video";
            } else {
                we.type = "web";
            }

            m_entries.push_back(we);
        }
    }
}

void WallpaperListModel::clearEntries() {
    m_entries.clear();
}
