#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include "ElaMessageBar.h"
#include "ElaText.h"

import GPPVersion;
import Tool;

namespace
{
    constexpr int CheckTimeoutMs = 10000;
    constexpr int DownloadStallTimeoutMs = 15000;
    constexpr qint64 HashBufferSize = 1024 * 1024;

    QString networkErrorText(const QNetworkReply* reply)
    {
        return reply ? reply->errorString() : QString();
    }
}

UpdateChecker::UpdateChecker(toml::ordered_value& globalConfig, ElaText* statusText, QObject* parent)
    : QObject(parent), m_statusText(statusText), m_globalConfig(globalConfig)
{
    m_checkManager = new QNetworkAccessManager(this);
    connect(m_checkManager, &QNetworkAccessManager::finished, this, &UpdateChecker::onReplyFinished);

    m_downloadManager = new QNetworkAccessManager(this);
    connect(m_downloadManager, &QNetworkAccessManager::finished, this, &UpdateChecker::onDownloadFinished);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/GPPGUI/Resource/images/julixian_s.ico"));
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [=]()
        {
            Q_EMIT applyUpdateAndRestartSignal();
        });

    m_checkTimer = new QTimer(this);
    m_checkTimer->setSingleShot(true);
    connect(m_checkTimer, &QTimer::timeout, this, &UpdateChecker::onReplyTimeout);

    m_downloadTimer = new QTimer(this);
    m_downloadTimer->setSingleShot(true);
    connect(m_downloadTimer, &QTimer::timeout, this, &UpdateChecker::onDownloadTimeout);
}

UpdateChecker::~UpdateChecker()
{
    if (m_checkReply) {
        m_checkReply->abort();
    }
    if (m_downloadReply) {
        m_downloadReply->abort();
    }
    resetDownloadFile();
}

void UpdateChecker::check(bool downloadIfAvailableAfterChecking)
{
    if (m_state == UpdateState::Downloading) {
        ElaMessageBar::information(ElaMessageBarType::Top, tr("请稍候"), tr("正在下载更新包..."), 5000);
        return;
    }

    if (m_state == UpdateState::ReadyToInstall) {
        if (hasValidLocalPackage(m_latestRelease.asset)) {
            showReadyToInstallNotification(tr("下载已完成"), tr("点击以关闭程序并安装更新"));
            return;
        }
        else {
            setState(UpdateState::UpdateAvailable);
        }
    }

    if (m_state == UpdateState::Checking) {
        m_pendingDownloadRequest = m_pendingDownloadRequest || downloadIfAvailableAfterChecking;
        ElaMessageBar::information(ElaMessageBarType::Top, tr("请稍候"), tr("正在检查更新..."), 5000);
        return;
    }

    if (m_state == UpdateState::UpdateAvailable && downloadIfAvailableAfterChecking) {
        m_pendingDownloadRequest = true;
        maybeStartDownload(true);
        return;
    }

    startCheck(downloadIfAvailableAfterChecking);
}

bool UpdateChecker::shouldDownloadButtonEnabled() const
{
	if (m_state == UpdateState::UpdateAvailable) {
        return true;
	}
    return false;
}

bool UpdateChecker::shouldStartUpdater() const
{
    if (m_state == UpdateState::ReadyToInstall && hasValidLocalPackage(m_latestRelease.asset)) {
        return true;
    }
    return false;
}

void UpdateChecker::setState(UpdateState state)
{
    m_state = state;
}

void UpdateChecker::startCheck(bool requestDownload)
{
    m_pendingDownloadRequest = requestDownload;
    setState(UpdateState::Checking);
    if (m_statusText) {
        m_statusText->setText(tr("正在检查更新..."));
    }

    const QUrl url("https://api.github.com/repos/" + m_repoOwner + "/" + m_repoName + "/releases/latest");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "GalTranslPP-GUI");

    m_checkReply = m_checkManager->get(request);
    m_checkTimer->start(CheckTimeoutMs);
}

void UpdateChecker::onReplyTimeout()
{
    if (m_checkReply && m_checkReply->isRunning()) {
        m_checkReply->abort();
    }
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    if (reply != m_checkReply) {
        reply->deleteLater();
        return;
    }

    m_checkTimer->stop();
    m_checkReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = networkErrorText(reply);
        reply->deleteLater();
        failCheck(error.isEmpty() ? tr("网络连接失败，请检查网络设置。") : error);
        return;
    }

    LatestReleaseInfo releaseInfo;
    QString errorMessage;
    if (!parseLatestRelease(reply->readAll(), releaseInfo, &errorMessage)) {
        reply->deleteLater();
        failCheck(errorMessage);
        return;
    }
    reply->deleteLater();

    m_latestRelease = releaseInfo;
    if (!m_latestRelease.hasNewVersion) {
        setState(UpdateState::Idle);
        if (m_statusText) {
            m_statusText->setText(tr("当前已是最新版本"));
        }
        ElaMessageBar::success(ElaMessageBarType::Top, tr("版本检测"), tr("当前已是最新的版本"), 5000);
        Q_EMIT checkCompleteSignal(false);
        return;
    }

    setState(UpdateState::UpdateAvailable);
    if (m_statusText) {
        m_statusText->setText(tr("检测到新版本！"));
    }
    if (!m_pendingDownloadRequest) {
        ElaMessageBar::information(ElaMessageBarType::Top, tr("检测到新版本"),
            tr("最新版本: %1").arg(m_latestRelease.version), 5000);
    }

    maybeStartDownload(m_pendingDownloadRequest || toml::find_or(m_globalConfig, "autoDownloadUpdate", true));
    if (m_state != UpdateState::ReadyToInstall) {
        Q_EMIT checkCompleteSignal(true);
    }
}

bool UpdateChecker::parseLatestRelease(const QByteArray& responseData, LatestReleaseInfo& info, QString* errorMessage) const
{
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isObject()) {
        if (errorMessage) {
            *errorMessage = tr("获取更新信息失败。");
        }
        return false;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    info.version = jsonObj["tag_name"].toString();
    if (info.version.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("更新信息中缺少版本号。");
        }
        return false;
    }

    const int versionCompare = compareVersion(info.version.toStdString(), GPPVERSION);
    if (versionCompare == -2) {
        if (errorMessage) {
            *errorMessage = tr("版本号解析失败。");
        }
        return false;
    }
    info.hasNewVersion = versionCompare > 0;
    info.isCompatible = versionCompare != 2;
    info.asset = findGuiCoreAsset(jsonObj["assets"].toArray()).value_or(UpdateAssetInfo{});

    if (info.hasNewVersion && !info.asset.downloadUrl.isValid()) {
        if (errorMessage) {
            *errorMessage = tr("发布页中未找到 GUICORE.7z 更新包。");
        }
        return false;
    }

    return true;
}

std::optional<UpdateChecker::UpdateAssetInfo> UpdateChecker::findGuiCoreAsset(const QJsonArray& assets) const
{
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset["name"].toString() != m_assetName) {
            continue;
        }

        UpdateAssetInfo info;
        info.name = asset["name"].toString();
        info.downloadUrl = QUrl(asset["browser_download_url"].toString());
        info.sha256 = normalizeSha256(asset["digest"].toString());
        return info;
    }

    return std::nullopt;
}

QString UpdateChecker::normalizeSha256(const QString& digest)
{
    QString normalized = digest.trimmed();
    if (normalized.startsWith("sha256:", Qt::CaseInsensitive)) {
        normalized = normalized.mid(QStringLiteral("sha256:").size());
    }
    return normalized.toLower();
}

QString UpdateChecker::calculateFileSha256(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(HashBufferSize));
    }
    return hash.result().toHex();
}

bool UpdateChecker::hasValidLocalPackage(const UpdateAssetInfo& asset) const
{
    if (asset.sha256.isEmpty() || !QFile::exists(m_packagePath)) {
        return false;
    }

    return calculateFileSha256(m_packagePath) == asset.sha256;
}

void UpdateChecker::maybeStartDownload(bool requestDownload)
{
    if (!m_latestRelease.hasNewVersion) {
        return;
    }

    if (!requestDownload) {
        return;
    }

    if (!m_latestRelease.isCompatible && !m_pendingDownloadRequest) {
        ElaMessageBar::warning(
            ElaMessageBarType::Top,
            tr("不兼容更新"),
            tr("最新版含有不兼容当前版本的内容，请确认 GitHub 发布页更新日志后再下载。"),
            5000);
        return;
    }

    if (hasValidLocalPackage(m_latestRelease.asset)) {
        finishReadyToInstall(tr("更新下载已完成"));
        return;
    }

    startDownload(m_latestRelease.asset);
}

void UpdateChecker::startDownload(const UpdateAssetInfo& asset)
{
    if (m_state == UpdateState::Downloading) {
        return;
    }

    QFile::remove(m_tempPackagePath);
    m_downloadFile = new QFile(m_tempPackagePath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        resetDownloadFile();
        failDownload(tr("无法创建更新包临时文件。"));
        return;
    }

    QNetworkRequest request(asset.downloadUrl);
    request.setRawHeader("User-Agent", "GalTranslPP-GUI");
    m_downloadReply = m_downloadManager->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]()
        {
            if (m_downloadFile && m_downloadReply) {
                m_downloadFile->write(m_downloadReply->readAll());
            }
        });
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &UpdateChecker::onDownloadProgress);

    setState(UpdateState::Downloading);
    if (m_statusText) {
        m_statusText->setText(tr("下载更新..."));
    }
    ElaMessageBar::information(ElaMessageBarType::Top, tr("下载更新"), tr("正在下载更新包..."), 5000);
    m_downloadTimer->start(DownloadStallTimeoutMs);
}

void UpdateChecker::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_statusText) {
        const QString totalText = bytesTotal > 0 ? formatBytes(bytesTotal) : tr("未知大小");
        m_statusText->setText(tr("下载更新... %1/%2").arg(formatBytes(bytesReceived), totalText));
    }
    m_downloadTimer->start(DownloadStallTimeoutMs);
}

QString UpdateChecker::formatBytes(qint64 bytes)
{
    constexpr double Mib = 1024.0 * 1024.0;
    return QString::number((double)bytes / Mib, 'f', 1) + " MB";
}

void UpdateChecker::onDownloadTimeout()
{
    if (m_downloadReply && m_downloadReply->isRunning()) {
        m_downloadReply->abort();
    }
}

void UpdateChecker::onDownloadFinished(QNetworkReply* reply)
{
    if (reply != m_downloadReply) {
        reply->deleteLater();
        return;
    }

    m_downloadTimer->stop();
    m_downloadReply = nullptr;

    if (m_downloadFile) {
        m_downloadFile->write(reply->readAll());
        m_downloadFile->close();
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = networkErrorText(reply);
        reply->deleteLater();
        resetDownloadFile();
        QFile::remove(m_tempPackagePath);
        failDownload(error.isEmpty() ? tr("网络连接失败，请检查网络设置。") : error);
        return;
    }
    reply->deleteLater();

    resetDownloadFile();
    if (!m_latestRelease.asset.sha256.isEmpty()
        && calculateFileSha256(m_tempPackagePath) != m_latestRelease.asset.sha256) {
        QFile::remove(m_tempPackagePath);
        failDownload(tr("更新包校验失败，请重新下载。"));
        return;
    }

    QFile::remove(m_packagePath);
    if (!QFile::rename(m_tempPackagePath, m_packagePath)) {
        QFile::remove(m_tempPackagePath);
        failDownload(tr("无法保存更新包。"));
        return;
    }

    finishReadyToInstall(tr("更新下载成功"));
}

void UpdateChecker::finishReadyToInstall(const QString& statusMessage)
{
    setState(UpdateState::ReadyToInstall);
    m_downloadSuccess = true;
    if (m_statusText) {
        m_statusText->setText(statusMessage);
    }

    ElaMessageBar::success(ElaMessageBarType::Top, tr("更新下载成功"), tr("将在程序关闭后自动安装更新"), 5000);
    showReadyToInstallNotification(tr("下载完成"), tr("点击以关闭程序并安装更新"));
    Q_EMIT checkCompleteSignal(true);
}

void UpdateChecker::showReadyToInstallNotification(const QString& title, const QString& message)
{
    if (!m_trayIcon) {
        return;
    }

    m_trayIcon->show();
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 5000);
    QTimer::singleShot(5000, m_trayIcon, &QSystemTrayIcon::hide);
}

void UpdateChecker::failCheck(const QString& message)
{
    setState(UpdateState::Idle);
    if (m_statusText) {
        m_statusText->setText(tr("更新检测失败"));
    }
    ElaMessageBar::warning(ElaMessageBarType::Top, tr("更新检测失败"), message, 5000);
    Q_EMIT checkCompleteSignal(false);
}

void UpdateChecker::failDownload(const QString& message)
{
    setState(m_latestRelease.hasNewVersion ? UpdateState::UpdateAvailable : UpdateState::Idle);
    if (m_statusText) {
        m_statusText->setText(tr("更新下载失败"));
    }
    ElaMessageBar::warning(ElaMessageBarType::Top, tr("更新下载失败"), message, 5000);
    Q_EMIT checkCompleteSignal(m_latestRelease.hasNewVersion);
}

void UpdateChecker::resetDownloadFile()
{
    if (!m_downloadFile) {
        return;
    }

    m_downloadFile->close();
    m_downloadFile->deleteLater();
    m_downloadFile = nullptr;
}
