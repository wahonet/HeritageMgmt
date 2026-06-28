#ifndef HERITAGE_LLM_CLIENT_H
#define HERITAGE_LLM_CLIENT_H

// 大模型客户端：调 OpenAI 兼容的 chat/completions（DeepSeek）。对应 Go internal/llm/client.go。
// 用 QNetworkAccessManager 同步阻塞（QEventLoop）调用。请求/响应解析抽成静态纯函数供单测。

#include "core/domain/ConfigTypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace heritage::llm {

struct Message {
    QString role;
    QString content;
};

struct Options {
    double temperature = 0.7;
    int maxTokens = 0;     // 0 = 不设置
    bool jsonObject = false;
    int timeoutMs = 90000; // 0 = 默认 90s
};

class Client {
public:
    explicit Client(const LlmConfig& cfg);

    bool configured() const;

    // 同步调用，返回首条 choice.message.content（trimmed）；失败时返回空并写 *err。
    QString chat(const QVector<Message>& messages, const Options& opts, QString* err);

    // 可单测的纯函数：
    static QByteArray buildRequestBody(const LlmConfig& cfg, const QVector<Message>& msgs,
                                       const Options& opts);
    // 从响应体解析：成功返回 content(trimmed)；失败返回空并写 *err。
    static QString parseContent(const QByteArray& body, QString* err);

private:
    LlmConfig cfg_;
};

} // namespace heritage::llm

#endif // HERITAGE_LLM_CLIENT_H
