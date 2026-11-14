#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class QNetworkReply;

// Settings for an OpenAI-compatible chat provider. OpenClip never needs an account:
// Ollama runs locally for free, and Groq / Gemini / OpenRouter all have free tiers.
struct AiSettings {
    QString preset  = "ollama";
    QString baseUrl = "http://localhost:11434/v1";
    QString model   = "llama3.2";
    QString apiKey;
    bool askBefore = true;
};

// The function/tool schema the agent is allowed to call.
QJsonArray aiToolDefinitions();

// Talks to an OpenAI-compatible /chat/completions endpoint and runs the tool calls it
// receives. The host application owns the actual timeline, so it supplies the executor.
class AiAgent : public QObject {
    Q_OBJECT
public:
    using ToolExecutor = std::function<QJsonObject(const QString& name, const QJsonObject& args)>;
    using ToolApprover = std::function<bool(const QJsonArray& calls)>;

    explicit AiAgent(QObject* parent = nullptr);

    bool busy() const { return m_busy; }
    void reset();                       // forget the conversation

    void start(const AiSettings& settings, const QString& systemPrompt, const QString& userText,
               const ToolExecutor& exec, const ToolApprover& approve);

signals:
    void assistantText(const QString& text);
    void status(const QString& text);
    void failed(const QString& message);
    void finished();

private:
    void step();
    void post(const QJsonObject& body);
    void handleReply(QNetworkReply* reply);
    void finish();

    AiSettings m_cfg;
    QJsonArray m_messages;
    QNetworkAccessManager m_net;
    QPointer<QNetworkReply> m_reply;
    ToolExecutor m_exec;
    ToolApprover m_approve;
    bool m_busy = false;
    bool m_cancelled = false;
    int m_round = 0;
};
