#ifndef QG_MODIFYHEALGAPSOPTIONS_H
#define QG_MODIFYHEALGAPSOPTIONS_H

#include <QWidget>

class QLineEdit;
class QCheckBox;
class QPushButton;
class RS_ActionInterface;
class RS_ActionModifyHealGaps;

class QG_ModifyHealGapsOptions : public QWidget {
    Q_OBJECT

public:
    explicit QG_ModifyHealGapsOptions(QWidget* parent = nullptr, Qt::WindowFlags fl = {});
    ~QG_ModifyHealGapsOptions() override;

public slots:
    void setAction(RS_ActionInterface* a, bool update);
    void updateTolerance(const QString& tol);
    void updateCloseContour(bool checked);
    void applyHeal();

protected:
    RS_ActionModifyHealGaps* action;

protected slots:
    virtual void languageChange();

private:
    void saveSettings();

    QLineEdit* leTolerance;
    QCheckBox* cbCloseContour;
    QPushButton* pbApply;
};

#endif // QG_MODIFYHEALGAPSOPTIONS_H
