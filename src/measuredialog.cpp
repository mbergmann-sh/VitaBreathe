// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "measuredialog.h"
#include "breathmodules.h"
#include "database.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QSpinBox *optionalSpin(int min, int max, int step, const QString &suffix, int def)
{
    auto *s = new QSpinBox;
    s->setRange(min - 1, max);          // min - 1 = nicht erfasst
    s->setSingleStep(step);
    s->setSpecialValueText(QStringLiteral("–"));
    s->setSuffix(suffix);
    s->setValue(min - 1);
    s->setProperty("vbDefault", def);
    s->setMinimumWidth(110);
    s->setAccelerated(true);
    return s;
}
int readOptional(const QSpinBox *s) { return s->value() == s->minimum() ? kNoValue : s->value(); }
void writeOptional(QSpinBox *s, int v) { s->setValue(v >= 0 ? v : s->minimum()); }
} // namespace

MeasureForm::MeasureForm(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_heading = new QLabel;
    m_heading->setWordWrap(true);
    QFont hf = m_heading->font();
    hf.setBold(true);
    hf.setPointSizeF(hf.pointSizeF() * 1.15);
    m_heading->setFont(hf);
    lay->addWidget(m_heading);
    auto *form = m_form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_spo2 = optionalSpin(70, 100, 1, QStringLiteral(" %"), 97);
    m_pf = optionalSpin(50, 900, 10, QStringLiteral(" l/min"), 450);
    m_pulse = optionalSpin(30, 220, 1, QStringLiteral(" /min"), 70);
    m_feeling = optionalSpin(1, 10, 1, QString(), 6);
    const QString hints[4] = { tr("Sauerstoffsättigung (Pulsoxymeter)"), tr("bester von drei Blasversuchen"),
                               tr("z. B. vom Pulsoxymeter"), tr("1 = sehr schlecht … 10 = sehr gut") };
    QSpinBox *spins[4] = { m_spo2, m_pf, m_pulse, m_feeling };
    const QString labels[4] = { tr("SpO₂:"), tr("Peak Flow:"), tr("Puls:"), tr("Befinden:") };
    for (int i = 0; i < 4; ++i) {
        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(spins[i]);
        auto *hint = new QLabel(hints[i]);
        hint->setEnabled(false);
        h->addWidget(hint, 1);
        form->addRow(labels[i], row);
        m_rows[i] = row;
    }
    lay->addLayout(form);
    auto *note = new QLabel(tr("Leere Felder („–“) werden nicht gespeichert. Pfeiltaste nach oben setzt einen üblichen Startwert."));
    note->setWordWrap(true);
    note->setEnabled(false);
    lay->addWidget(note);
    // Startwert beim ersten Schritt nach oben
    for (QSpinBox *s : spins) {
        connect(s, qOverload<int>(&QSpinBox::valueChanged), s, [s](int v) {
            if (v == s->minimum() + 1 && s->property("vbJumped").toBool() == false) {
                s->setProperty("vbJumped", true);
                s->setValue(s->property("vbDefault").toInt());
            }
        });
    }
}

void MeasureForm::setHeading(const QString &text) { m_heading->setText(text); m_heading->setVisible(!text.isEmpty()); }

void MeasureForm::setVisibleFields(bool spo2, bool peakFlow, bool pulse, bool feeling)
{
    const bool v[4] = { spo2, peakFlow, pulse, feeling };
    for (int i = 0; i < 4; ++i) {
        m_rows[i]->setVisible(v[i]);
        if (QWidget *l = m_form->labelForField(m_rows[i])) l->setVisible(v[i]);
    }
}

void MeasureForm::setValues(int spo2, int pf, int pulse, int feeling)
{
    for (QSpinBox *s : { m_spo2, m_pf, m_pulse, m_feeling }) s->setProperty("vbJumped", true);
    writeOptional(m_spo2, spo2);
    writeOptional(m_pf, pf);
    writeOptional(m_pulse, pulse);
    writeOptional(m_feeling, feeling);
}

int MeasureForm::spo2() const { return readOptional(m_spo2); }
int MeasureForm::peakFlow() const { return readOptional(m_pf); }
int MeasureForm::pulse() const { return readOptional(m_pulse); }
int MeasureForm::feeling() const { return readOptional(m_feeling); }

// ====================================================================== Eintrag bearbeiten

SessionEditDialog::SessionEditDialog(const SessionRecord &s, QWidget *parent) : QDialog(parent), m_rec(s)
{
    setWindowTitle(s.id ? tr("Übungseinheit bearbeiten") : tr("Übungseinheit nachtragen"));
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_module = new QComboBox;
    for (const ModuleDef &m : BreathModules::all()) m_module->addItem(m.name, m.id);
    if (m_module->findData(s.moduleId) < 0 && !s.moduleId.isEmpty()) m_module->addItem(s.moduleId, s.moduleId);
    m_module->setCurrentIndex(qMax(0, m_module->findData(s.moduleId)));
    form->addRow(tr("Übung:"), m_module);
    m_event = new QComboBox;
    m_event->setEditable(true);
    m_event->addItems(Database::instance().eventNames(s.userId));
    m_event->setCurrentText(s.eventName);
    form->addRow(tr("Anlass:"), m_event);
    m_started = new QDateTimeEdit(s.started.isValid() ? s.started : QDateTime::currentDateTime());
    m_started->setCalendarPopup(true);
    m_started->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm"));
    form->addRow(tr("Beginn:"), m_started);
    m_minutes = new QSpinBox;
    m_minutes->setRange(1, 180);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(qMax(1, (s.actualSec ? s.actualSec : 300) / 60));
    form->addRow(tr("Dauer:"), m_minutes);
    m_cycles = new QSpinBox;
    m_cycles->setRange(0, 999);
    m_cycles->setValue(s.cycles);
    form->addRow(tr("Atemzyklen bzw. Wiederholungen:"), m_cycles);
    auto *dev = new QHBoxLayout;
    m_devBalls = new QSpinBox;
    m_devBalls->setRange(-1, 3);
    m_devBalls->setSpecialValueText(QStringLiteral("–"));
    m_devBalls->setSuffix(tr(" Bälle"));
        connect(m_devBalls, QOverload<int>::of(&QSpinBox::valueChanged), m_devBalls,
                [sb = m_devBalls](int v) { sb->setSuffix(v == 1 ? tr(" Ball") : tr(" Bälle")); });
    m_devBalls->setValue(s.deviceBalls >= 0 ? s.deviceBalls : -1);
    m_devHold = new QSpinBox;
    m_devHold->setRange(-1, 60);
    m_devHold->setSpecialValueText(QStringLiteral("–"));
    m_devHold->setSuffix(tr(" s gehalten"));
    m_devHold->setValue(s.deviceHoldSec >= 0 ? s.deviceHoldSec : -1);
    dev->addWidget(m_devBalls);
    dev->addWidget(m_devHold);
    dev->addStretch();
    form->addRow(tr("Atemtrainer:"), dev);
    lay->addLayout(form);

    auto *meas = new QHBoxLayout;
    auto *gb = new QGroupBox(tr("Vor der Übung"));
    auto *gbl = new QVBoxLayout(gb);
    m_before = new MeasureForm;
    m_before->setHeading({});
    m_before->setValues(s.spo2Before, s.pfBefore, s.pulseBefore, s.feelingBefore);
    gbl->addWidget(m_before);
    meas->addWidget(gb);
    auto *ga = new QGroupBox(tr("Nach der Übung"));
    auto *gal = new QVBoxLayout(ga);
    m_after = new MeasureForm;
    m_after->setHeading({});
    m_after->setValues(s.spo2After, s.pfAfter, s.pulseAfter, s.feelingAfter);
    gal->addWidget(m_after);
    meas->addWidget(ga);
    lay->addLayout(meas);
    lay->addWidget(new QLabel(tr("Notiz:")));
    m_note = new QPlainTextEdit(s.note);
    m_note->setMaximumHeight(90);
    lay->addWidget(m_note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

SessionRecord SessionEditDialog::record() const
{
    SessionRecord s = m_rec;
    const QString newModule = m_module->currentData().toString();
    if (newModule != s.moduleId) {
        s.moduleId = newModule;
        if (const ModuleDef *m = BreathModules::find(newModule)) s.pattern = m->defaultPattern();
    }
    s.eventName = m_event->currentText().trimmed();
    s.started = m_started->dateTime();
    const int sec = m_minutes->value() * 60;
    if (s.actualSec / 60 != m_minutes->value()) s.actualSec = sec;
    if (!s.plannedSec) s.plannedSec = sec;
    if (!s.id) s.completed = true;
    s.cycles = m_cycles->value();
    s.spo2Before = m_before->spo2();
    s.pfBefore = m_before->peakFlow();
    s.pulseBefore = m_before->pulse();
    s.feelingBefore = m_before->feeling();
    s.spo2After = m_after->spo2();
    s.pfAfter = m_after->peakFlow();
    s.pulseAfter = m_after->pulse();
    s.feelingAfter = m_after->feeling();
    s.note = m_note->toPlainText().trimmed();
    s.deviceBalls = m_devBalls->value() >= 0 ? m_devBalls->value() : kNoValue;
    s.deviceHoldSec = m_devHold->value() >= 0 ? m_devHold->value() : kNoValue;
    return s;
}
