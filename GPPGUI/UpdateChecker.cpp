#include "UpdateChecker.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDesktopServices>

#include "ElaMessageBar.h"

import Tool;

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::check()
{
    // GitHub API for the latest release
    QUrl url("https://api.github.com/repos/" + m_repoOwner + "/" + m_repoName + "/releases/latest");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_networkManager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        ElaMessageBar::warning(ElaMessageBarType::TopLeft, "更新检测失败", "网络连接失败，请检查网络设置。", 5000);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

    if (!jsonDoc.isObject()) {
        ElaMessageBar::warning(ElaMessageBarType::TopLeft, "更新检测失败", "获取更新信息失败。", 5000);
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    std::string latestVersion = jsonObj["tag_name"].toString().toStdString();
    bool hasNewVersion = isVersionGreaterThan(latestVersion, GPPVERSION);

    if (hasNewVersion) {
        ElaMessageBar::information(ElaMessageBarType::TopLeft, "检测到新版本", "最新版本: " + QString::fromStdString(GPPVERSION), 5000);
    }
    else {
        ElaMessageBar::success(ElaMessageBarType::TopLeft, "版本检测", "当前已是最新的版本", 5000);
    }
    reply->deleteLater();
}

bool UpdateChecker::isVersionGreaterThan(const std::string& v1, const std::string& v2)
{
    std::string v1s = v1.find_last_of("v") == std::string::npos ? v1 : v1.substr(v1.find_last_of("v") + 1);
    std::string v2s = v2.find_last_of("v") == std::string::npos ? v2 : v2.substr(v2.find_last_of("v") + 1);

    std::vector<std::string> v1parts = splitString(v1s, '.');
    std::vector<std::string> v2parts = splitString(v2s, '.');

    size_t len = std::max(v1parts.size(), v2parts.size());

    for (size_t i = 0; i < len; i++) {
        int v1part = i < v1parts.size() ? std::stoi(v1parts[i]) : 0;
        int v2part = i < v2parts.size() ? std::stoi(v2parts[i]) : 0;

        if (v1part > v2part) {
            return true;
        }
        else if (v1part < v2part) {
            return false;
        }
    }

    return false;
}
