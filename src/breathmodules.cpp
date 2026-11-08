// SPDX-License-Identifier: MIT
#include "breathmodules.h"
#include "database.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

namespace {
// Übersetzungskontext "BreathModules" (für lupdate erkennbar)
struct BM { Q_DECLARE_TR_FUNCTIONS(BreathModules) };
}

QString phaseName(PhaseKind k)
{
    switch (k) {
    case PhaseKind::Inhale:  return BM::tr("Einatmen");
    case PhaseKind::HoldIn:  return BM::tr("Luft anhalten");
    case PhaseKind::Exhale:  return BM::tr("Ausatmen");
    case PhaseKind::HoldOut: return BM::tr("Halten");
    case PhaseKind::Calm:    return BM::tr("Ruhig atmen");
    case PhaseKind::Huff:    return BM::tr("Huffen");
    case PhaseKind::Ready:   return BM::tr("Abhusten bei Bedarf");
    }
    return {};
}

QString formatSeconds(double s)
{
    return QLocale().toString(s, 'f', qFuzzyCompare(s, double(qRound(s))) ? 0 : 1);
}

QString formatDuration(int sec)
{
    return QStringLiteral("%1:%2 min").arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'));
}

QList<SegmentDef> ModuleDef::effectiveSegments() const
{
    if (!segments.isEmpty()) return segments;
    SegmentDef s;
    s.firstPhase = 0;
    s.phaseCount = phases.size();
    return { s };
}

QString ModuleDef::defaultPattern() const
{
    QStringList l;
    for (const PhaseDef &p : phases) l << formatSeconds(p.defaultSec);
    return l.join(QLatin1Char('-'));
}

namespace BreathModules {

QString relaxCategory() { return BM::tr("Entspannung und Schlaf"); }
QString airwayCategory() { return BM::tr("Atemwege – Asthma und COPD"); }

const QList<ModuleDef> &all()
{
    static const QList<ModuleDef> modules = [] {
        QList<ModuleDef> l;

        // ------------------------------------------------ Entspannung und Schlaf
        // 1. Box-Atmen
        ModuleDef box;
        box.id = QStringLiteral("box");
        box.name = BM::tr("Box-Atmen (4-4-4-4)");
        box.shortName = BM::tr("Box-Atmen");
        box.category = relaxCategory();
        box.description = BM::tr("Der Klassiker zur schnellen Stressreduzierung: Einatmen, Halten, Ausatmen und "
                                 "wieder Halten – vier gleich lange Phasen wie die Seiten eines Quadrats.");
        box.tips = QStringList{ BM::tr("Folgen Sie dem Punkt: Er wandert im Uhrzeigersinn von Ecke zu Ecke – jede Seite ist eine Phase."),
                                BM::tr("Atmen Sie ruhig durch die Nase ein und langsam wieder aus."),
                                BM::tr("Das Halten soll entspannt sein – lassen Sie Schultern und Kiefer locker.") };
        box.phases = { { PhaseKind::Inhale, 4, 2, 10, BM::tr("ruhig durch die Nase") },
                       { PhaseKind::HoldIn, 4, 0, 10, BM::tr("Luft entspannt halten") },
                       { PhaseKind::Exhale, 4, 2, 10, BM::tr("langsam ausatmen") },
                       { PhaseKind::HoldOut, 4, 0, 10, BM::tr("leer und entspannt halten") } };
        box.visual = ModuleDef::Visual::Box;
        box.icon = QStringLiteral("box");
        box.defaultEvent = BM::tr("Stressbewältigung");
        l << box;

        // 2. 4-7-8 – langes Ausatmen
        ModuleDef r;
        r.id = QStringLiteral("four_seven_eight");
        r.name = BM::tr("Langes Ausatmen (4-7-8)");
        r.shortName = BM::tr("4-7-8-Atmung");
        r.category = relaxCategory();
        r.description = BM::tr("„Langes Ausatmen“ oder „Entspannung“: 4 Sekunden einatmen, 7 Sekunden halten, "
                               "8 Sekunden hörbar ausatmen. Beruhigt und hilft beim Einschlafen.");
        r.tips = QStringList{ BM::tr("Der Kreis dehnt sich beim Einatmen aus, bleibt beim Halten stehen und zieht sich beim Ausatmen zusammen."),
                              BM::tr("Einatmen ruhig und tief durch die Nase, ausatmen langsam, vollständig und hörbar durch den Mund – "
                                     "gern mit einem leichten Zischlaut."),
                              BM::tr("Wenn das Halten Stress oder ein Engegefühl auslöst: Phasen verkürzen oder die 4-6-Methode wählen.") };
        r.phases = { { PhaseKind::Inhale, 4, 2, 10, BM::tr("ruhig und tief durch die Nase") },
                     { PhaseKind::HoldIn, 7, 0, 15, BM::tr("Luft in der Lunge ruhen lassen") },
                     { PhaseKind::Exhale, 8, 3, 16, BM::tr("langsam und hörbar durch den Mund") } };
        r.icon = QStringLiteral("circle");
        r.defaultEvent = BM::tr("Entspannung");
        l << r;

        // 3. 4-6 – für Einsteiger
        ModuleDef s;
        s.id = QStringLiteral("four_six");
        s.name = BM::tr("4-6-Methode (für Einsteiger)");
        s.shortName = BM::tr("4-6-Methode");
        s.category = relaxCategory();
        s.description = BM::tr("Die sanfte Alternative ohne Atemanhalten: 4 Sekunden einatmen, 6 Sekunden ausatmen. "
                               "Ideal für 5 bis 10 Minuten im Bett.");
        s.tips = QStringList{ BM::tr("Konzentrieren Sie sich nur darauf, dass das Ausatmen spürbar länger dauert als das Einatmen."),
                              BM::tr("Ein- und Ausatmen entspannt und gleichmäßig durch die Nase."),
                              BM::tr("Gut geeignet für 5 bis 10 Minuten im Bett vor dem Einschlafen.") };
        s.phases = { { PhaseKind::Inhale, 4, 2, 10, BM::tr("entspannt durch die Nase") },
                     { PhaseKind::Exhale, 6, 3, 14, BM::tr("langsam und gleichmäßig durch die Nase") } };
        s.icon = QStringLiteral("wave");
        s.defaultEvent = BM::tr("Einschlafübung");
        l << s;

        // ------------------------------------------------ Atemwege (Asthma, COPD)
        // 4. Atemnot-Hilfe – Schnellstart
        ModuleDef h;
        h.id = QStringLiteral("relief");
        h.name = BM::tr("Atemnot-Hilfe (Lippenbremse im Kutschersitz)");
        h.shortName = BM::tr("Atemnot-Hilfe");
        h.category = airwayCategory();
        h.description = BM::tr("Startet sofort ohne Vorbereitung: eine atemerleichternde Haltung einnehmen und mit der "
                               "Lippenbremse lange ausatmen. Für Momente mit Luftnot – zusätzlich zu Ihrem ärztlichen Notfallplan.");
        h.tips = QStringList{ BM::tr("Zuerst das Bedarfs- bzw. Notfallspray so anwenden, wie es Ihr ärztlicher Plan vorsieht."),
                              BM::tr("Kutschersitz: Oberkörper nach vorn neigen, Unterarme auf die Oberschenkel stützen. "
                                     "Im Stehen: Hände auf Tisch, Fensterbank oder Geländer stützen (Torwarthaltung)."),
                              BM::tr("Kurz durch die Nase einatmen, dann lange gegen die locker aufeinanderliegenden Lippen ausatmen – nicht pressen."),
                              BM::tr("Bleiben Sie so lange in der Haltung, bis sich die Atmung beruhigt.") };
        h.phases = { { PhaseKind::Inhale, 2, 1, 5, BM::tr("kurz durch die Nase") },
                     { PhaseKind::Exhale, 4, 2, 10, BM::tr("lange mit der Lippenbremse") } };
        h.defaultTotalSec = 180;
        h.icon = QStringLiteral("lungs");
        h.defaultEvent = BM::tr("Atemnot");
        h.posture = BM::tr("Kutschersitz: Oberkörper nach vorn, Unterarme auf den Oberschenkeln – oder im Stehen abstützen.");
        h.emergencyNote = BM::tr("Bedarfs- bzw. Notfallspray nach Ihrem ärztlichen Plan anwenden. Wird die Atemnot "
                                 "nicht besser oder schlimmer: sofort Notruf 112.");
        h.quickHelp = true;
        h.airway = true;
        l << h;

        // 5. Lippenbremse
        ModuleDef p;
        p.id = QStringLiteral("pursed_lip");
        p.name = BM::tr("Lippenbremse (dosiertes Ausatmen)");
        p.shortName = BM::tr("Lippenbremse");
        p.category = airwayCategory();
        p.description = BM::tr("Die Standardtechnik der Atemphysiotherapie: Ausatmen gegen den leichten Widerstand der "
                               "Lippen hält die Atemwege länger offen. Hilft bei Atemnot und unter Belastung.");
        p.tips = QStringList{ BM::tr("Durch die Nase einatmen, dann durch die locker aufeinanderliegenden Lippen ausatmen – "
                                     "als wollten Sie eine Kerzenflamme nur zum Flackern bringen."),
                              BM::tr("Die Wangen dürfen sich leicht blähen, aber nicht pressen: Das Ausatmen soll ruhig und lang sein."),
                              BM::tr("Ausatmen etwa doppelt so lang wie einatmen. Üben Sie regelmäßig, damit Sie die Lippenbremse "
                                     "auch beim Treppensteigen und anderen Belastungen ganz automatisch einsetzen.") };
        p.phases = { { PhaseKind::Inhale, 2, 1, 6, BM::tr("ruhig durch die Nase") },
                     { PhaseKind::Exhale, 4, 2, 12, BM::tr("lange durch die locker geschlossenen Lippen") } };
        p.icon = QStringLiteral("lips");
        p.defaultEvent = BM::tr("Atemtraining");
        p.posture = BM::tr("Aufrecht sitzen, Schultern locker.");
        p.airway = true;
        l << p;

        // 6. Bauch- bzw. Zwerchfellatmung
        ModuleDef d;
        d.id = QStringLiteral("diaphragm");
        d.name = BM::tr("Bauchatmung (Zwerchfellatmung)");
        d.shortName = BM::tr("Bauchatmung");
        d.category = airwayCategory();
        d.description = BM::tr("Atmen mit dem Zwerchfell statt mit der Atemhilfsmuskulatur von Hals und Brust: braucht "
                               "weniger Kraft und entlastet. Ausatmen gern mit der Lippenbremse.");
        d.tips = QStringList{ BM::tr("Legen Sie eine Hand auf den Bauch und eine auf die Brust. Beim Einatmen hebt sich der Bauch, "
                                     "die Brust bleibt möglichst ruhig."),
                              BM::tr("Im Liegen mit angewinkelten Beinen fällt die Bauchatmung am Anfang leichter."),
                              BM::tr("Schultern nicht hochziehen. Ausatmen ruhig mit der Lippenbremse – der Bauch senkt sich.") };
        d.phases = { { PhaseKind::Inhale, 4, 2, 8, BM::tr("durch die Nase – der Bauch hebt sich unter der Hand") },
                     { PhaseKind::Exhale, 6, 3, 12, BM::tr("mit der Lippenbremse – der Bauch senkt sich") } };
        d.icon = QStringLiteral("belly");
        d.defaultEvent = BM::tr("Atemtraining");
        d.posture = BM::tr("Bequem sitzen oder auf dem Rücken liegen, Beine angewinkelt.");
        d.airway = true;
        l << d;

        // 7. Ruhige Nasenatmung (Atemschulung gegen Überatmung)
        ModuleDef n;
        n.id = QStringLiteral("nasal_calm");
        n.name = BM::tr("Ruhige Nasenatmung (gegen Überatmung)");
        n.shortName = BM::tr("Ruhige Nasenatmung");
        n.category = airwayCategory();
        n.description = BM::tr("Atemschulung in Anlehnung an Papworth-Methode und Atemschule: langsam, leise und klein durch "
                               "die Nase atmen. Kann bei Asthma Symptome einer Überatmung (Hyperventilation) lindern.");
        n.tips = QStringList{ BM::tr("Mund geschlossen halten, leise und eher flach durch die Nase atmen – keine tiefen Atemzüge."),
                              BM::tr("Ein leichtes Gefühl von „etwas weniger Luft“ ist in Ordnung, Stress oder Luftnot nicht: "
                                     "dann normal weiteratmen."),
                              BM::tr("Diese Übung ersetzt keine Asthma-Medikamente. Sie dient der Entspannung und einem ruhigeren Atemmuster.") };
        n.phases = { { PhaseKind::Inhale, 3, 2, 6, BM::tr("leise und klein durch die Nase") },
                     { PhaseKind::Exhale, 4, 2, 8, BM::tr("entspannt durch die Nase ausströmen lassen") },
                     { PhaseKind::HoldOut, 1, 0, 4, BM::tr("kurze, entspannte Pause") } };
        n.icon = QStringLiteral("nose");
        n.defaultEvent = BM::tr("Atemtraining");
        n.posture = BM::tr("Aufrecht und bequem sitzen, Schultern und Kiefer locker.");
        n.airway = true;
        l << n;

        // 8. ACBT – aktiver Zyklus von Atemtechniken zum Abhusten
        ModuleDef a;
        a.id = QStringLiteral("acbt");
        a.name = BM::tr("Abhusten mit ACBT (aktiver Atemzyklus)");
        a.shortName = BM::tr("ACBT – Abhusten");
        a.category = airwayCategory();
        a.description = BM::tr("„Active Cycle of Breathing Techniques“ zum Lösen und Abhusten von Schleim ohne Hilfsmittel: "
                               "Atemkontrolle, tiefe Atemzüge und Huffen im Wechsel. Nach jeder Runde husten Sie bei Bedarf ab.");
        a.tips = QStringList{ BM::tr("Am besten nach Anleitung durch Ihre Atemphysiotherapie üben. Ein bronchienerweiterndes Spray "
                                     "bzw. Inhalieren nach Ihrem ärztlichen Plan vorher erleichtert das Abhusten."),
                              BM::tr("Huffen: mit offenem Mund und offenem Rachen kräftig aushauchen – als wollten Sie eine "
                                     "Brille anhauchen. Nicht zu kräftig, sonst kann die Atmung pfeifen."),
                              BM::tr("Nur husten, wenn Sie Schleim im Rachen spüren – dauerndes Husten ermüdet und kann die Atemwege reizen."),
                              BM::tr("Ausreichend trinken hilft, den Schleim zu lösen. Nicht direkt vor dem Schlafengehen üben.") };
        a.phases = { { PhaseKind::Calm, 30, 10, 120, BM::tr("ruhig und entspannt atmen, Schultern locker") },
                     { PhaseKind::Inhale, 3, 2, 6, BM::tr("langsam und tief einatmen – die unteren Rippen weiten sich") },
                     { PhaseKind::HoldIn, 3, 0, 5, BM::tr("kurz halten") },
                     { PhaseKind::Exhale, 4, 2, 8, BM::tr("locker ausatmen, nicht pressen") },
                     { PhaseKind::Calm, 20, 5, 60, BM::tr("ruhig und entspannt atmen") },
                     { PhaseKind::Inhale, 2, 1, 4, BM::tr("mittelgroß einatmen") },
                     { PhaseKind::Huff, 2, 1, 4, BM::tr("mit offenem Mund kräftig aushauchen") },
                     { PhaseKind::Calm, 8, 4, 30, BM::tr("ruhig atmen") } };
        SegmentDef s1{ BM::tr("Atemkontrolle"), 0, 1, 1, 1, 1, false, {} };
        SegmentDef s2{ BM::tr("Tiefe Atemzüge"), 1, 3, 3, 1, 5, false, {} };
        SegmentDef s3{ BM::tr("Atemkontrolle"), 4, 1, 1, 1, 1, false, {} };
        SegmentDef s4{ BM::tr("Huffen"), 5, 3, 2, 1, 4, true,
                       BM::tr("Spüren Sie Schleim im Rachen? Jetzt abhusten. Danach mit „Weiter“ (Leertaste) die "
                              "nächste Runde beginnen.") };
        a.segments = { s1, s2, s3, s4 };
        a.defaultTotalSec = 600;
        a.maxTotalSec = 1800;
        a.icon = QStringLiteral("acbt");
        a.defaultEvent = BM::tr("Abhusten");
        a.posture = BM::tr("Aufrecht sitzen, Füße auf dem Boden – oder die Lagerung, die Ihre Physiotherapie empfiehlt.");
        a.airway = true;
        l << a;

        // Weitere Module hier ergänzen …
        return l;
    }();
    return modules;
}

const ModuleDef *find(const QString &id)
{
    for (const ModuleDef &m : all())
        if (m.id == id) return &m;
    return nullptr;
}

QString displayName(const QString &id)
{
    if (const ModuleDef *m = find(id)) return m->shortName;
    return id.isEmpty() ? QStringLiteral("–") : id;
}

QStringList generalTips()
{
    return { BM::tr("Setzen oder legen Sie sich bequem hin und lockern Sie enge Kleidung."),
             BM::tr("Sie können die Augen schließen – ein Signalton kündigt jede neue Phase an."),
             BM::tr("Atmen Sie nie gegen Widerstand. Bei Schwindel, Unwohlsein oder Atemnot brechen Sie die Übung ab."),
             BM::tr("VitaBreathe ist ein Übungs-Tagebuch und kein Medizinprodukt; es ersetzt keine ärztliche Beratung.") };
}

QStringList airwayTips()
{
    return { BM::tr("Besprechen Sie Atemübungen bei Asthma oder COPD mit Ihrer Ärztin, Ihrem Arzt oder Ihrer "
                    "Atemphysiotherapie – sie ergänzen die Behandlung, ersetzen aber keine Medikamente."),
             BM::tr("Brechen Sie ab, wenn Schwindel, Engegefühl oder zunehmende Atemnot auftreten, und handeln Sie "
                    "nach Ihrem Notfallplan."),
             BM::tr("VitaBreathe ist ein Übungs-Tagebuch und kein Medizinprodukt.") };
}

} // namespace BreathModules

// ---------------------------------------------------------------- Einstellungen

ModuleSettings ModuleSettings::defaults(const ModuleDef &m)
{
    ModuleSettings s;
    for (const PhaseDef &p : m.phases) s.phaseSec << p.defaultSec;
    for (const SegmentDef &g : m.effectiveSegments()) s.repeats << g.defaultRepeats;
    s.totalSec = m.defaultTotalSec;
    s.eventName = m.defaultEvent;
    if (m.quickHelp) s.showTips = false;
    return s;
}

ModuleSettings ModuleSettings::load(qint64 userId, const ModuleDef &m)
{
    ModuleSettings s = defaults(m);
    const QJsonObject o = Database::instance().userJson(userId, QStringLiteral("module/") + m.id);
    if (o.isEmpty()) return s;
    const QJsonArray ph = o.value(QStringLiteral("phases")).toArray();
    if (ph.size() == m.phases.size())
        for (int i = 0; i < ph.size(); ++i)
            s.phaseSec[i] = qBound(m.phases.at(i).minSec, ph.at(i).toDouble(), m.phases.at(i).maxSec);
    const QJsonArray rp = o.value(QStringLiteral("repeats")).toArray();
    const QList<SegmentDef> segs = m.effectiveSegments();
    if (rp.size() == segs.size())
        for (int i = 0; i < rp.size(); ++i)
            s.repeats[i] = qBound(segs.at(i).minRepeats, rp.at(i).toInt(), segs.at(i).maxRepeats);
    s.totalSec = qBound(60, o.value(QStringLiteral("total")).toInt(s.totalSec), m.maxTotalSec);
    s.signal = o.value(QStringLiteral("signal")).toBool(s.signal);
    s.distinctTones = o.value(QStringLiteral("distinct")).toBool(s.distinctTones);
    s.signalVolume = o.value(QStringLiteral("signalVolume")).toInt(s.signalVolume);
    s.vibrate = o.value(QStringLiteral("vibrate")).toBool(s.vibrate);
    s.musicEnabled = o.value(QStringLiteral("music")).toBool(false);
    s.musicFile = o.value(QStringLiteral("musicFile")).toString();
    s.musicVolume = o.value(QStringLiteral("musicVolume")).toInt(s.musicVolume);
    s.trainingMode = o.value(QStringLiteral("training")).toBool(false);
    s.askSpo2 = o.value(QStringLiteral("askSpo2")).toBool(true);
    s.askPeakFlow = o.value(QStringLiteral("askPeakFlow")).toBool(true);
    s.askPulse = o.value(QStringLiteral("askPulse")).toBool(true);
    s.askFeeling = o.value(QStringLiteral("askFeeling")).toBool(true);
    s.eventName = o.value(QStringLiteral("event")).toString(s.eventName);
    s.showTips = o.value(QStringLiteral("tips")).toBool(s.showTips);
    return s;
}

void ModuleSettings::save(qint64 userId, const ModuleDef &m) const
{
    QJsonArray ph, rp;
    for (double d : phaseSec) ph << d;
    for (int r : repeats) rp << r;
    Database::instance().setUserJson(userId, QStringLiteral("module/") + m.id, QJsonObject{
        { QStringLiteral("phases"), ph }, { QStringLiteral("repeats"), rp }, { QStringLiteral("total"), totalSec },
        { QStringLiteral("signal"), signal }, { QStringLiteral("distinct"), distinctTones },
        { QStringLiteral("signalVolume"), signalVolume }, { QStringLiteral("vibrate"), vibrate },
        { QStringLiteral("music"), musicEnabled }, { QStringLiteral("musicFile"), musicFile },
        { QStringLiteral("musicVolume"), musicVolume }, { QStringLiteral("training"), trainingMode },
        { QStringLiteral("askSpo2"), askSpo2 }, { QStringLiteral("askPeakFlow"), askPeakFlow },
        { QStringLiteral("askPulse"), askPulse }, { QStringLiteral("askFeeling"), askFeeling },
        { QStringLiteral("event"), eventName }, { QStringLiteral("tips"), showTips } });
}

QString ModuleSettings::pattern() const
{
    QStringList l;
    for (double d : phaseSec) l << formatSeconds(d);
    return l.join(QLatin1Char('-'));
}

double ModuleSettings::cycleSec() const
{
    double t = 0;
    for (double d : phaseSec) t += d;
    return t;
}

QList<StepDef> ModuleSettings::round(const ModuleDef &m) const
{
    QList<StepDef> steps;
    const QList<SegmentDef> segs = m.effectiveSegments();
    for (int g = 0; g < segs.size(); ++g) {
        const SegmentDef &seg = segs.at(g);
        const int reps = qMax(1, repeats.value(g, seg.defaultRepeats));
        for (int r = 0; r < reps; ++r)
            for (int i = seg.firstPhase; i < seg.firstPhase + seg.phaseCount && i < m.phases.size(); ++i) {
                const double sec = phaseSec.value(i);
                if (sec <= 0.05) continue;     // abgeschaltete Phase
                StepDef st;
                st.kind = m.phases.at(i).kind;
                st.sec = sec;
                st.phaseIndex = i;
                st.segment = g;
                st.hint = m.phases.at(i).hint;
                st.segmentTitle = seg.title;
                steps << st;
            }
        if (seg.readyAfter) {
            StepDef st;
            st.kind = PhaseKind::Ready;
            st.sec = 0;
            st.segment = g;
            st.hint = seg.readyHint;
            st.segmentTitle = seg.title;
            steps << st;
        }
    }
    return steps;
}

double ModuleSettings::roundSec(const ModuleDef &m) const
{
    double t = 0;
    for (const StepDef &s : round(m)) t += s.sec;
    return t;
}
