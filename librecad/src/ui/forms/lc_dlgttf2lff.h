#ifndef LC_DLGTTF2LFF_H
#define LC_DLGTTF2LFF_H

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

class LC_DlgTtf2Lff : public QDialog {
    Q_OBJECT

public:
    explicit LC_DlgTtf2Lff(QWidget* parent = nullptr);

private slots:
    void browseTtf();
    void browseLff();
    void convert();

private:
    QLineEdit* leAuthor;
    QLineEdit* leFontName;
    QLineEdit* leLffPath;
    QLineEdit* leLicense;
    QLineEdit* leTtfPath;
    QSpinBox* sbNodes;
    QSpinBox* sbPrecision;
    QDoubleSpinBox* dsbLetterSpacing;
    QDoubleSpinBox* dsbWordSpacing;
    QDoubleSpinBox* dsbLineSpacing;
};

#endif // LC_DLGTTF2LFF_H
