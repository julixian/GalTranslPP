#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <optional>

#include <QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QSystemTrayIcon>
#include <toml.hpp>

class ElaText;
class QFile;

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(toml::ordered_value& globalConfig, ElaText* statusText, QObject* parent = nullptr);
    ~UpdateChecker() override;
    void check(bool downloadIfAvailableAfterChecking = false);
    bool shouldDownloadButtonEnabled() const;
    bool shouldStartUpdater() const;

Q_SIGNALS:
    void checkCompleteSignal(bool hasNewVersion);
    void applyUpdateAndRestartSignal();

private Q_SLOTS:
    void onReplyFinished(QNetworkReply* reply);
    void onReplyTimeout();
    void onDownloadFinished(QNetworkReply* reply);
    void onDownloadTimeout();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    struct UpdateAssetInfo {
        QString name;
        QUrl downloadUrl;
        QString sha256;
    };

    struct LatestReleaseInfo {
        QString version;
        bool hasNewVersion{};
        bool isCompatible = true;
        UpdateAssetInfo asset;
    };

    enum class UpdateState {
        Idle,
        Checking,
        UpdateAvailable,
        Downloading,
        ReadyToInstall,
    };

    static QString calculateFileSha256(const QString& filePath);
    static QString normalizeSha256(const QString& digest);
    static QString formatBytes(qint64 bytes);

    void setState(UpdateState state);
    void startCheck(bool requestDownload);
    bool parseLatestRelease(const QByteArray& responseData, LatestReleaseInfo& info, QString* errorMessage) const;
    std::optional<UpdateAssetInfo> findGuiCoreAsset(const QJsonArray& assets) const;
    bool hasValidLocalPackage(const UpdateAssetInfo& asset) const;
    void maybeStartDownload(bool requestDownload);
    void startDownload(const UpdateAssetInfo& asset);
    void finishReadyToInstall(const QString& statusMessage);
    void showReadyToInstallNotification(const QString& title, const QString& message);
    void failCheck(const QString& message);
    void failDownload(const QString& message);
    void resetDownloadFile();

    UpdateState m_state = UpdateState::Idle;
    LatestReleaseInfo m_latestRelease;
    bool m_pendingDownloadRequest{};
    bool m_downloadSuccess{};
    ElaText* m_statusText = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;

    QNetworkAccessManager* m_checkManager = nullptr;
    QNetworkReply* m_checkReply = nullptr;
    QTimer* m_checkTimer = nullptr;
    QNetworkAccessManager* m_downloadManager = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QTimer* m_downloadTimer = nullptr;
    QFile* m_downloadFile = nullptr;

    toml::ordered_value& m_globalConfig;
    const QString m_repoOwner = "julixian";
    const QString m_repoName = "GalTranslPP";
    const QString m_assetName = "GUICORE.7z";
    const QString m_packagePath = "GUICORE.7z";
    const QString m_tempPackagePath = "GUICORE.7z.download";
};

#endif // UPDATECHECKER_H
