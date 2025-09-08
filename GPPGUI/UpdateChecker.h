#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <string>

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    void check();

signals:
    // 信号：检查完成 (是否有新版, 最新版本号)
    void checkFinished(bool hasNewVersion, QString latestVersion);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:

    bool isVersionGreaterThan(const std::string& version1, const std::string& version2);

    QNetworkAccessManager* m_networkManager;

    const QString m_repoOwner = "julixian";
    const QString m_repoName = "GalTranslPP";
};

#endif // UPDATECHECKER_H
