#include "lc_dlgttf2lff.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <ft2build.h>
#include "rs_font.h"

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

#include "rs_system.h"
#include "rs_fontlist.h"

namespace {

struct Ttf2LffContext {
    FILE* fpLff = nullptr;
    double prevx = 0.0;
    double prevy = 0.0;
    bool firstpass = true;
    bool startcontour = true;
    double xMin = 1000.0;
    int nodes = 4;
    int precision = 6;
    double factor = 1.0;
    double yMax = -1000.0;
    char numFormat[16] = "%.6f";

    std::string clearZeros(double num) const {
        char buf[64];
        snprintf(buf, sizeof(buf), numFormat, num);
        std::string str(buf);
        if (str.find('.') != std::string::npos) {
            int i = static_cast<int>(str.length()) - 1;
            while (i > 1 && str[i] == '0') {
                --i;
            }
            if (str[i] != '.') {
                ++i;
            }
            return str.substr(0, i);
        }
        return str;
    }
};

int cbMoveTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<Ttf2LffContext*>(user);
    if (ctx->firstpass) {
        if (to->x < ctx->xMin) ctx->xMin = to->x;
    } else {
        ctx->prevx = to->x;
        ctx->prevy = to->y;
        if (ctx->fpLff) {
            std::string sx = ctx->clearZeros((to->x - ctx->xMin) * ctx->factor);
            std::string sy = ctx->clearZeros(to->y * ctx->factor);
            if (ctx->startcontour) {
                ctx->startcontour = false;
                fprintf(ctx->fpLff, "%s,%s", sx.c_str(), sy.c_str());
            } else {
                fprintf(ctx->fpLff, "\n%s,%s", sx.c_str(), sy.c_str());
            }
        }
    }
    return 0;
}

int cbLineTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<Ttf2LffContext*>(user);
    if (ctx->firstpass) {
        if (to->x < ctx->xMin) ctx->xMin = to->x;
    } else {
        if (ctx->fpLff) {
            std::string sx = ctx->clearZeros((to->x - ctx->xMin) * ctx->factor);
            std::string sy = ctx->clearZeros(to->y * ctx->factor);
            if (ctx->startcontour) {
                fprintf(ctx->fpLff, "%s,%s", sx.c_str(), sy.c_str());
                ctx->startcontour = false;
            } else {
                fprintf(ctx->fpLff, ";%s,%s", sx.c_str(), sy.c_str());
            }
        }
        ctx->prevx = to->x;
        ctx->prevy = to->y;
        if (to->y > ctx->yMax) ctx->yMax = to->y;
    }
    return 0;
}

int cbConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<Ttf2LffContext*>(user);
    if (ctx->firstpass) {
        if (to->x < ctx->xMin) ctx->xMin = to->x;
        return 0;
    }
    double ox = ctx->prevx;
    double oy = ctx->prevy;
    if (ctx->fpLff) {
        std::string sox = ctx->clearZeros((ox - ctx->xMin) * ctx->factor);
        std::string soy = ctx->clearZeros(oy * ctx->factor);
        if (ctx->startcontour) {
            fprintf(ctx->fpLff, "%s,%s", sox.c_str(), soy.c_str());
            ctx->startcontour = false;
        } else {
            fprintf(ctx->fpLff, ";%s,%s", sox.c_str(), soy.c_str());
        }
        for (double t = 0.0; t <= 1.0 + 1e-9; t += 1.0 / ctx->nodes) {
            double tc = (t > 1.0) ? 1.0 : t;
            double px = std::pow(1.0 - tc, 2) * ctx->prevx + 2.0 * tc * (1.0 - tc) * control->x + tc * tc * to->x;
            double py = std::pow(1.0 - tc, 2) * ctx->prevy + 2.0 * tc * (1.0 - tc) * control->y + tc * tc * to->y;
            std::string spx = ctx->clearZeros((px - ctx->xMin) * ctx->factor);
            std::string spy = ctx->clearZeros(py * ctx->factor);
            fprintf(ctx->fpLff, ";%s,%s", spx.c_str(), spy.c_str());
        }
    }
    ctx->prevx = to->x;
    ctx->prevy = to->y;
    if (to->y > ctx->yMax) ctx->yMax = to->y;
    return 0;
}

int cbCubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<Ttf2LffContext*>(user);
    if (ctx->firstpass) {
        if (to->x < ctx->xMin) ctx->xMin = to->x;
        return 0;
    }
    if (ctx->fpLff) {
        std::string sox = ctx->clearZeros((ctx->prevx - ctx->xMin) * ctx->factor);
        std::string soy = ctx->clearZeros(ctx->prevy * ctx->factor);
        if (ctx->startcontour) {
            fprintf(ctx->fpLff, "%s,%s", sox.c_str(), soy.c_str());
            ctx->startcontour = false;
        } else {
            fprintf(ctx->fpLff, ";%s,%s", sox.c_str(), soy.c_str());
        }
        for (double t = 0.0; t <= 1.0 + 1e-9; t += 1.0 / ctx->nodes) {
            double tc = (t > 1.0) ? 1.0 : t;
            double u = 1.0 - tc;
            double px = u*u*u*ctx->prevx + 3*u*u*tc*control1->x + 3*u*tc*tc*control2->x + tc*tc*tc*to->x;
            double py = u*u*u*ctx->prevy + 3*u*u*tc*control1->y + 3*u*tc*tc*control2->y + tc*tc*tc*to->y;
            std::string spx = ctx->clearZeros((px - ctx->xMin) * ctx->factor);
            std::string spy = ctx->clearZeros(py * ctx->factor);
            fprintf(ctx->fpLff, ";%s,%s", spx.c_str(), spy.c_str());
        }
    }
    ctx->prevx = to->x;
    ctx->prevy = to->y;
    if (to->y > ctx->yMax) ctx->yMax = to->y;
    return 0;
}

FT_Error convertGlyph(FT_Face face, FT_ULong charcode, Ttf2LffContext& ctx) {
    static const FT_Outline_Funcs outlineFuncs = {
        cbMoveTo, cbLineTo, cbConicTo, cbCubicTo, 0, 0
    };

    FT_Error error = FT_Load_Glyph(face, FT_Get_Char_Index(face, charcode),
                                   FT_LOAD_NO_BITMAP | FT_LOAD_NO_SCALE);
    if (error) return error;

    FT_Glyph glyph;
    error = FT_Get_Glyph(face->glyph, &glyph);
    if (error) return error;

    if (face->glyph->format == ft_glyph_format_outline) {
        auto* og = reinterpret_cast<FT_OutlineGlyph>(glyph);
        if (ctx.fpLff) {
            fprintf(ctx.fpLff, "\n[#%04X]\n", static_cast<unsigned>(charcode));
        }
        ctx.xMin = 1000.0;
        ctx.firstpass = true;
        FT_Outline_Decompose(&(og->outline), &outlineFuncs, &ctx);

        ctx.firstpass = false;
        ctx.startcontour = true;
        error = FT_Outline_Decompose(&(og->outline), &outlineFuncs, &ctx);
        if (ctx.fpLff) {
            fprintf(ctx.fpLff, "\n");
        }
    }
    FT_Done_Glyph(glyph);
    return error;
}

QString readSfntName(FT_Face face, FT_UShort nameId) {
    if (!FT_IS_SFNT(face)) return QString();
    FT_UInt count = FT_Get_Sfnt_Name_Count(face);
    QString fallback;
    for (FT_UInt i = 0; i < count; ++i) {
        FT_SfntName sfnt;
        if (FT_Get_Sfnt_Name(face, i, &sfnt) != 0) continue;
        if (sfnt.name_id != nameId) continue;

        // Windows Unicode (UTF-16BE) — самый надёжный вариант (включая кириллицу)
        if (sfnt.platform_id == TT_PLATFORM_MICROSOFT &&
            (sfnt.encoding_id == TT_MS_ID_UNICODE_CS || sfnt.encoding_id == TT_MS_ID_UCS_4)) {
            QString s;
            for (FT_UInt b = 0; b + 1 < sfnt.string_len; b += 2) {
                ushort ch = (static_cast<uchar>(sfnt.string[b]) << 8) | static_cast<uchar>(sfnt.string[b + 1]);
                if (ch != 0) s.append(QChar(ch));
            }
            if (!s.isEmpty()) return s.trimmed();
        } else if (sfnt.platform_id == TT_PLATFORM_MACINTOSH && sfnt.encoding_id == TT_MAC_ID_ROMAN) {
            fallback = QString::fromLatin1(reinterpret_cast<const char*>(sfnt.string), sfnt.string_len).trimmed();
        }
    }
    return fallback;
}

} // namespace

LC_DlgTtf2Lff::LC_DlgTtf2Lff(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import Font (TTF to LFF)"));
    setMinimumWidth(480);

    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    // TTF input
    auto* ttfRow = new QHBoxLayout();
    leTtfPath = new QLineEdit(this);
    auto* btnBrowseTtf = new QPushButton(tr("Browse..."), this);
    ttfRow->addWidget(leTtfPath);
    ttfRow->addWidget(btnBrowseTtf);
    form->addRow(tr("Source TTF file:"), ttfRow);

    // LFF output
    auto* lffRow = new QHBoxLayout();
    leLffPath = new QLineEdit(this);
    auto* btnBrowseLff = new QPushButton(tr("Browse..."), this);
    lffRow->addWidget(leLffPath);
    lffRow->addWidget(btnBrowseLff);
    form->addRow(tr("Target LFF file:"), lffRow);

    leFontName = new QLineEdit(this);
    leFontName->setPlaceholderText(tr("Auto-detected from TTF"));
    form->addRow(tr("Font name:"), leFontName);
    sbNodes = new QSpinBox(this);
    sbNodes->setRange(1, 64);
    sbNodes->setValue(4);
    form->addRow(tr("Nodes per spline:"), sbNodes);

    sbPrecision = new QSpinBox(this);
    sbPrecision->setRange(1, 10);
    sbPrecision->setValue(6);
    form->addRow(tr("Decimal precision:"), sbPrecision);

    dsbLetterSpacing = new QDoubleSpinBox(this);
    dsbLetterSpacing->setRange(0.0, 100.0);
    dsbLetterSpacing->setValue(3.0);
    form->addRow(tr("Letter spacing:"), dsbLetterSpacing);

    dsbWordSpacing = new QDoubleSpinBox(this);
    dsbWordSpacing->setRange(0.0, 100.0);
    dsbWordSpacing->setValue(6.75);
    form->addRow(tr("Word spacing:"), dsbWordSpacing);

    dsbLineSpacing = new QDoubleSpinBox(this);
    dsbLineSpacing->setRange(0.1, 10.0);
    dsbLineSpacing->setSingleStep(0.1);
    dsbLineSpacing->setValue(1.0);
    form->addRow(tr("Line spacing factor:"), dsbLineSpacing);

    leAuthor = new QLineEdit("Unknown", this);
    form->addRow(tr("Author:"), leAuthor);

    leLicense = new QLineEdit("Unknown", this);
    form->addRow(tr("License:"), leLicense);

    mainLayout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* btnConvert = new QPushButton(tr("Import"), this);
    auto* btnCancel = new QPushButton(tr("Cancel"), this);
    btnConvert->setDefault(true);
    btnRow->addWidget(btnConvert);
    btnRow->addWidget(btnCancel);
    mainLayout->addLayout(btnRow);

    connect(btnBrowseTtf, &QPushButton::clicked, this, &LC_DlgTtf2Lff::browseTtf);
    connect(btnBrowseLff, &QPushButton::clicked, this, &LC_DlgTtf2Lff::browseLff);
    connect(btnConvert, &QPushButton::clicked, this, &LC_DlgTtf2Lff::convert);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void LC_DlgTtf2Lff::browseTtf() {
    QString file = QFileDialog::getOpenFileName(
        this, tr("Select TrueType Font"), QString(),
        tr("TrueType Fonts (*.ttf *.TTF);;All Files (*)"));
    if (file.isEmpty()) return;

    FT_Library lib = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&lib) == 0) {
        if (FT_New_Face(lib, QFile::encodeName(file).constData(), 0, &face) == 0) {
            QString fontName = readSfntName(face, TT_NAME_ID_FULL_NAME);
            if (fontName.isEmpty() && face->family_name) {
                fontName = QString::fromUtf8(face->family_name);
            }
            leFontName->setText(fontName.isEmpty() ? QFileInfo(file).completeBaseName() : fontName);

            QString author = readSfntName(face, TT_NAME_ID_DESIGNER);
            if (author.isEmpty()) {
                author = readSfntName(face, TT_NAME_ID_MANUFACTURER);
            }
            leAuthor->setText(author.isEmpty() ? "Unknown" : author);

            QString license = readSfntName(face, TT_NAME_ID_LICENSE);
            if (license.isEmpty()) {
                license = readSfntName(face, TT_NAME_ID_COPYRIGHT);
            }
            if (!license.isEmpty()) {
                leLicense->setText(license);
            }
            FT_Done_Face(face);
        }
        FT_Done_Library(lib);
    }


    leTtfPath->setText(file);
    if (leLffPath->text().isEmpty()) {
        QFileInfo fi(file);
// Путь к папке шрифтов - глобальный
//        QStringList fontDirs = RS_SYSTEM->getDirectoryList("fonts");
// Путь к папке шрифтов - локальный
// ./librecad/support/fonts
        QString targetDir = QDir(QCoreApplication::applicationDirPath()).filePath("support/fonts");
        QDir().mkpath(targetDir);
        leLffPath->setText(QDir(targetDir).filePath(fi.completeBaseName() + ".lff"));
    }
}

void LC_DlgTtf2Lff::browseLff() {
    QString file = QFileDialog::getSaveFileName(
        this, tr("Save LibreCAD Font"), leLffPath->text(),
        tr("LibreCAD Fonts (*.lff)"));
    if (!file.isEmpty()) {
        if (!file.endsWith(".lff", Qt::CaseInsensitive)) {
            file += ".lff";
        }
        leLffPath->setText(file);
    }
}

void LC_DlgTtf2Lff::convert() {
    QString ttfPath = leTtfPath->text().trimmed();
    QString lffPath = leLffPath->text().trimmed();
    if (ttfPath.isEmpty() || lffPath.isEmpty()) {
        QMessageBox::warning(this, tr("Import Font"), tr("Please specify both TTF and LFF file paths."));
        return;
    }
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        QMessageBox::critical(this, tr("Import Font"), tr("Failed to initialize FreeType library."));
        return;
    }
    QByteArray ttfBytes = QFile::encodeName(ttfPath);
    if (FT_New_Face(library, ttfBytes.constData(), 0, &face) != 0) {
        FT_Done_Library(library);
        QMessageBox::critical(this, tr("Import Font"), tr("Cannot open TTF file:\n%1").arg(ttfPath));
        return;
    }
    Ttf2LffContext ctx;
    ctx.nodes = sbNodes->value();
    ctx.precision = sbPrecision->value();
    snprintf(ctx.numFormat, sizeof(ctx.numFormat), "%%.%df", ctx.precision);
    // Trace 'A' to calculate scaling factor
    ctx.yMax = -1000.0;
    convertGlyph(face, 65, ctx);
    ctx.factor = (ctx.yMax > 0.0) ? (1.0 / (1.0 / 9.0 * ctx.yMax)) : 1.0;
    QByteArray lffBytes = QFile::encodeName(lffPath);
    ctx.fpLff = fopen(lffBytes.constData(), "wt");
    if (!ctx.fpLff) {
        FT_Done_Face(face);
        FT_Done_Library(library);
        QMessageBox::critical(this, tr("Import Font"), tr("Cannot create LFF file:\n%1").arg(lffPath));
        return;
    }

//    QString familyName = face->family_name ? QString::fromUtf8(face->family_name) : QFileInfo(ttfPath).completeBaseName();
    QString familyName = leFontName->text().trimmed();
    if (familyName.isEmpty()) {
        familyName = face->family_name ? QString::fromUtf8(face->family_name) : QFileInfo(ttfPath).completeBaseName();
    }
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    fprintf(ctx.fpLff, "# Format:            LibreCAD Font 1\n");
    fprintf(ctx.fpLff, "# Creator:           ttf2lff (LibreCAD)\n");
    fprintf(ctx.fpLff, "# Version:           1\n");
    fprintf(ctx.fpLff, "# Name:              %s\n", familyName.toUtf8().constData());
    fprintf(ctx.fpLff, "# LetterSpacing:     %s\n", ctx.clearZeros(dsbLetterSpacing->value()).c_str());
    fprintf(ctx.fpLff, "# WordSpacing:       %s\n", ctx.clearZeros(dsbWordSpacing->value()).c_str());
    fprintf(ctx.fpLff, "# LineSpacingFactor: %s\n", ctx.clearZeros(dsbLineSpacing->value()).c_str());
    fprintf(ctx.fpLff, "# Created:           %s\n", dateStr.toUtf8().constData());
    fprintf(ctx.fpLff, "# Last modified:     %s\n", dateStr.toUtf8().constData());
    fprintf(ctx.fpLff, "# Author:            %s\n", leAuthor->text().toUtf8().constData());
    fprintf(ctx.fpLff, "# License:           %s\n\n", leLicense->text().toUtf8().constData());
    FT_UInt gindex = 0;
    FT_ULong charcode = FT_Get_First_Char(face, &gindex);
    while (gindex != 0) {
        convertGlyph(face, charcode, ctx);
        charcode = FT_Get_Next_Char(face, charcode, &gindex);
    }
    fclose(ctx.fpLff);
    FT_Done_Face(face);
    FT_Done_Library(library);
    // Reload font list so new font is immediately available
    // Перезагружаем список шрифтов в движке
    RS_FONTLIST->clearFonts();
    RS_FONTLIST->init();
    QMessageBox::information(this, tr("Import Font"),
    tr("Font '%1' successfully imported to:\n%2").arg(familyName, lffPath));
    accept();
}
