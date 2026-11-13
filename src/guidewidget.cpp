// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "guidewidget.h"
#include "icons.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

GuideWidget::GuideWidget(const QList<GuideStep> &steps, int imageWidth, QWidget *parent)
    : QWidget(parent), m_imageWidth(imageWidth)
{
    for (const GuideStep &s : steps) {
        auto *cell = new QWidget(this);
        auto *v = new QVBoxLayout(cell);
        v->setContentsMargins(4, 4, 4, 4);
        auto *img = new QLabel;
        img->setPixmap(Icons::illustration(s.image, QSize(imageWidth, imageWidth * 3 / 4)));
        img->setAlignment(Qt::AlignCenter);
        v->addWidget(img);
        auto *text = new QLabel(s.text);
        text->setWordWrap(true);
        text->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        text->setMaximumWidth(imageWidth + 40);
        v->addWidget(text, 1);
        m_cells << cell;
    }
    new QGridLayout(this);
    relayout();
}

void GuideWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayout();
}

void GuideWidget::relayout()
{
    const int cols = qBound(1, width() / (m_imageWidth + 50), qMax(1, int(m_cells.size())));
    if (cols == m_columns) return;
    m_columns = cols;
    auto *grid = static_cast<QGridLayout *>(layout());
    for (QWidget *c : m_cells) grid->removeWidget(c);
    for (int i = 0; i < m_cells.size(); ++i) grid->addWidget(m_cells.at(i), i / cols, i % cols, Qt::AlignTop);
}

GuideDialog::GuideDialog(const ModuleDef &m, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Anleitung – %1").arg(m.shortName));
    auto *lay = new QVBoxLayout(this);
    auto *head = new QLabel(QStringLiteral("<b>%1</b><br/>%2").arg(m.name.toHtmlEscaped(), m.description.toHtmlEscaped()));
    head->setWordWrap(true);
    lay->addWidget(head);
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    QList<GuideStep> steps = m.guide;
    if (steps.isEmpty() && !m.postureImage.isEmpty()) steps << GuideStep{ m.postureImage, m.posture };
    sa->setWidget(new GuideWidget(steps, 220));
    lay->addWidget(sa, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
    resize(820, 640);
}
