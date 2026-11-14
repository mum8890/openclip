#include "dialogs.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QDoubleSpinBox* dspin(double lo, double hi, double step, double val, int dec = 2) {
    auto* s = new QDoubleSpinBox;
    s->setRange(lo, hi); s->setSingleStep(step); s->setDecimals(dec); s->setValue(val);
    return s;
}
QComboBox* combo(const QStringList& items, const QString& current) {
    auto* c = new QComboBox;
    c->addItems(items);
    c->setCurrentText(current);
    return c;
}
bool runDialog(QDialog& dlg, QFormLayout* form, const QString& note = {}) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto* root = new QVBoxLayout(&dlg);
    root->addLayout(form);
    if (!note.isEmpty()) { auto* l = new QLabel(note); l->setWordWrap(true); root->addWidget(l); }
    root->addWidget(box);
    return dlg.exec() == QDialog::Accepted;
}
} // namespace

bool editClipDialog(QWidget* parent, Clip& c) {
    QDialog dlg(parent);
    dlg.setWindowTitle("Edit clip");
    auto* form = new QFormLayout;
    auto* speed = dspin(0.5, 2.0, 0.05, c.speed);
    auto* effect = combo(kEffects, c.effect);
    auto* bright = dspin(-1, 1, 0.05, c.brightness);
    auto* contrast = dspin(0, 3, 0.05, c.contrast);
    auto* sat = dspin(0, 3, 0.05, c.saturation);
    auto* trans = new QComboBox;
    for (const QString& t : kTransitions) trans->addItem(transitionLabel(t), t);
    trans->setCurrentIndex(std::max(0, trans->findData(c.transition)));
    auto* tdur = dspin(0.1, 3.0, 0.1, c.transitionDur, 1);
    form->addRow("Speed (0.5 slow-mo, 2 fast)", speed);
    form->addRow("Effect / filter", effect);
    form->addRow("Brightness", bright);
    form->addRow("Contrast", contrast);
    form->addRow("Saturation", sat);
    form->addRow("Transition from previous clip", trans);
    form->addRow("Transition length (s)", tdur);
    if (!runDialog(dlg, form, "The player now shows the real look: pause on any frame to see the effect, and press Play to watch the rendered preview. Effects and transitions also appear in the exported video and in Quick preview."))
        return false;
    c.speed = speed->value(); c.effect = effect->currentText();
    c.brightness = bright->value(); c.contrast = contrast->value(); c.saturation = sat->value();
    c.transition = trans->currentData().toString(); c.transitionDur = tdur->value();
    return true;
}

bool addTextDialog(QWidget* parent, TextItem& t) {
    QDialog dlg(parent);
    dlg.setWindowTitle("Add text");
    auto* form = new QFormLayout;
    auto* text = new QLineEdit(t.text);
    auto* start = dspin(0, 36000, 0.5, t.start);
    auto* dur = dspin(0.5, 600, 0.5, t.duration);
    auto* style = combo(kTextStyles, t.style);
    auto* pos = combo(kTextPositions, t.position);
    auto* anim = combo(kTextAnimations, t.animation);
    auto* size = new QSpinBox; size->setRange(3, 30); size->setValue(t.sizePct); size->setSuffix(" %");
    form->addRow("Text", text);
    form->addRow("Starts at (s)", start);
    form->addRow("Shown for (s)", dur);
    form->addRow("Style", style);
    form->addRow("Position", pos);
    form->addRow("Animation", anim);
    form->addRow("Size (% of video height)", size);
    if (!runDialog(dlg, form, "Times are in the exported video. Click a purple text bar on the timeline and press Delete to remove it.")) return false;
    if (text->text().trimmed().isEmpty()) return false;
    t.text = text->text(); t.start = start->value(); t.duration = dur->value();
    t.style = style->currentText(); t.position = pos->currentText();
    t.animation = anim->currentText(); t.sizePct = size->value();
    return true;
}

bool exportDialog(QWidget* parent, ExportSettings& s, const QString& title) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto* form = new QFormLayout;

    auto* res = new QComboBox;
    const QList<QPair<QString, int>> resList{{"480p", 480}, {"720p HD", 720}, {"1080p Full HD", 1080},
                                             {"1440p 2K", 1440}, {"4K Ultra HD", 2160}, {"8K", 4320}};
    for (const auto& r : resList) { res->addItem(r.first, r.second); if (r.second == s.base) res->setCurrentIndex(res->count() - 1); }

    auto* aspect = new QComboBox;
    aspect->addItem("16:9 Landscape (YouTube)", "16:9");
    aspect->addItem("9:16 Vertical (Shorts / TikTok / Reels)", "9:16");
    aspect->addItem("1:1 Square", "1:1");
    aspect->setCurrentIndex(std::max(0, aspect->findData(s.aspect)));

    auto* fit = new QComboBox;
    fit->addItem("Fit - keep the whole picture (black bars)", false);
    fit->addItem("Fill - crop to cover the frame", true);
    fit->setCurrentIndex(s.fill ? 1 : 0);

    auto* fps = new QComboBox;
    for (int f : {24, 30, 60}) { fps->addItem(QString("%1 fps").arg(f), f); if (f == s.fps) fps->setCurrentIndex(fps->count() - 1); }

    auto* quality = new QComboBox;
    quality->addItem("High quality (big file)", 18);
    quality->addItem("Balanced", 23);
    quality->addItem("Small file", 28);
    quality->setCurrentIndex(s.crf <= 20 ? 0 : (s.crf <= 25 ? 1 : 2));

    auto* up = combo({"None", "Lanczos", "Lanczos + sharpen", "Lanczos + clean + sharpen"}, s.upscale);

    auto* speed = new QComboBox;
    speed->addItem("Fast export", "veryfast");
    speed->addItem("Balanced", "medium");
    speed->addItem("Best compression (slow)", "slow");
    speed->setCurrentIndex(std::max(0, speed->findData(s.preset)));

    form->addRow("Resolution", res);
    form->addRow("Shape", aspect);
    form->addRow("Picture", fit);
    form->addRow("Frame rate", fps);
    form->addRow("Quality", quality);
    form->addRow("Upscaling (low-quality clips)", up);
    form->addRow("Speed", speed);

    if (!runDialog(dlg, form,
        "Upscaling stretches smaller footage to the chosen size using high-quality scaling, with optional "
        "sharpening and noise cleanup. It cannot invent real detail like AI upscalers do. 4K and 8K take a long "
        "time and a lot of memory; 8K uses H.265, which some players need an extension for.")) return false;

    s.base = res->currentData().toInt(); s.aspect = aspect->currentData().toString();
    s.fill = fit->currentData().toBool(); s.fps = fps->currentData().toInt();
    s.crf = quality->currentData().toInt(); s.upscale = up->currentText();
    s.preset = speed->currentData().toString();
    return true;
}

bool templateDialog(QWidget* parent, VideoTemplate& chosen, QVector<SlotPick>& picks) {
    QVector<VideoTemplate> all = loadBuiltInTemplates();
    if (all.isEmpty()) { QMessageBox::warning(parent, "Templates", "No templates found."); return false; }

    QDialog dlg(parent);
    dlg.setWindowTitle("New from template");
    dlg.resize(760, 420);
    auto* root = new QVBoxLayout(&dlg);
    auto* top = new QHBoxLayout;
    auto* chooser = new QComboBox;
    for (const auto& t : all) chooser->addItem(t.name);
    auto* openBtn = new QPushButton("Open template file...");
    top->addWidget(chooser, 1);
    top->addWidget(openBtn);
    root->addLayout(top);
    auto* desc = new QLabel;
    desc->setWordWrap(true);
    root->addWidget(desc);
    auto* holderLay = new QVBoxLayout;
    root->addLayout(holderLay, 1);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText("Create project");
    root->addWidget(box);

    QWidget* holder = nullptr;
    QVector<QLineEdit*> paths;
    QVector<QDoubleSpinBox*> starts;
    auto rebuild = [&]() {
        delete holder;
        paths.clear(); starts.clear();
        holder = new QWidget;
        auto* form = new QFormLayout(holder);
        const VideoTemplate& t = all[chooser->currentIndex()];
        desc->setText(t.description + "\n\nChoose a video for each part. You can still trim, edit and change everything afterwards.");
        for (const TemplateSlot& s : t.parts) {
            auto* row = new QWidget;
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            auto* path = new QLineEdit; path->setReadOnly(true); path->setPlaceholderText("No video chosen");
            auto* btn = new QPushButton("Browse...");
            auto* start = new QDoubleSpinBox; start->setRange(0, 36000); start->setDecimals(1);
            start->setPrefix("start at "); start->setSuffix(" s");
            h->addWidget(path, 1); h->addWidget(btn); h->addWidget(start);
            QObject::connect(btn, &QPushButton::clicked, &dlg, [path, &dlg]() {
                const QString f = QFileDialog::getOpenFileName(&dlg, "Choose video", {}, "Video (*.mp4 *.mov *.mkv *.webm *.avi)");
                if (!f.isEmpty()) path->setText(f);
            });
            QString label = QString("%1 (%2 s").arg(s.label).arg(s.duration);
            if (s.speed != 1.0) label += QString(", %1x").arg(s.speed);
            form->addRow(label + ")", row);
            paths << path; starts << start;
        }
        holderLay->addWidget(holder);
    };
    QObject::connect(chooser, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { rebuild(); });
    QObject::connect(openBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString f = QFileDialog::getOpenFileName(&dlg, "Open template file", {}, "Template (*.json)");
        if (f.isEmpty()) return;
        VideoTemplate t;
        if (!loadTemplateFile(f, t)) { QMessageBox::warning(&dlg, "Template", "That file is not a valid template."); return; }
        all.append(t);
        chooser->addItem(t.name);
        chooser->setCurrentIndex(chooser->count() - 1);
    });
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        for (auto* p : paths)
            if (p->text().isEmpty()) { QMessageBox::information(&dlg, "Template", "Please choose a video for every part."); return; }
        dlg.accept();
    });
    rebuild();

    if (dlg.exec() != QDialog::Accepted) return false;
    chosen = all[chooser->currentIndex()];
    picks.clear();
    for (int i = 0; i < paths.size(); ++i) picks.append({paths[i]->text(), starts[i]->value()});
    return true;
}

// ---- export progress ---------------------------------------------------------------------------------

namespace {
QString mmss(double seconds) {
    const int total = int(std::max(0.0, seconds) + 0.5);
    return QString("%1:%2").arg(total / 60, 2, 10, QChar('0')).arg(total % 60, 2, 10, QChar('0'));
}
} // namespace

ExportProgressDialog::ExportProgressDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    setModal(false);
    setMinimumWidth(440);
    m_clock.start();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 16);
    root->setSpacing(8);

    m_percent = new QLabel("0%");
    m_percent->setObjectName("percent");
    m_percent->setAlignment(Qt::AlignCenter);

    m_status = new QLabel("Starting FFmpeg...");
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);

    m_bar = new QProgressBar;
    m_bar->setObjectName("big");
    m_bar->setRange(0, 1000);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);

    m_timing = new QLabel("Starting...");
    m_timing->setObjectName("muted");
    m_timing->setAlignment(Qt::AlignCenter);

    auto* bottom = new QHBoxLayout;
    m_cancel = new QPushButton("Cancel");
    m_cancel->setMinimumWidth(110);
    bottom->addStretch();
    bottom->addWidget(m_cancel);

    root->addWidget(m_percent);
    root->addWidget(m_bar);
    root->addWidget(m_status);
    root->addWidget(m_timing);
    root->addSpacing(4);
    root->addLayout(bottom);

    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        if (m_done) { accept(); return; }
        if (m_cancelled) return;
        m_cancelled = true;
        m_cancel->setText("Cancelling...");
        m_cancel->setEnabled(false);
        m_status->setText("Stopping FFmpeg...");
        emit cancelRequested();
    });
}

void ExportProgressDialog::setProgress(double fraction) {
    if (m_done || m_cancelled) return;
    const double f = std::clamp(fraction, 0.0, 1.0);
    const int pct = int(f * 100.0 + 0.5);
    m_percent->setText(QString("%1%").arg(pct));
    m_bar->setValue(int(f * 1000));
    const double elapsed = m_clock.elapsed() / 1000.0;
    if (f > 0.01 && elapsed > 1.0) {
        const double left = elapsed * (1.0 - f) / f;
        m_timing->setText(QString("%1 elapsed  -  about %2 left").arg(mmss(elapsed), mmss(left)));
    } else {
        m_timing->setText(QString("%1 elapsed").arg(mmss(elapsed)));
    }
}

void ExportProgressDialog::setMessage(const QString& message) {
    if (!m_done) m_status->setText(message);
}

void ExportProgressDialog::complete(bool ok, const QString& message) {
    if (m_done) return;
    m_done = true;
    m_bar->setValue(ok ? 1000 : m_bar->value());
    if (ok) {
        m_percent->setText("100%");
        m_status->setText(message.isEmpty() ? "Finished." : message);
        m_timing->setText(QString("Done in %1").arg(mmss(m_clock.elapsed() / 1000.0)));
        m_cancel->setText("Close");
        m_cancel->setEnabled(true);
        QTimer::singleShot(1200, this, &QDialog::accept);
    } else {
        m_status->setText(message.isEmpty() ? "Stopped." : message);
        m_cancel->setText("Close");
        m_cancel->setEnabled(true);
    }
}
