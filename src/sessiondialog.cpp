// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "sessiondialog.h"
#include "audio.h"
#include "breathengine.h"
#include "breathview.h"
#include "database.h"
#include "guidewidget.h"
#include "icons.h"
#include "measuredialog.h"
#include "playlistwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QtMath>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
enum Page { Prep, Before, Run, After, Summary };

QSlider *volumeSlider(int v)
{
    auto *s = new QSlider(Qt::Horizontal);
    s->setRange(0, 100);
    s->setValue(v);
    s->setMinimumWidth(140);
    return s;
}

QLabel *heading(const QString &text, double scale = 1.35)
{
    auto *l = new QLabel(text);
    QFont f = l->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * scale);
    l->setFont(f);
    l->setWordWrap(true);
    return l;
}

QLabel *emergencyLabel(const QString &text)
{
    auto *l = new QLabel(QStringLiteral("⚠ ") + text);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("QLabel { background-color: #fdecea; color: #8a1c13; border: 1px solid #e0a39e;"
                                    " border-radius: 5px; padding: 6px; font-weight: bold; }"));
    return l;
}

QWidget *scrollable(QWidget *content)
{
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(content);
    return sa;
}

Tones::Kind toneFor(PhaseKind k)
{
    switch (k) {
    case PhaseKind::Inhale: return Tones::Kind::Inhale;
    case PhaseKind::Exhale:
    case PhaseKind::Huff:   return Tones::Kind::Exhale;
    case PhaseKind::Calm:   return Tones::Kind::Phase;
    case PhaseKind::Ready:  return Tones::Kind::Reminder;
    default:                return Tones::Kind::Hold;
    }
}
} // namespace

SessionDialog::SessionDialog(qint64 userId, const QString &moduleId, QWidget *parent, const QString &eventName)
    : QDialog(parent), m_userId(userId), m_module(BreathModules::find(moduleId))
{
    if (!m_module) m_module = &BreathModules::all().first();
    setWindowTitle(tr("%1 – ViviaBreathe").arg(m_module->name));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowIcon(Icons::get(m_module->icon));
    m_engine = new BreathEngine(this);
    m_music = new MusicPlayer(this);
    m_music->setLoop(true);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    lay->addWidget(m_stack);
    m_stack->addWidget(buildPrepPage());
    m_stack->addWidget(buildMeasurePage(true));
    m_stack->addWidget(buildRunPage());
    m_stack->addWidget(buildMeasurePage(false));
    m_stack->addWidget(buildSummaryPage());

    loadSettings(ModuleSettings::load(userId, *m_module));
    if (!eventName.isEmpty()) m_event->setCurrentText(eventName);

    connect(m_engine, &BreathEngine::phaseChanged, this, &SessionDialog::onPhaseChanged);
    connect(m_engine, &BreathEngine::finished, this, &SessionDialog::onFinished);
    connect(m_engine, &BreathEngine::tick, this, &SessionDialog::updateRunInfo);
    connect(m_music, &MusicPlayer::errorOccurred, this, [this](const QString &msg) {
        m_runInfo->setToolTip(msg);
    });
    Tones::preload();
    resize(760, 820);
    // Atemnot-Hilfe: sofort starten, ohne Vorbereitung und ohne Messungen
    if (m_module->quickHelp)
        QTimer::singleShot(0, this, [this] {
            m_trainingBox->setChecked(false);
            m_musicBox->setChecked(false);
            startExercise();
        });
}

// ---------------------------------------------------------------- 1. Vorbereitung

QWidget *SessionDialog::buildPrepPage()
{
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);

    auto *head = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(Icons::get(m_module->icon).pixmap(48, 48));
    head->addWidget(icon, 0, Qt::AlignTop);
    auto *ht = new QVBoxLayout;
    ht->addWidget(heading(m_module->name, 1.5));
    auto *desc = new QLabel(m_module->description);
    desc->setWordWrap(true);
    ht->addWidget(desc);
    head->addLayout(ht, 1);
    lay->addLayout(head);

    // Tipps
    auto *tipsFrame = new QFrame;
    tipsFrame->setObjectName(QStringLiteral("vbTips"));
    tipsFrame->setStyleSheet(QStringLiteral("#vbTips { background-color: rgba(20,116,188,0.10); border: 1px solid rgba(20,116,188,0.45);"
                                            " border-radius: 6px; }"));
    auto *tl = new QVBoxLayout(tipsFrame);
    auto *tipHead = new QHBoxLayout;
    auto *ti = new QLabel;
    ti->setPixmap(Icons::get(QStringLiteral("info")).pixmap(20, 20));
    tipHead->addWidget(ti);
    tipHead->addWidget(heading(tr("Tipps vor dem Start"), 1.05), 1);
    m_showTips = new QCheckBox(tr("anzeigen"));
    tipHead->addWidget(m_showTips);
    tl->addLayout(tipHead);
    m_tips = new QLabel;
    m_tips->setWordWrap(true);
    m_tips->setTextFormat(Qt::RichText);
    QStringList items;
    for (const QString &t : m_module->tips + (m_module->airway ? BreathModules::airwayTips() : BreathModules::generalTips()))
        items << QStringLiteral("<li style='margin-bottom:3px'>%1</li>").arg(t.toHtmlEscaped());
    m_tips->setText(QStringLiteral("<ul style='margin-left:-20px'>%1</ul>").arg(items.join(QString())));
    tl->addWidget(m_tips);
    connect(m_showTips, &QCheckBox::toggled, m_tips, &QLabel::setVisible);
    lay->addWidget(tipsFrame);
    if (!m_module->emergencyNote.isEmpty()) lay->addWidget(emergencyLabel(m_module->emergencyNote));
    if (!m_module->guide.isEmpty()) {
        auto *gbox = new QGroupBox(tr("Bebilderte Anleitung"));
        auto *gl = new QVBoxLayout(gbox);
        auto *show = new QCheckBox(tr("anzeigen"));
        show->setChecked(true);
        gl->addWidget(show);
        auto *guide = new GuideWidget(m_module->guide, 160);
        gl->addWidget(guide);
        connect(show, &QCheckBox::toggled, guide, &QWidget::setVisible);
        lay->addWidget(gbox);
    } else if (!m_module->posture.isEmpty()) {
        auto *row = new QHBoxLayout;
        if (!m_module->postureImage.isEmpty()) {
            auto *img = new QLabel;
            img->setPixmap(Icons::illustration(m_module->postureImage, QSize(150, 112)));
            row->addWidget(img);
        }
        auto *posture = new QLabel(tr("<b>Haltung:</b> %1").arg(m_module->posture.toHtmlEscaped()));
        posture->setWordWrap(true);
        row->addWidget(posture, 1);
        lay->addLayout(row);
    }

    // Dauer und Phasen
    auto *timing = new QGroupBox(tr("Dauer und Atemrhythmus"));
    auto *tg = new QGridLayout(timing);
    auto *totalLabel = new QLabel(tr("Gesamtdauer:"));
    tg->addWidget(totalLabel, 0, 0);
    m_total = new QSpinBox;
    m_total->setRange(1, 60);
    m_total->setSuffix(tr(" min"));
    tg->addWidget(m_total, 0, 1);
    m_total->setMaximum(qMax(1, m_module->maxTotalSec / 60));
    if (m_module->singleRound) {          // Übung = festgelegte Zahl von Wiederholungen
        totalLabel->hide();
        m_total->hide();
    }
    if (m_module->deviceTraining) {
        auto *tl = new QLabel(tr("Ziel:"));
        tg->addWidget(tl, 0, 0);
        m_target = new QComboBox;
        m_target->addItem(tr("1 Ball (600 ml/s)"), 1);
        m_target->addItem(tr("2 Bälle (900 ml/s)"), 2);
        m_target->addItem(tr("3 Bälle (1200 ml/s)"), 3);
        m_target->setToolTip(tr("Wie viele Bälle Sie anheben und oben halten möchten. Erst steigern, wenn das Ziel sicher gelingt."));
        tg->addWidget(m_target, 0, 1);
        auto *guideBtn = new QPushButton(Icons::get(QStringLiteral("info")), tr("Anleitung …"));
        connect(guideBtn, &QPushButton::clicked, this, [this] { GuideDialog(*m_module, this).exec(); });
        tg->addWidget(guideBtn, 0, 2, Qt::AlignRight);
    }
    int row = 1;
    const QList<SegmentDef> segs = m_module->effectiveSegments();
    m_phaseSpins.resize(m_module->phases.size());
    for (int g = 0; g < segs.size(); ++g) {
        const SegmentDef &seg = segs.at(g);
        QSpinBox *rep = nullptr;
        if (m_module->hasSegments()) {
            auto *title = new QLabel(QStringLiteral("<b>%1</b>").arg(seg.title.toHtmlEscaped()));
            tg->addWidget(title, row, 0);
            if (seg.maxRepeats > 1) {
                rep = new QSpinBox;
                rep->setRange(seg.minRepeats, seg.maxRepeats);
                rep->setSuffix(tr(" ×"));
                rep->setToolTip(tr("Wiederholungen je Runde"));
                tg->addWidget(rep, row, 1);
                connect(rep, qOverload<int>(&QSpinBox::valueChanged), this, &SessionDialog::updateCycleInfo);
            }
            if (seg.readyAfter) {
                auto *ready = new QLabel(tr("danach: abhusten, weiter mit Leertaste"));
                ready->setEnabled(false);
                tg->addWidget(ready, row, 2);
            }
            ++row;
        }
        m_repeatSpins << rep;
        for (int i = seg.firstPhase; i < seg.firstPhase + seg.phaseCount; ++i) {
            const PhaseDef &p = m_module->phases.at(i);
            tg->addWidget(new QLabel((m_module->hasSegments() ? QStringLiteral("    ") : QString()) + phaseName(p.kind) + QLatin1Char(':')), row, 0);
            auto *sp = new QDoubleSpinBox;
            sp->setRange(p.minSec, p.maxSec);
            sp->setDecimals(1);
            sp->setSingleStep(p.maxSec > 30 ? 5 : 0.5);
            sp->setSuffix(tr(" s"));
            if (p.minSec <= 0) sp->setSpecialValueText(tr("aus"));
            tg->addWidget(sp, row, 1);
            auto *hint = new QLabel(p.hint);
            hint->setEnabled(false);
            hint->setWordWrap(true);
            tg->addWidget(hint, row, 2);
            m_phaseSpins[i] = sp;
            connect(sp, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &SessionDialog::updateCycleInfo);
            ++row;
        }
    }
    connect(m_total, qOverload<int>(&QSpinBox::valueChanged), this, &SessionDialog::updateCycleInfo);
    m_cycleInfo = new QLabel;
    tg->addWidget(m_cycleInfo, row, 0, 1, 2);
    auto *reset = new QPushButton(Icons::get(QStringLiteral("reset")), tr("Reset"));
    reset->setToolTip(tr("Standardwerte wiederherstellen (%1, %2 min)")
                          .arg(m_module->defaultPattern()).arg(m_module->defaultTotalSec / 60));
    connect(reset, &QPushButton::clicked, this, [this] {
        const ModuleSettings d = ModuleSettings::defaults(*m_module);
        m_total->setValue(qMax(1, d.totalSec / 60));
        if (m_target) m_target->setCurrentIndex(0);
        for (int i = 0; i < m_phaseSpins.size(); ++i) m_phaseSpins[i]->setValue(d.phaseSec.value(i));
        for (int i = 0; i < m_repeatSpins.size(); ++i)
            if (m_repeatSpins[i]) m_repeatSpins[i]->setValue(d.repeats.value(i, 1));
    });
    tg->addWidget(reset, row, 2, Qt::AlignRight);
    tg->setColumnStretch(2, 1);
    lay->addWidget(timing);

    // Anlass
    auto *ev = new QHBoxLayout;
    ev->addWidget(new QLabel(tr("Anlass:")));
    m_event = new QComboBox;
    m_event->setEditable(true);
    m_event->addItems(Database::instance().eventNames(m_userId));
    m_event->setToolTip(tr("Wird im Tagebuch vermerkt, z. B. „Einschlafübung“ oder „Stressbewältigung“."));
    ev->addWidget(m_event, 1);
    lay->addLayout(ev);

    // Signaltöne
    auto *sig = new QGroupBox(tr("Signaltöne"));
    auto *sg = new QGridLayout(sig);
    m_signal = new QCheckBox(tr("Signalton (Beep) beim Phasenwechsel – so können Sie die Augen schließen"));
    sg->addWidget(m_signal, 0, 0, 1, 3);
    m_distinct = new QCheckBox(tr("unterschiedliche Töne für Ein-, Ausatmen und Halten"));
    sg->addWidget(m_distinct, 1, 0, 1, 3);
    m_gong = new QCheckBox(tr("Gong am Ende der Übung"));
    sg->addWidget(m_gong, 2, 0, 1, 3);
    sg->addWidget(new QLabel(tr("Lautstärke:")), 3, 0);
    m_signalVol = volumeSlider(70);
    sg->addWidget(m_signalVol, 3, 1);
    auto *test = new QPushButton(Icons::get(QStringLiteral("volume")), tr("Probe"));
    connect(test, &QPushButton::clicked, this, [this] {
        Tones::play(m_distinct->isChecked() ? Tones::Kind::Inhale : Tones::Kind::Phase, m_signalVol->value());
    });
    sg->addWidget(test, 3, 2);
    auto *sigHint = new QLabel(tr("Während der Übung schalten Sie die Töne mit der Schaltfläche „Töne“ oder der Taste M ab."));
    sigHint->setWordWrap(true);
    sigHint->setEnabled(false);
    sg->addWidget(sigHint, 4, 0, 1, 3);
    m_vibrate = new QCheckBox(tr("Vibrationsimpuls (Android-Geräte)"));
    m_vibrate->setVisible(Tones::vibrationAvailable());
    sg->addWidget(m_vibrate, 5, 0, 1, 3);
    sg->setColumnStretch(1, 1);
    auto syncSignal = [this] {
        m_distinct->setEnabled(m_signal->isChecked());
        m_signalVol->setEnabled(m_signal->isChecked() || m_gong->isChecked());
    };
    connect(m_signal, &QCheckBox::toggled, this, syncSignal);
    connect(m_gong, &QCheckBox::toggled, this, syncSignal);
    lay->addWidget(sig);

    // Hintergrundmusik
    m_musicBox = new QGroupBox(tr("Hintergrundmusik"));
    m_musicBox->setCheckable(true);
    auto *mg = new QGridLayout(m_musicBox);
    m_musicFile = new MusicFileEdit;
    mg->addWidget(m_musicFile, 0, 0, 1, 2);
    mg->addWidget(new QLabel(tr("Lautstärke:")), 1, 0);
    m_musicVol = volumeSlider(40);
    mg->addWidget(m_musicVol, 1, 1);
    connect(m_musicVol, &QSlider::valueChanged, m_musicFile, &MusicFileEdit::setPreviewVolume);
    if (!MusicPlayer::available()) {
        m_musicBox->setEnabled(false);
        m_musicBox->setToolTip(tr("Ohne Qt Multimedia übersetzt – keine Musikwiedergabe."));
    }
    lay->addWidget(m_musicBox);

    // Trainingsmodus
    m_trainingBox = new QGroupBox(tr("Trainingsmodus: Messwerte vor und nach der Übung erfassen"));
    m_trainingBox->setCheckable(true);
    auto *trl = new QHBoxLayout(m_trainingBox);
    m_askSpo2 = new QCheckBox(tr("SpO₂"));
    m_askPf = new QCheckBox(tr("Peak Flow"));
    m_askPulse = new QCheckBox(tr("Puls"));
    m_askFeeling = new QCheckBox(tr("Befinden"));
    for (QCheckBox *c : { m_askSpo2, m_askPf, m_askPulse, m_askFeeling }) trl->addWidget(c);
    trl->addStretch();
    lay->addWidget(m_trainingBox);
    lay->addStretch();

    outer->addWidget(scrollable(content), 1);
    auto *bb = new QDialogButtonBox;
    auto *start = bb->addButton(tr("Übung starten"), QDialogButtonBox::AcceptRole);
    start->setIcon(Icons::get(QStringLiteral("play")));
    start->setDefault(true);
    bb->addButton(QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &SessionDialog::startExercise);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(bb);
    return page;
}

void SessionDialog::loadSettings(const ModuleSettings &s)
{
    m_total->setValue(qMax(1, s.totalSec / 60));
    for (int i = 0; i < m_phaseSpins.size(); ++i) m_phaseSpins[i]->setValue(s.phaseSec.value(i));
    for (int i = 0; i < m_repeatSpins.size(); ++i)
        if (m_repeatSpins[i]) m_repeatSpins[i]->setValue(s.repeats.value(i, 1));
    m_event->setCurrentText(s.eventName);
    m_signal->setChecked(s.signal);
    m_gong->setChecked(s.doneGong);
    m_distinct->setChecked(s.distinctTones);
    m_distinct->setEnabled(s.signal);
    m_signalVol->setValue(s.signalVolume);
    m_signalVol->setEnabled(s.signal || s.doneGong);
    m_vibrate->setChecked(s.vibrate);
    m_musicBox->setChecked(s.musicEnabled && MusicPlayer::available());
    m_musicFile->setFile(s.musicFile);
    m_musicVol->setValue(s.musicVolume);
    m_musicFile->setPreviewVolume(s.musicVolume);
    m_trainingBox->setChecked(s.trainingMode);
    m_askSpo2->setChecked(s.askSpo2);
    m_askPf->setChecked(s.askPeakFlow);
    m_askPulse->setChecked(s.askPulse);
    m_askFeeling->setChecked(s.askFeeling);
    if (m_target) m_target->setCurrentIndex(qMax(0, m_target->findData(s.deviceTarget)));
    m_showTips->setChecked(s.showTips);
    m_tips->setVisible(s.showTips);
    updateCycleInfo();
}

ModuleSettings SessionDialog::currentSettings() const
{
    ModuleSettings s = ModuleSettings::defaults(*m_module);
    s.totalSec = m_total->value() * 60;
    for (int i = 0; i < m_phaseSpins.size(); ++i) s.phaseSec[i] = m_phaseSpins.at(i)->value();
    for (int i = 0; i < m_repeatSpins.size(); ++i)
        if (m_repeatSpins.at(i)) s.repeats[i] = m_repeatSpins.at(i)->value();
    s.eventName = m_event->currentText().trimmed();
    s.signal = m_signal->isChecked();
    s.doneGong = m_gong->isChecked();
    s.distinctTones = m_distinct->isChecked();
    s.signalVolume = m_signalVol->value();
    s.vibrate = m_vibrate->isChecked();
    s.musicEnabled = m_musicBox->isChecked();
    s.musicFile = m_musicFile->file();
    s.musicVolume = m_musicVol->value();
    s.trainingMode = m_trainingBox->isChecked();
    s.askSpo2 = m_askSpo2->isChecked();
    s.askPeakFlow = m_askPf->isChecked();
    s.askPulse = m_askPulse->isChecked();
    s.askFeeling = m_askFeeling->isChecked();
    s.showTips = m_showTips->isChecked();
    if (m_target) s.deviceTarget = m_target->currentData().toInt();
    if (m_module->singleRound) s.totalSec = 10;   // endet nach der einen Runde mit allen Wiederholungen
    return s;
}

void SessionDialog::updateCycleInfo()
{
    if (!m_cycleInfo) return;      // noch im Aufbau
    ModuleSettings st = ModuleSettings::defaults(*m_module);
    for (int i = 0; i < m_phaseSpins.size(); ++i)
        if (m_phaseSpins.at(i)) st.phaseSec[i] = m_phaseSpins.at(i)->value();
    for (int i = 0; i < m_repeatSpins.size(); ++i)
        if (m_repeatSpins.at(i)) st.repeats[i] = m_repeatSpins.at(i)->value();
    const double round = st.roundSec(*m_module);
    if (round <= 0) {
        m_cycleInfo->setText(tr("Bitte mindestens eine Phase einstellen."));
        return;
    }
    const int rounds = qCeil(m_total->value() * 60 / round);
    if (m_module->singleRound) {
        m_cycleInfo->setText(tr("Dauer der Übung: etwa %1").arg(formatDuration(int(round + 0.5))));
        return;
    }
    if (m_module->hasSegments()) {
        m_cycleInfo->setText(tr("Eine Runde: %1 (ohne Abhusten) · etwa %2 Runden").arg(formatDuration(int(round + 0.5))).arg(rounds));
        return;
    }
    const double perMin = 60.0 / round;
    m_cycleInfo->setText(tr("Ein Atemzyklus: %1 s · %2 Atemzüge pro Minute · etwa %3 Zyklen")
                             .arg(formatSeconds(round), QLocale().toString(perMin, 'f', 1))
                             .arg(rounds));
}

// ---------------------------------------------------------------- 2./4. Messungen

QWidget *SessionDialog::buildMeasurePage(bool before)
{
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);
    lay->addWidget(heading(before ? tr("Vor der Übung messen") : tr("Nach der Übung messen"), 1.5));
    auto *info = new QLabel(before ? tr("Messen Sie in Ruhe, bevor Sie beginnen. Felder, die Sie nicht messen möchten, "
                                        "lassen Sie einfach leer.")
                                   : tr("Bleiben Sie noch einen Moment ruhig sitzen und messen Sie dann erneut – so "
                                        "lässt sich die Wirkung der Übung im Verlauf vergleichen."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *form = new MeasureForm;
    form->setHeading({});
    lay->addWidget(form);
    if (!before) {
        lay->addWidget(new QLabel(tr("Notiz (optional):")));
        auto *note = new QPlainTextEdit;
        note->setPlaceholderText(tr("Wie ging es Ihnen bei der Übung?"));
        note->setMaximumHeight(110);
        lay->addWidget(note);
        connect(note, &QPlainTextEdit::textChanged, this, [this, note] { m_note->setPlainText(note->toPlainText()); });
    }
    lay->addStretch();
    outer->addWidget(scrollable(content), 1);
    auto *bb = new QDialogButtonBox;
    auto *next = bb->addButton(before ? tr("Weiter zur Übung") : tr("Weiter"), QDialogButtonBox::AcceptRole);
    next->setDefault(true);
    auto *skip = bb->addButton(tr("Überspringen"), QDialogButtonBox::ResetRole);
    outer->addWidget(bb);
    if (before) {
        m_before = form;
        connect(next, &QPushButton::clicked, this, [this] {
            m_rec.spo2Before = m_before->spo2();
            m_rec.pfBefore = m_before->peakFlow();
            m_rec.pulseBefore = m_before->pulse();
            m_rec.feelingBefore = m_before->feeling();
            beginRun();
        });
        connect(skip, &QPushButton::clicked, this, &SessionDialog::beginRun);
    } else {
        m_after = form;
        connect(next, &QPushButton::clicked, this, [this] {
            m_rec.spo2After = m_after->spo2();
            m_rec.pfAfter = m_after->peakFlow();
            m_rec.pulseAfter = m_after->pulse();
            m_rec.feelingAfter = m_after->feeling();
            showSummary();
        });
        connect(skip, &QPushButton::clicked, this, &SessionDialog::showSummary);
    }
    return page;
}

// ---------------------------------------------------------------- 3. Übung

QWidget *SessionDialog::buildRunPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    m_runTop = new QWidget;
    auto *top = new QHBoxLayout(m_runTop);
    top->setContentsMargins(0, 0, 0, 0);
    m_runTitle = new QLabel(m_module->shortName);
    QFont f = m_runTitle->font();
    f.setBold(true);
    m_runTitle->setFont(f);
    top->addWidget(m_runTitle);
    top->addStretch();
    m_runInfo = new QLabel;
    top->addWidget(m_runInfo);
    lay->addWidget(m_runTop);

    if (!m_module->emergencyNote.isEmpty()) lay->addWidget(emergencyLabel(m_module->emergencyNote));
    if (!m_module->posture.isEmpty()) {
        auto *row = new QHBoxLayout;
        row->addStretch();
        if (m_module->quickHelp && !m_module->postureImage.isEmpty()) {
            auto *img = new QLabel;
            img->setPixmap(Icons::illustration(m_module->postureImage, QSize(120, 90)));
            row->addWidget(img);
        }
        auto *posture = new QLabel(tr("<b>Haltung:</b> %1").arg(m_module->posture.toHtmlEscaped()));
        posture->setWordWrap(true);
        row->addWidget(posture);
        row->addStretch();
        lay->addLayout(row);
    }
    m_view = new BreathView;
    m_view->setEngine(m_engine);
    lay->addWidget(m_view, 1);

    m_runBar = new QWidget;
    auto *bar = new QHBoxLayout(m_runBar);
    bar->setContentsMargins(0, 0, 0, 0);
    m_pauseBtn = new QPushButton(Icons::get(QStringLiteral("pause")), tr("Pause"));
    m_pauseBtn->setToolTip(tr("Pause / Fortsetzen (Leertaste)"));
    m_stopBtn = new QPushButton(Icons::get(QStringLiteral("stop")), tr("Beenden"));
    m_stopBtn->setToolTip(tr("Übung vorzeitig beenden (Esc)"));
    m_fullBtn = new QPushButton(Icons::get(QStringLiteral("fullscreen")), tr("Vollbild"));
    m_fullBtn->setToolTip(tr("Vollbild ein/aus (F11)"));
    m_nextBtn = new QPushButton(Icons::get(QStringLiteral("play")), tr("Weiter"));
    m_nextBtn->setToolTip(tr("Nächste Runde beginnen (Leertaste)"));
    m_nextBtn->setVisible(false);
    QFont nb = m_nextBtn->font();
    nb.setBold(true);
    m_nextBtn->setFont(nb);
    bar->addWidget(m_nextBtn);
    bar->addWidget(m_pauseBtn);
    bar->addWidget(m_stopBtn);
    bar->addStretch();
    m_soundBtn = new QPushButton(Icons::get(QStringLiteral("volume")), tr("Töne"));
    m_soundBtn->setCheckable(true);
    m_soundBtn->setToolTip(tr("Signaltöne (Beeps) und Gong ein/aus (M)"));
    bar->addWidget(m_soundBtn);
    bar->addSpacing(12);
    connect(m_soundBtn, &QPushButton::toggled, this, &SessionDialog::setSoundOn);
    auto *mv = m_runMusicIcon = new QLabel;
    mv->setPixmap(Icons::get(QStringLiteral("music")).pixmap(18, 18));
    mv->setToolTip(tr("Lautstärke der Hintergrundmusik"));
    bar->addWidget(mv);
    m_runMusicVol = volumeSlider(40);
    m_runMusicVol->setMaximumWidth(160);
    bar->addWidget(m_runMusicVol);
    bar->addWidget(m_fullBtn);
    lay->addWidget(m_runBar);

    connect(m_pauseBtn, &QPushButton::clicked, this, &SessionDialog::togglePause);
    connect(m_nextBtn, &QPushButton::clicked, this, [this] { m_engine->proceed(); });
    connect(m_stopBtn, &QPushButton::clicked, this, [this] { m_engine->stop(); });
    connect(m_fullBtn, &QPushButton::clicked, this, &SessionDialog::toggleFullScreen);
    connect(m_runMusicVol, &QSlider::valueChanged, this, [this](int v) {
        m_music->setVolume(v);
        m_settings.musicVolume = v;
    });
    return page;
}

void SessionDialog::startExercise()
{
    m_settings = currentSettings();
    if (m_settings.cycleSec() <= 0) {
        QMessageBox::information(this, windowTitle(), tr("Bitte mindestens eine Phase mit einer Dauer einstellen."));
        return;
    }
    if (m_settings.musicEnabled && !m_settings.musicFile.isEmpty() && !QFileInfo::exists(m_settings.musicFile)) {
        if (QMessageBox::question(this, windowTitle(), tr("Die Musikdatei wurde nicht gefunden:\n%1\n\nOhne Musik fortfahren?")
                                                           .arg(m_settings.musicFile)) != QMessageBox::Yes)
            return;
        m_settings.musicEnabled = false;
    }
    m_settings.save(m_userId, *m_module);
    Database::instance().setUserSetting(m_userId, QStringLiteral("ui/lastModule"), m_module->id);

    m_rec = SessionRecord();
    m_rec.userId = m_userId;
    m_rec.moduleId = m_module->id;
    m_rec.eventName = m_settings.eventName;
    m_rec.plannedSec = m_module->singleRound ? int(m_settings.roundSec(*m_module) + 0.5) : m_settings.totalSec;
    m_rec.pattern = m_settings.pattern();

    if (m_settings.trainingMode) {
        m_before->setVisibleFields(m_settings.askSpo2, m_settings.askPeakFlow, m_settings.askPulse, m_settings.askFeeling);
        m_after->setVisibleFields(m_settings.askSpo2, m_settings.askPeakFlow, m_settings.askPulse, m_settings.askFeeling);
        m_stack->setCurrentIndex(Before);
    } else {
        beginRun();
    }
}

void SessionDialog::beginRun()
{
    m_engine->setup(m_settings.round(*m_module), m_settings.totalSec, m_module->quickHelp ? 1 : 3);
    m_view->setModule(m_module, m_settings.phaseSec);
    m_view->setDeviceTarget(m_settings.deviceTarget);
    m_runMusicVol->setValue(m_settings.musicVolume);
    m_runMusicVol->setVisible(m_settings.musicEnabled);
    m_runMusicIcon->setVisible(m_settings.musicEnabled);
    {
        QSignalBlocker b(m_soundBtn);
        m_soundBtn->setChecked(m_settings.signal || m_settings.doneGong);
        m_soundBtn->setIcon(Icons::get(m_soundBtn->isChecked() ? QStringLiteral("volume") : QStringLiteral("mute")));
    }
    m_rec.started = QDateTime::currentDateTime();
    m_stack->setCurrentIndex(Run);
    m_pauseBtn->setText(tr("Pause"));
    m_pauseBtn->setIcon(Icons::get(QStringLiteral("pause")));
    if (m_settings.musicEnabled && !m_settings.musicFile.isEmpty()) {
        m_music->setPlaylist({ m_settings.musicFile });
        m_music->setVolume(0);
        m_music->play();
        m_music->fadeTo(m_settings.musicVolume, 4000);
    }
    m_engine->start();
    m_view->setFocus();
}

void SessionDialog::onPhaseChanged(int, PhaseKind kind)
{
    const bool waiting = kind == PhaseKind::Ready;
    m_nextBtn->setVisible(waiting);
    m_pauseBtn->setEnabled(!waiting);
    if (waiting) m_nextBtn->setFocus();
    if (m_settings.signal)
        Tones::play(m_settings.distinctTones ? toneFor(kind) : Tones::Kind::Phase, m_settings.signalVolume);
    if (m_settings.vibrate) Tones::vibrate(kind == PhaseKind::Inhale ? 160 : 90);
}

void SessionDialog::updateRunInfo()
{
    const auto st = m_engine->state();
    if (st == BreathEngine::State::Preroll) {
        m_runInfo->setText(tr("Start in %1 s").arg(qCeil(m_engine->prerollRemaining())));
        return;
    }
    const int el = int(m_engine->elapsed());
    const int rem = int(qCeil(m_engine->remaining()));
    if (m_module->singleRound) {
        const int perRep = qMax(1, int(m_engine->steps().size()) / qMax(1, m_settings.repeats.value(0, 1)));
        const int total = qMax(1, m_settings.repeats.value(0, 1));
        const int rep = st == BreathEngine::State::Finished ? total : qMin(total, m_engine->phaseIndex() / perRep + 1);
        m_runInfo->setText(tr("Wiederholung %1 von %2 · vergangen %3").arg(rep).arg(total).arg(formatDuration(el)));
        return;
    }
    m_runInfo->setText((m_module->hasSegments() ? tr("Runde %1 · vergangen %2 · noch %3") : tr("Zyklus %1 · vergangen %2 · noch %3"))
                           .arg(m_engine->cycles() + (st == BreathEngine::State::Finished ? 0 : 1))
                           .arg(formatDuration(el), formatDuration(rem)));
}

void SessionDialog::togglePause()
{
    if (m_engine->state() == BreathEngine::State::Paused) {
        m_engine->resume();
        m_music->resume();
        m_pauseBtn->setText(tr("Pause"));
        m_pauseBtn->setIcon(Icons::get(QStringLiteral("pause")));
    } else if (m_engine->state() == BreathEngine::State::Running || m_engine->state() == BreathEngine::State::Preroll) {
        m_engine->pause();
        m_music->pause();
        m_pauseBtn->setText(tr("Fortsetzen"));
        m_pauseBtn->setIcon(Icons::get(QStringLiteral("play")));
    }
}

void SessionDialog::setSoundOn(bool on)
{
    m_settings.signal = on;
    m_settings.doneGong = on;
    m_soundBtn->setIcon(Icons::get(on ? QStringLiteral("volume") : QStringLiteral("mute")));
    // auch für das nächste Mal merken und in der Vorbereitung anzeigen
    m_signal->setChecked(on);
    m_gong->setChecked(on);
    m_settings.save(m_userId, *m_module);
    m_view->setFocus();
}

void SessionDialog::toggleFullScreen()
{
    if (isFullScreen()) showNormal();
    else showFullScreen();
}

void SessionDialog::onFinished(bool completed)
{
    m_rec.actualSec = int(m_engine->elapsed() + 0.5);
    m_rec.cycles = m_engine->cycles();
    if (m_module->singleRound) {           // erledigte Wiederholungen
        const int total = qMax(1, m_settings.repeats.value(0, 1));
        const int perRep = qMax(1, int(m_engine->steps().size()) / total);
        m_rec.cycles = completed ? total : m_engine->phaseIndex() / perRep;
    }
    m_rec.completed = completed;
    m_nextBtn->setVisible(false);
    m_music->fadeTo(0, completed ? 5000 : 1500, true);
    if (completed && m_settings.doneGong) Tones::play(Tones::Kind::Done, m_settings.signalVolume);
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    QTimer::singleShot(completed ? 2500 : 300, this, [this] {
        if (isFullScreen()) showNormal();
        m_pauseBtn->setEnabled(true);
        m_stopBtn->setEnabled(true);
        if (m_settings.trainingMode) {
            m_after->setValues(kNoValue, kNoValue, kNoValue, kNoValue);
            m_stack->setCurrentIndex(After);
        } else {
            showSummary();
        }
    });
}

// ---------------------------------------------------------------- 5. Zusammenfassung

QWidget *SessionDialog::buildSummaryPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->addWidget(heading(tr("Zusammenfassung"), 1.5));
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    lay->addWidget(m_summary);
    if (m_module->deviceTraining) {
        auto *res = new QGroupBox(tr("Ergebnis mit dem Atemtrainer"));
        auto *rf = new QFormLayout(res);
        m_devBalls = new QSpinBox;
        m_devBalls->setRange(-1, 3);
        m_devBalls->setSpecialValueText(QStringLiteral("–"));
        m_devBalls->setSuffix(tr(" Bälle"));
        connect(m_devBalls, QOverload<int>::of(&QSpinBox::valueChanged), m_devBalls,
                [sb = m_devBalls](int v) { sb->setSuffix(v == 1 ? tr(" Ball") : tr(" Bälle")); });
        rf->addRow(tr("Meist erreicht:"), m_devBalls);
        m_devHold = new QSpinBox;
        m_devHold->setRange(-1, 60);
        m_devHold->setSpecialValueText(QStringLiteral("–"));
        m_devHold->setSuffix(tr(" s"));
        rf->addRow(tr("Längste Haltezeit:"), m_devHold);
        lay->addWidget(res);
    }
    lay->addWidget(new QLabel(tr("Notiz:")));
    m_note = new QPlainTextEdit;
    m_note->setPlaceholderText(tr("Wie ging es Ihnen bei der Übung?"));
    m_note->setMaximumHeight(120);
    lay->addWidget(m_note);
    lay->addStretch();
    auto *bb = new QDialogButtonBox;
    auto *save = bb->addButton(tr("Im Tagebuch speichern"), QDialogButtonBox::AcceptRole);
    save->setDefault(true);
    save->setIcon(Icons::get(QStringLiteral("history")));
    auto *discard = bb->addButton(tr("Verwerfen"), QDialogButtonBox::DestructiveRole);
    auto *again = bb->addButton(tr("Noch einmal"), QDialogButtonBox::ResetRole);
    connect(save, &QPushButton::clicked, this, [this] {
        m_rec.note = m_note->toPlainText().trimmed();
        if (m_devBalls) {
            m_rec.deviceBalls = m_devBalls->value() >= 0 ? m_devBalls->value() : kNoValue;
            m_rec.deviceHoldSec = m_devHold->value() >= 0 ? m_devHold->value() : kNoValue;
        }
        QString err;
        if (!Database::instance().saveSession(m_rec, &err)) {
            QMessageBox::warning(this, windowTitle(), tr("Speichern fehlgeschlagen: %1").arg(err));
            return;
        }
        m_saved = true;
        accept();
    });
    connect(discard, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, windowTitle(), tr("Diese Übungseinheit nicht im Tagebuch speichern?"))
            == QMessageBox::Yes)
            QDialog::reject();
    });
    connect(again, &QPushButton::clicked, this, [this] {
        m_note->clear();
        m_stack->setCurrentIndex(Prep);
    });
    lay->addWidget(bb);
    return page;
}

void SessionDialog::showSummary()
{
    auto val = [](int v, const QString &unit) { return v >= 0 ? QString::number(v) + unit : QStringLiteral("–"); };
    QString html = QStringLiteral("<table cellspacing='4'>");
    auto row = [&html](const QString &k, const QString &v) {
        html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(k.toHtmlEscaped(), v.toHtmlEscaped());
    };
    row(tr("Übung:"), m_module->name);
    if (!m_rec.eventName.isEmpty()) row(tr("Anlass:"), m_rec.eventName);
    row(tr("Muster:"), m_rec.pattern + tr(" Sekunden"));
    row(tr("Dauer:"), tr("%1 von geplant %2").arg(formatDuration(m_rec.actualSec), formatDuration(m_rec.plannedSec)));
    row(m_module->singleRound ? tr("Wiederholungen:") : tr("Atemzyklen:"), QString::number(m_rec.cycles));
    row(tr("Status:"), m_rec.completed ? tr("vollständig") : tr("vorzeitig beendet"));
    html += QStringLiteral("</table>");
    if (m_rec.hasMeasurements() || m_rec.feelingBefore >= 0 || m_rec.feelingAfter >= 0) {
        html += QStringLiteral("<p><b>%1</b></p><table cellspacing='4' border='0'><tr><th></th><th>%2</th><th>%3</th></tr>")
                    .arg(tr("Messwerte"), tr("vorher"), tr("nachher"));
        auto mrow = [&](const QString &k, int a, int b, const QString &u) {
            if (a < 0 && b < 0) return;
            html += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td align='right'>%3</td></tr>")
                        .arg(k, val(a, u), val(b, u));
        };
        mrow(tr("SpO₂"), m_rec.spo2Before, m_rec.spo2After, QStringLiteral(" %"));
        mrow(tr("Peak Flow"), m_rec.pfBefore, m_rec.pfAfter, QStringLiteral(" l/min"));
        mrow(tr("Puls"), m_rec.pulseBefore, m_rec.pulseAfter, QStringLiteral(" /min"));
        mrow(tr("Befinden"), m_rec.feelingBefore, m_rec.feelingAfter, QStringLiteral("/10"));
        html += QStringLiteral("</table>");
    }
    m_summary->setText(html);
    if (m_devBalls && m_devBalls->value() < 0) {
        m_devBalls->setValue(m_settings.deviceTarget);   // Vorschlag: das Ziel
        m_devHold->setValue(qRound(m_settings.phaseSec.value(2, 3)));
    }
    m_stack->setCurrentIndex(Summary);
}

// ---------------------------------------------------------------- Tastatur, Schließen

void SessionDialog::keyPressEvent(QKeyEvent *e)
{
    if (m_stack->currentIndex() == Run) {
        if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (m_engine->state() == BreathEngine::State::Waiting) m_engine->proceed();
            else if (e->key() == Qt::Key_Space) togglePause();
            return;
        }
        if (e->key() == Qt::Key_F11) { toggleFullScreen(); return; }
        if (e->key() == Qt::Key_M) { m_soundBtn->toggle(); return; }
        if (e->key() == Qt::Key_Escape) {
            if (isFullScreen()) { showNormal(); return; }
            m_engine->stop();
            return;
        }
    }
    QDialog::keyPressEvent(e);
}

void SessionDialog::reject()
{
    if (m_stack->currentIndex() == Run) {
        m_engine->stop();       // → Zusammenfassung
        return;
    }
    if (m_stack->currentIndex() == After || m_stack->currentIndex() == Summary) {
        if (QMessageBox::question(this, windowTitle(), tr("Fenster schließen, ohne die Übungseinheit im Tagebuch zu speichern?"))
            != QMessageBox::Yes)
            return;
    }
    m_music->stop();
    QDialog::reject();
}
