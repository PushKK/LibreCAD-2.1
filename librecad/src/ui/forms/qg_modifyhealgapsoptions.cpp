#include "qg_modifyhealgapsoptions.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>

#include "rs_actionmodifyhealgaps.h"
#include "rs_settings.h"
#include "rs_math.h"
#include "rs_debug.h"

QG_ModifyHealGapsOptions::QG_ModifyHealGapsOptions(QWidget* parent, Qt::WindowFlags fl)
    : QWidget(parent, fl)
    , action(nullptr)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    const QFontMetrics fm = fontMetrics();
    const int charW = fm.averageCharWidth();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 1, 4, 1);
    layout->setSpacing(6);

// Проверить
    lblTol = new QLabel(tr("Tolerance:"), this);
//    auto* lblTol = new QLabel(tr("Tolerance:"), this);
    lblTol->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    leTolerance = new QLineEdit(this);
    leTolerance->setFixedWidth(charW * 10);
    leTolerance->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    leTolerance->setToolTip(tr("Maximum gap distance to heal"));

    cbCloseContour = new QCheckBox(tr("Close contour"), this);
    cbCloseContour->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    cbCloseContour->setToolTip(tr("Also close the gap between start and end of contour"));

    pbApply = new QPushButton(tr("Apply"), this);
    pbApply->setFixedWidth(fm.horizontalAdvance(pbApply->text()) + charW * 3);
    pbApply->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    pbApply->setToolTip(tr("Heal gaps now (or press Enter)"));

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);

    layout->addWidget(lblTol);
    layout->addWidget(leTolerance);
    layout->addWidget(cbCloseContour);
    layout->addWidget(pbApply);
    layout->addWidget(sep);
    layout->addStretch(1);

    // Вычисляем реальный размер всех элементов с учётом текущего шрифта и фиксируем минимальную ширину
//    layout->activate();
//    adjustSize();
//    setMinimumSize(sizeHint());

//    connect(leTolerance, &QLineEdit::textChanged, this, &QG_ModifyHealGapsOptions::updateTolerance);
//    connect(cbCloseContour, &QCheckBox::toggled, this, &QG_ModifyHealGapsOptions::updateCloseContour);
//    connect(leTolerance, &QLineEdit::returnPressed, this, &QG_ModifyHealGapsOptions::applyHeal, Qt::QueuedConnection);
//    connect(pbApply, &QPushButton::clicked, this, &QG_ModifyHealGapsOptions::applyHeal, Qt::QueuedConnection);
}

QG_ModifyHealGapsOptions::~QG_ModifyHealGapsOptions() {
    saveSettings();
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void QG_ModifyHealGapsOptions::languageChange()
{
// Проверить
//	ui->retranslateUi(this);
    lblTol->setText(tr("Tolerance:"));
    cbCloseContour->setText(tr("Close contour"));
    pbApply->setText(tr("Apply"));
}

void QG_ModifyHealGapsOptions::saveSettings() {
    RS_SETTINGS->beginGroup("/Modify");
    RS_SETTINGS->writeEntry("/HealGapsTolerance", leTolerance->text());
    RS_SETTINGS->writeEntry("/HealGapsClose", cbCloseContour->isChecked() ? 1 : 0);
    RS_SETTINGS->endGroup();
}

void QG_ModifyHealGapsOptions::setAction(RS_ActionInterface* a, bool update) {
    if (a && a->rtti() == RS2::ActionModifyHealGaps) {
        action = static_cast<RS_ActionModifyHealGaps*>(a);

        QString sTol;
        bool closeLoop = true;
        if (update) {
            sTol = QString::number(action->getTolerance());
            closeLoop = action->getCloseContour();
        } else {
            RS_SETTINGS->beginGroup("/Modify");
            sTol = RS_SETTINGS->readEntry("/HealGapsTolerance", "0.1");
            closeLoop = (RS_SETTINGS->readNumEntry("/HealGapsClose", 1) != 0);
            RS_SETTINGS->endGroup();
        }

        leTolerance->blockSignals(true);
        cbCloseContour->blockSignals(true);
        leTolerance->setText(sTol);
        cbCloseContour->setChecked(closeLoop);
        leTolerance->blockSignals(false);
        cbCloseContour->blockSignals(false);

        double val = RS_Math::eval(sTol);
        if (val >= 0.0) action->setTolerance(val);
        action->setCloseContour(closeLoop);
    } else {
        RS_DEBUG->print(RS_Debug::D_ERROR,
                        "QG_ModifyHealGapsOptions::setAction: wrong action type");
        action = nullptr;
    }
}

void QG_ModifyHealGapsOptions::updateTolerance(const QString& tol) {
    if (action) {
        bool ok = false;
        double val = RS_Math::eval(tol, &ok);
        if (ok && val >= 0.0) {
            action->setTolerance(val);
        }
    }
}

void QG_ModifyHealGapsOptions::updateCloseContour(bool checked) {
    if (action) {
        action->setCloseContour(checked);
    }
}

void QG_ModifyHealGapsOptions::applyHeal() {
    RS_ActionModifyHealGaps* act = action;
    saveSettings();
    if (act) {
        act->trigger();
    }
}
