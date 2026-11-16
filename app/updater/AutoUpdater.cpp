#include "AutoUpdater.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>

#include "platform/PlatformIntegration.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#endif

namespace {

struct HttpResponse {
  int statusCode{0};
  QByteArray body;
  QString error;
};

#ifdef Q_OS_WIN
struct WinHttpHandleDeleter {
  void operator()(HINTERNET h) const {
    if (h) WinHttpCloseHandle(h);
  }
};
using ScopedHINTERNET = std::unique_ptr<void, WinHttpHandleDeleter>;

HttpResponse httpGet(const QString &urlString) {
  HttpResponse resp;

  ScopedHINTERNET hSession(WinHttpOpen(
      L"VYNX-QR-Updater",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0));
  if (!hSession) {
    resp.error = QStringLiteral("WinHttpOpen failed (code %1)").arg(GetLastError());
    return resp;
  }

  URL_COMPONENTS urlComp{};
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwHostNameLength = static_cast<DWORD>(-1);
  urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
  urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);
  urlComp.dwSchemeLength = static_cast<DWORD>(-1);

  std::wstring wUrl = urlString.toStdWString();
  if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.length()), 0, &urlComp)) {
    resp.error = QStringLiteral("WinHttpCrackUrl failed (code %1)").arg(GetLastError());
    return resp;
  }

  std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
  std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);
  if (path.empty()) path = L"/";

  ScopedHINTERNET hConnect(WinHttpConnect(hSession.get(), host.c_str(), urlComp.nPort, 0));
  if (!hConnect) {
    resp.error = QStringLiteral("WinHttpConnect failed (code %1)").arg(GetLastError());
    return resp;
  }

  DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  ScopedHINTERNET hRequest(WinHttpOpenRequest(
      hConnect.get(),
      L"GET",
      path.c_str(),
      NULL,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      flags));
  if (!hRequest) {
    resp.error = QStringLiteral("WinHttpOpenRequest failed (code %1)").arg(GetLastError());
    return resp;
  }

  DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hRequest.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

  LPCWSTR headers = L"Accept: application/vnd.github.v3+json\r\n";
  if (!WinHttpSendRequest(hRequest.get(), headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    resp.error = QStringLiteral("WinHttpSendRequest failed (code %1)").arg(GetLastError());
    return resp;
  }

  if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
    resp.error = QStringLiteral("WinHttpReceiveResponse failed (code %1)").arg(GetLastError());
    return resp;
  }

  DWORD statusCode = 0;
  DWORD size = sizeof(statusCode);
  WinHttpQueryHeaders(
      hRequest.get(),
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &statusCode,
      &size,
      WINHTTP_NO_HEADER_INDEX);
  resp.statusCode = static_cast<int>(statusCode);

  if (resp.statusCode != 200) {
    resp.error = QStringLiteral("HTTP status: %1").arg(resp.statusCode);
    return resp;
  }

  DWORD bytesAvailable = 0;
  while (WinHttpQueryDataAvailable(hRequest.get(), &bytesAvailable) && bytesAvailable > 0) {
    QByteArray chunk;
    chunk.resize(static_cast<qsizetype>(bytesAvailable));
    DWORD bytesRead = 0;
    if (WinHttpReadData(hRequest.get(), chunk.data(), bytesAvailable, &bytesRead)) {
      chunk.resize(static_cast<qsizetype>(bytesRead));
      resp.body.append(chunk);
    } else {
      break;
    }
  }

  return resp;
}

bool downloadFile(const QString &urlString, const QString &destPath,
                  const std::function<void(qint64, qint64)> &progressCallback,
                  QString &errorOut) {
  ScopedHINTERNET hSession(WinHttpOpen(
      L"VYNX-QR-Updater",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0));
  if (!hSession) {
    errorOut = QStringLiteral("WinHttpOpen failed (code %1)").arg(GetLastError());
    return false;
  }

  URL_COMPONENTS urlComp{};
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwHostNameLength = static_cast<DWORD>(-1);
  urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
  urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);
  urlComp.dwSchemeLength = static_cast<DWORD>(-1);

  std::wstring wUrl = urlString.toStdWString();
  if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.length()), 0, &urlComp)) {
    errorOut = QStringLiteral("WinHttpCrackUrl failed (code %1)").arg(GetLastError());
    return false;
  }

  std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
  std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);
  if (path.empty()) path = L"/";

  ScopedHINTERNET hConnect(WinHttpConnect(hSession.get(), host.c_str(), urlComp.nPort, 0));
  if (!hConnect) {
    errorOut = QStringLiteral("WinHttpConnect failed (code %1)").arg(GetLastError());
    return false;
  }

  DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  ScopedHINTERNET hRequest(WinHttpOpenRequest(
      hConnect.get(),
      L"GET",
      path.c_str(),
      NULL,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      flags));
  if (!hRequest) {
    errorOut = QStringLiteral("WinHttpOpenRequest failed (code %1)").arg(GetLastError());
    return false;
  }

  DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hRequest.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

  LPCWSTR headers = L"Accept: application/octet-stream\r\n";
  if (!WinHttpSendRequest(hRequest.get(), headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    errorOut = QStringLiteral("WinHttpSendRequest failed (code %1)").arg(GetLastError());
    return false;
  }

  if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
    errorOut = QStringLiteral("WinHttpReceiveResponse failed (code %1)").arg(GetLastError());
    return false;
  }

  DWORD statusCode = 0;
  DWORD size = sizeof(statusCode);
  WinHttpQueryHeaders(
      hRequest.get(),
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &statusCode,
      &size,
      WINHTTP_NO_HEADER_INDEX);
  if (statusCode != 200) {
    errorOut = QStringLiteral("Download failed with HTTP status %1").arg(statusCode);
    return false;
  }

  DWORD contentLength = 0;
  DWORD clSize = sizeof(contentLength);
  bool hasCl = WinHttpQueryHeaders(
      hRequest.get(),
      WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &contentLength,
      &clSize,
      WINHTTP_NO_HEADER_INDEX);
  qint64 totalBytes = hasCl ? static_cast<qint64>(contentLength) : -1;

  QFile file(destPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    errorOut = QStringLiteral("Cannot open destination file %1: %2").arg(destPath, file.errorString());
    return false;
  }

  qint64 receivedBytes = 0;
  QByteArray buffer(65536, Qt::Uninitialized);
  DWORD bytesRead = 0;
  while (WinHttpReadData(hRequest.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead) && bytesRead > 0) {
    if (file.write(buffer.constData(), bytesRead) != static_cast<qint64>(bytesRead)) {
      errorOut = QStringLiteral("Error writing to file: %1").arg(file.errorString());
      file.close();
      return false;
    }
    receivedBytes += bytesRead;
    if (progressCallback) {
      progressCallback(receivedBytes, totalBytes);
    }
  }

  file.close();
  return true;
}
#else
HttpResponse httpGet(const QString &urlString) {
  HttpResponse resp;
  QNetworkAccessManager manager;
  QNetworkRequest request(QUrl(urlString));
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VYNX-QR-Updater"));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = manager.get(request);
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  if (reply->error() != QNetworkReply::NoError) {
    resp.error = reply->errorString();
  } else {
    resp.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    resp.body = reply->readAll();
  }
  reply->deleteLater();
  return resp;
}

bool downloadFile(const QString &urlString, const QString &destPath,
                  const std::function<void(qint64, qint64)> &progressCallback,
                  QString &errorOut) {
  QNetworkAccessManager manager;
  QNetworkRequest request(QUrl(urlString));
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VYNX-QR-Updater"));
  request.setRawHeader("Accept", "application/octet-stream");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  QFile file(destPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    errorOut = QStringLiteral("Cannot open file %1: %2").arg(destPath, file.errorString());
    return false;
  }

  QNetworkReply *reply = manager.get(request);
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(reply, &QNetworkReply::readyRead, [&file, reply] {
    file.write(reply->readAll());
  });
  QObject::connect(reply, &QNetworkReply::downloadProgress, [&progressCallback](qint64 received, qint64 total) {
    if (progressCallback) progressCallback(received, total);
  });
  loop.exec();

  if (reply->error() != QNetworkReply::NoError) {
    errorOut = reply->errorString();
    file.close();
    reply->deleteLater();
    return false;
  }
  file.close();
  reply->deleteLater();
  return true;
}
#endif

} // namespace

AutoUpdater::AutoUpdater(QObject *parent) : QObject(parent) {}

AutoUpdater::~AutoUpdater() = default;

bool AutoUpdater::isNewerVersion(const QString &remote, const QString &current) {
  auto parseParts = [](QString v) -> std::vector<int> {
    while (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V'))) {
      v.remove(0, 1);
    }
    const int dashIdx = v.indexOf(QLatin1Char('-'));
    if (dashIdx >= 0) {
      v = v.left(dashIdx);
    }
    const auto list = v.split(QLatin1Char('.'));
    std::vector<int> parts;
    for (const auto &item : list) {
      bool ok = false;
      const int num = item.toInt(&ok);
      parts.push_back(ok ? num : 0);
    }
    while (parts.size() < 3) {
      parts.push_back(0);
    }
    return parts;
  };

  const auto rParts = parseParts(remote);
  const auto cParts = parseParts(current);
  const size_t count = std::max(rParts.size(), cParts.size());
  for (size_t i = 0; i < count; ++i) {
    const int r = (i < rParts.size()) ? rParts[i] : 0;
    const int c = (i < cParts.size()) ? cParts[i] : 0;
    if (r > c) return true;
    if (r < c) return false;
  }
  return false;
}

void AutoUpdater::checkForUpdates(bool silent) {
  if (checking_) return;
  checking_ = true;

  QThread::create([this, silent] {
    const QString apiUrl = QStringLiteral("https://api.github.com/repos/Maks0101aps/VYNX-QR/releases/latest");
    HttpResponse resp = httpGet(apiUrl);

    if (resp.statusCode != 200 || resp.body.isEmpty()) {
      QMetaObject::invokeMethod(this, [this, silent, err = resp.error] {
        checking_ = false;
        if (!silent) {
          emit checkFailed(err.isEmpty() ? QStringLiteral("Network error") : err);
        }
      });
      return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(resp.body, &parseError);
    if (doc.isNull() || !doc.isObject()) {
      QMetaObject::invokeMethod(this, [this, silent] {
        checking_ = false;
        if (!silent) {
          emit checkFailed(QStringLiteral("Invalid JSON response from server"));
        }
      });
      return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    QString remoteVer = tagName;
    if (remoteVer.startsWith(QLatin1Char('v')) || remoteVer.startsWith(QLatin1Char('V'))) {
      remoteVer.remove(0, 1);
    }

    const QString currentVer = PlatformIntegration::versionString();
    if (!isNewerVersion(remoteVer, currentVer)) {
      QMetaObject::invokeMethod(this, [this, silent] {
        checking_ = false;
        if (!silent) {
          emit noUpdateAvailable();
        }
      });
      return;
    }

    UpdateInfo info;
    info.version = remoteVer;
    info.tagName = tagName;
    info.releaseNotes = obj.value(QStringLiteral("body")).toString();
    info.releaseUrl = obj.value(QStringLiteral("html_url")).toString();

    QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
    for (const auto &assetVal : assets) {
      QJsonObject assetObj = assetVal.toObject();
      QString name = assetObj.value(QStringLiteral("name")).toString();
      QString downloadUrl = assetObj.value(QStringLiteral("browser_download_url")).toString();
      qint64 size = static_cast<qint64>(assetObj.value(QStringLiteral("size")).toDouble());

#ifdef Q_OS_WIN
      if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        if (name.contains(QStringLiteral("Setup"), Qt::CaseInsensitive)) {
          info.downloadUrl = downloadUrl;
          info.assetName = name;
          info.assetSize = size;
          break;
        } else if (info.downloadUrl.isEmpty()) {
          info.downloadUrl = downloadUrl;
          info.assetName = name;
          info.assetSize = size;
        }
      }
#else
      if (name.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)) {
        info.downloadUrl = downloadUrl;
        info.assetName = name;
        info.assetSize = size;
        break;
      }
#endif
    }

    QMetaObject::invokeMethod(this, [this, info] {
      checking_ = false;
      emit updateAvailable(info);
    });
  })->start();
}

void AutoUpdater::downloadAndInstall(const UpdateInfo &info) {
  if (downloading_) return;
  if (info.downloadUrl.isEmpty()) {
    emit downloadFailed(QStringLiteral("No download link found for this release."));
    return;
  }
  downloading_ = true;

  const QString fileName = info.assetName.isEmpty() ? QStringLiteral("VYNX-QR-Update.exe") : info.assetName;
  const QString destPath = QDir(QDir::tempPath()).filePath(fileName);

  QThread::create([this, url = info.downloadUrl, destPath] {
    QString error;
    bool success = downloadFile(url, destPath, [this](qint64 rec, qint64 tot) {
      QMetaObject::invokeMethod(this, [this, rec, tot] {
        emit downloadProgress(rec, tot);
      });
    }, error);

    QMetaObject::invokeMethod(this, [this, success, destPath, error] {
      downloading_ = false;
      if (success) {
        emit downloadFinished(destPath);
      } else {
        emit downloadFailed(error);
      }
    });
  })->start();
}

void AutoUpdater::installAndRestart(const QString &installerPath) {
#ifdef Q_OS_WIN
  const QString nativeApp = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  const QString nativeSetup = QDir::toNativeSeparators(installerPath);

  // NSIS per-user installer runs silently with /S.
  // Wait 1s so the current process exits and releases file locks, run installer, then start updated app.
  const QString cmd = QStringLiteral("timeout /t 1 /nobreak >nul & start \"\" /wait \"%1\" /S & start \"\" \"%2\"")
                          .arg(nativeSetup, nativeApp);

  QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/C"), cmd});
  QApplication::quit();
  std::exit(0);
#else
  // Linux AppImage / executable
  QFile::setPermissions(installerPath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
      QFileDevice::ReadGroup | QFileDevice::ExeGroup |
      QFileDevice::ReadOther | QFileDevice::ExeOther);
  QProcess::startDetached(installerPath, {});
  QApplication::quit();
  std::exit(0);
#endif
}
