#include "aiagent.h"
#include "timeline.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QJsonObject strProp(const QString& desc) {
    QJsonObject p; p["type"] = QStringLiteral("string"); p["description"] = desc; return p;
}
QJsonObject numProp(const QString& desc) {
    QJsonObject p; p["type"] = QStringLiteral("number"); p["description"] = desc; return p;
}
QJsonObject intProp(const QString& desc) {
    QJsonObject p; p["type"] = QStringLiteral("integer"); p["description"] = desc; return p;
}
QJsonObject tool(const QString& name, const QString& desc,
                 const QJsonObject& props, const QJsonArray& required) {
    QJsonObject params;
    params["type"] = QStringLiteral("object");
    params["properties"] = props;
    params["required"] = required;
    QJsonObject fn;
    fn["name"] = name;
    fn["description"] = desc;
    fn["parameters"] = params;
    QJsonObject t;
    t["type"] = QStringLiteral("function");
    t["function"] = fn;
    return t;
}

QString clipEffectList() { return kEffects.join(", "); }
QString transitionNames() {
    QStringList out;
    for (const QString& t : kTransitions)
        if (t != "None") out << QString("%1 (%2)").arg(transitionLabel(t), t);
    return out.join(", ");
}

QString apiErrorMessage(const QByteArray& data) {
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(data, &pe);
    if (d.isObject()) {
        const QJsonObject o = d.object();
        const QJsonValue err = o.value("error");
        if (err.isObject()) return err.toObject().value("message").toString();
        if (err.isString()) return err.toString();
        if (o.contains("message")) return o.value("message").toString();
        if (o.contains("detail")) return o.value("detail").toString();
    }
    return QString::fromUtf8(data.left(400)).trimmed();
}

QString friendlyError(int status, const QByteArray& data) {
    QString detail = apiErrorMessage(data);
    QString head;
    switch (status) {
        case 400: head = "The provider did not understand the request (HTTP 400)."; break;
        case 401: head = "The API key was rejected (HTTP 401)."; break;
        case 403: head = "The provider refused access (HTTP 403)."; break;
        case 404: head = "That address was not found (HTTP 404)."; break;
        case 429: head = "The free limit was reached (HTTP 429). Wait a minute and try again."; break;
        case 500: case 502: case 503: case 504:
            head = QString("The provider is busy (HTTP %1). Try again in a moment.").arg(status); break;
        default:  head = QString("The AI request failed (HTTP %1).").arg(status); break;
    }
    if (status == 404)
        head += " Check the Base URL - it should end with /v1 (Gemini ends with /v1beta/openai).";
    if (!detail.isEmpty()) head += "\n\n" + detail;
    return head;
}

} // namespace

QJsonArray aiToolDefinitions() {
    QJsonArray tools;
    tools.append(tool("get_timeline",
        "Read the current project: clips, titles and the media library. Always call this first "
        "so you know the real 0-based indexes and lengths.",
        QJsonObject{}, {}));
    tools.append(tool("add_clip",
        "Add one item from the media library to the end of the timeline. Use the media index "
        "returned by get_timeline.",
        QJsonObject{{QStringLiteral("media"), intProp("0-based index in the media library")}},
        QJsonArray{QStringLiteral("media")}));
    tools.append(tool("set_effect",
        "Give a clip a look. Available effects: " + clipEffectList() + ".",
        QJsonObject{{QStringLiteral("clip"), intProp("0-based timeline clip index")},
                    {QStringLiteral("effect"), strProp("the effect name exactly as listed, or None")}},
        QJsonArray{QStringLiteral("clip"), QStringLiteral("effect")}));
    tools.append(tool("set_transition",
        "Put a transition between a clip and the clip before it. Clip index must be 1 or more. "
        "Available transitions: " + transitionNames() + ".",
        QJsonObject{{QStringLiteral("clip"), intProp("0-based index of the incoming clip (1 or more)")},
                    {QStringLiteral("transition"), strProp("FFmpeg transition name such as fade, dissolve or slideleft")},
                    {QStringLiteral("duration"), numProp("overlap in seconds, 0.1 to 2.0 (optional, default 0.5)")}},
        QJsonArray{QStringLiteral("clip"), QStringLiteral("transition")}));
    tools.append(tool("set_speed",
        "Change playback speed of a clip: 0.5 = slow motion, 1.0 = normal, 2.0 = fast.",
        QJsonObject{{QStringLiteral("clip"), intProp("0-based timeline clip index")},
                    {QStringLiteral("speed"), numProp("0.5 to 2.0")}},
        QJsonArray{QStringLiteral("clip"), QStringLiteral("speed")}));
    tools.append(tool("set_colour",
        "Adjust colour of a clip. brightness -1..1, contrast 0..3, saturation 0..3. "
        "Only the values you send are changed.",
        QJsonObject{{QStringLiteral("clip"), intProp("0-based timeline clip index")},
                    {QStringLiteral("brightness"), numProp("optional, -1 to 1, 0 = normal")},
                    {QStringLiteral("contrast"), numProp("optional, 0 to 3, 1 = normal")},
                    {QStringLiteral("saturation"), numProp("optional, 0 to 3, 1 = normal")}},
        QJsonArray{QStringLiteral("clip")}));
    tools.append(tool("add_title",
        "Add a text title over the video. Styles: " + kTextStyles.join(", ") +
        ". Positions: Top, Center, Bottom.",
        QJsonObject{{QStringLiteral("text"), strProp("the words to show")},
                    {QStringLiteral("start"), numProp("start time in seconds")},
                    {QStringLiteral("duration"), numProp("how long it stays on screen, in seconds")},
                    {QStringLiteral("style"), strProp("optional style name, default Gaming")},
                    {QStringLiteral("position"), strProp("optional Top, Center or Bottom, default Bottom")}},
        QJsonArray{QStringLiteral("text"), QStringLiteral("start")}));
    tools.append(tool("split_at",
        "Cut the clip at this time into two clips. The time must be inside a clip, not in a transition.",
        QJsonObject{{QStringLiteral("time"), numProp("seconds on the timeline")}},
        QJsonArray{QStringLiteral("time")}));
    tools.append(tool("remove_clip",
        "Delete a clip from the timeline.",
        QJsonObject{{QStringLiteral("clip"), intProp("0-based timeline clip index")}},
        QJsonArray{QStringLiteral("clip")}));
    tools.append(tool("seek",
        "Move the playhead and show that frame in the preview.",
        QJsonObject{{QStringLiteral("time"), numProp("seconds on the timeline")}},
        QJsonArray{QStringLiteral("time")}));
    return tools;
}

AiAgent::AiAgent(QObject* parent) : QObject(parent) {
    m_net.setTransferTimeout(180000);
}

void AiAgent::reset() {
    m_messages = QJsonArray();
    m_round = 0;
}

void AiAgent::finish() {
    m_busy = false;
    m_reply = nullptr;
    emit finished();
}

void AiAgent::start(const AiSettings& settings, const QString& systemPrompt, const QString& userText,
                    const ToolExecutor& exec, const ToolApprover& approve) {
    if (m_busy) { emit failed("The assistant is still answering. Give it a second."); return; }
    m_cfg = settings;
    m_exec = exec;
    m_approve = approve;
    m_busy = true;
    m_cancelled = false;
    m_round = 0;

    if (m_messages.isEmpty()) {
        QJsonObject sys;
        sys["role"] = QStringLiteral("system");
        sys["content"] = systemPrompt;
        m_messages.append(sys);
    }
    QJsonObject user;
    user["role"] = QStringLiteral("user");
    user["content"] = userText;
    m_messages.append(user);
    step();
}

void AiAgent::step() {
    if (++m_round > 8) {
        emit failed("The assistant took too many steps. Try rephrasing the request more simply.");
        finish();
        return;
    }
    QJsonObject body;
    body["model"] = m_cfg.model;
    body["messages"] = m_messages;
    body["temperature"] = 0.2;
    body["stream"] = false;
    body["tools"] = aiToolDefinitions();
    body["tool_choice"] = QStringLiteral("auto");
    emit status("Thinking...");
    post(body);
}

void AiAgent::post(const QJsonObject& body) {
    QString base = m_cfg.baseUrl.trimmed();
    while (base.endsWith('/')) base.chop(1);
    if (base.isEmpty()) { emit failed("No Base URL is set. Open AI settings and choose a provider."); finish(); return; }

    QNetworkRequest req{QUrl(base + "/chat/completions")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    if (!m_cfg.apiKey.trimmed().isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_cfg.apiKey.trimmed().toUtf8());
    if (base.contains("openrouter.ai")) {
        req.setRawHeader("HTTP-Referer", "https://github.com/mum8890/openclip");
        req.setRawHeader("X-Title", "OpenClip");
    }

    QNetworkReply* reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleReply(reply); });
}

void AiAgent::handleReply(QNetworkReply* reply) {
    m_reply = nullptr;
    const QByteArray data = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netError = reply->error();
    const QString netText = reply->errorString();
    reply->deleteLater();

    if (m_cancelled) {
        m_cancelled = false;
        m_busy = false;
        emit status("Stopped.");
        emit finished();
        return;
    }
    if (httpCode == 0 && netError != QNetworkReply::NoError) {
        QString base = m_cfg.baseUrl;
        while (base.endsWith('/')) base.chop(1);
        emit failed(QString("Could not reach %1.\n\n%2\n\n"
                            "If you use Ollama, start it first: run \"ollama serve\" and make sure the model "
                            "is downloaded (for example \"ollama pull llama3.2\").")
                        .arg(base, netText));
        finish();
        return;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (httpCode < 200 || httpCode >= 300) {
        emit failed(friendlyError(httpCode, data));
        finish();
        return;
    }
    if (!doc.isObject()) {
        emit failed("The provider sent a reply OpenClip could not read. "
                    "Check that the Base URL points to an OpenAI-compatible API (it usually ends with /v1).");
        finish();
        return;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        emit failed(apiErrorMessage(data));
        finish();
        return;
    }
    const QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty()) {
        emit failed("The provider returned an empty answer. Try again, or pick a different model.");
        finish();
        return;
    }

    QJsonObject message = choices.first().toObject().value("message").toObject();
    const QString content = message.value("content").toString().trimmed();
    const QJsonArray calls = message.value("tool_calls").toArray();
    if (!content.isEmpty()) emit assistantText(content);

    if (calls.isEmpty()) {
        QJsonObject assistant;
        assistant["role"] = QStringLiteral("assistant");
        assistant["content"] = message.value("content").toString();
        m_messages.append(assistant);
        finish();
        return;
    }

    QJsonObject assistant = message;
    assistant["role"] = QStringLiteral("assistant");
    if (!assistant.contains("content")) assistant["content"] = QString();
    m_messages.append(assistant);

    if (m_approve && !m_approve(calls)) {
        for (const QJsonValue& v : calls) {
            const QJsonObject call = v.toObject();
            QJsonObject declined;
            declined["ok"] = false;
            declined["declined"] = true;
            declined["reason"] = QStringLiteral("The user declined this change.");
            QJsonObject toolMsg;
            toolMsg["role"] = QStringLiteral("tool");
            toolMsg["tool_call_id"] = call.value("id").toString();
            toolMsg["content"] = QString::fromUtf8(QJsonDocument(declined).toJson(QJsonDocument::Compact));
            m_messages.append(toolMsg);
        }
        emit assistantText("No changes made. Tell me what you would like instead.");
        finish();
        return;
    }

    for (const QJsonValue& v : calls) {
        const QJsonObject call = v.toObject();
        const QJsonObject fn = call.value("function").toObject();
        const QString name = fn.value("name").toString();
        const QJsonValue rawArgs = fn.value("arguments");
        QJsonObject args;
        if (rawArgs.isObject()) {
            args = rawArgs.toObject();
        } else if (rawArgs.isString()) {
            QJsonParseError ae;
            const QJsonDocument ad = QJsonDocument::fromJson(rawArgs.toString().toUtf8(), &ae);
            if (ad.isObject()) args = ad.object();
        }

        emit status(QString("Running %1...").arg(name));
        QJsonObject result;
        if (m_exec) {
            result = m_exec(name, args);
        } else {
            result["ok"] = false;
            result["error"] = QStringLiteral("No tool runner is attached.");
        }
        QJsonObject toolMsg;
        toolMsg["role"] = QStringLiteral("tool");
        toolMsg["tool_call_id"] = call.value("id").toString();
        toolMsg["content"] = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        m_messages.append(toolMsg);
    }
    step();
}
