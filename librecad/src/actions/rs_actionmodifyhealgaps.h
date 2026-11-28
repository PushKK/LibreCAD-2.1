#ifndef RS_ACTIONMODIFYHEALGAPS_H
#define RS_ACTIONMODIFYHEALGAPS_H

#include "rs_actioninterface.h"

class RS_ActionModifyHealGaps : public RS_ActionInterface {
    Q_OBJECT

public:
    RS_ActionModifyHealGaps(RS_EntityContainer& container,
                            RS_GraphicView& graphicView);
    ~RS_ActionModifyHealGaps() override = default;

    void init(int status = 0) override;
    void trigger() override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void commandEvent(RS_CommandEvent* e) override;

    void showOptions() override;
    void hideOptions() override;

    void updateMouseButtonHints() override;
    void updateMouseCursor() override;

    void setTolerance(double tol) { tolerance = tol; }
    double getTolerance() const { return tolerance; }

    void setCloseContour(bool c) { closeContour = c; }
    bool getCloseContour() const { return closeContour; }





private:
    double tolerance;
    bool closeContour;
};

#endif // RS_ACTIONMODIFYHEALGAPS_H
