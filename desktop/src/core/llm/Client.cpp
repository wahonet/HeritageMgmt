#include "Client.h"

#include <QEventLoop>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace heritage::llm {

Client::Client(const LlmConfig& cfg) : cfg_(cfg) {}

bool Client::configured() const {
    return !cfg_.apiKey.trimmed().isEmpty();
}

QByteArray Client::buildRequestBody(const LlmConfig& cfg, const QVector<Message>& msgs,
                                    const Options& opts) {
    QJsonObject payload;
    payload[QStringLiteral("model")] = cfg.model;
    payload[QStringLiteral("temperature")] = opts.temperature;
    if (opts.maxTokens > 0)
        payload[QStringLiteral("max_tokens")] = opts.maxTokens;
    QJsonArray arr;
    for (const Message& m : msgs) {
        QJsonObject o;
        o[QStringLiteral("role")] = m.role;
        o[QStringLiteral("content")] = m.content;
        arr.append(o);
    }
    payload[QStringLiteral("messages")] = arr;
    if (opts.jsonObject) {
        QJsonObject rf;
        rf[QStringLiteral("type")] = QStringLiteral("json_object");
        payload[QStringLiteral("response_format")] = rf;
    }
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QString Client::parseContent(const QByteArray& body, QString* err) {
    if (err) err->clear();
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) {
        if (err) *err = QStringLiteral("响应非合法 JSON: ") + pe.errorString();
        return {};
    }
    const QJsonObject o = d.object();
    const QJsonObject errObj = o.value(QStringLiteral("error")).toObject();
    if (!errObj.isEmpty()) {
        if (err) *err = QStringLiteral("大模型错误: ") + errObj.value(QStringLiteral("message")).toString();
        return {};
    }
    const QJsonArray choices = o.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (err) *err = QStringLiteral("大模型返回为空");
        return {};
    }
    const QString content = choices.at(0).toObject()
                                .value(QStringLiteral("message"))
                                .toObject()
                                .value(QStringLiteral("content"))
                                .toString();
    return content.trimmed();
}

QString Client::chat(const QVector<Message>& messages, const Options& opts, QString* err) {
    if (err) err->clear();
    if (!configured()) {
        if (err) *err = QStringLiteral("未配置API Key,请编辑 config/llm.json");
        return {};
    }
    const QByteArray body = buildRequestBody(cfg_, messages, opts);
    const QString url = QString(cfg_.baseUrl).replace(QRegularExpression(QStringLiteral("/+$")), QString()) +
                        QStringLiteral("/chat/completions");

    QNetworkAccessManager nam;
    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + cfg_.apiKey.toUtf8());

    QNetworkReply* reply = nam.post(req, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    const int timeoutMs = opts.timeoutMs > 0 ? opts.timeoutMs : 90000;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (reply) reply->abort();
        loop.quit();
    });
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        if (err) *err = QStringLiteral("调用大模型超时");
        reply->deleteLater();
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = QStringLiteral("调用大模型失败(需联网): ") + reply->errorString();
        const QByteArray rbody = reply->readAll();
        if (!rbody.isEmpty())
            if (err) *err += QStringLiteral("\n") + QString::fromUtf8(rbody).left(400);
        reply->deleteLater();
        return {};
    }
    const QByteArray rbody = reply->readAll();
    reply->deleteLater();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode != 200) {
        if (err) *err = QStringLiteral("大模型API返回 %1: %2").arg(httpCode).arg(QString::fromUtf8(rbody).left(400));
        return {};
    }
    return parseContent(rbody, err);
}

} // namespace heritage::llm
