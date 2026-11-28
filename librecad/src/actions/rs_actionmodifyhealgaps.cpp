#include "rs_actionmodifyhealgaps.h"

#include <vector>
#include <list>
#include <algorithm>
#include <cmath>
#include <QMouseEvent>

#include "rs_commandevent.h"
#include "rs_dialogfactory.h"
#include "rs_graphicview.h"
#include "rs_document.h"
#include "rs_line.h"
#include "rs_math.h"
#include "rs_arc.h"
#include "rs_spline.h"
#include "lc_splinepoints.h"
#include "rs_settings.h"

namespace {

bool getEdgeEndpoints(RS_Entity* e, RS_Vector& start, RS_Vector& end) {
    if (e->rtti() == RS2::EntitySpline) {
        auto* sp = static_cast<RS_Spline*>(e);
        if (sp->getData().controlPoints.size() < 2) return false;
        start = sp->getData().controlPoints.front();
        end   = sp->getData().controlPoints.back();
    } else if (e->rtti() == RS2::EntitySplinePoints) {
        auto* sp = static_cast<LC_SplinePoints*>(e);
        start = sp->getStartpoint();
        end   = sp->getEndpoint();
    } else {
        start = e->getStartpoint();
        end   = e->getEndpoint();
    }
    return start.valid && end.valid;
}

void revertEdge(RS_Entity* e) {
    e->revertDirection();
}

void setEdgeEndpoint(RS_Entity*& e, bool isStart, const RS_Vector& pt) {
    switch (e->rtti()) {
    case RS2::EntityLine: {
        auto* line = static_cast<RS_Line*>(e);
        if (isStart) line->setStartpoint(pt);
        else         line->setEndpoint(pt);
        break;
    }
    case RS2::EntityArc: {
        auto* arc = static_cast<RS_Arc*>(e);
        double ang = arc->getCenter().angleTo(pt);
        if (isStart) arc->setAngle1(ang);
        else         arc->setAngle2(ang);
        arc->calculateBorders();
        break;
    }
     case RS2::EntitySpline: {
         auto* sp = static_cast<RS_Spline*>(e);
         RS_SplineData d = sp->getData();
         if (!d.controlPoints.empty()) {
             if (isStart) d.controlPoints.front() = pt;
             else         d.controlPoints.back() = pt;
             auto* newSp = new RS_Spline(sp->getParent(), d);
             newSp->setLayer(sp->getLayer());
             newSp->setPen(sp->getPen(false));
             newSp->setHandle(sp->getHandle());
             newSp->update();
             delete sp;
             e = newSp;
         }
         break;
    }
    case RS2::EntitySplinePoints: {
        auto* sp = static_cast<LC_SplinePoints*>(e);
        auto& d = sp->getData();
        if (!d.splinePoints.empty()) {
            if (isStart) d.splinePoints.front() = pt;
            else         d.splinePoints.back() = pt;
        } else if (!d.controlPoints.empty()) {
            if (isStart) d.controlPoints.front() = pt;
            else         d.controlPoints.back() = pt;
        }
        sp->update();
        break;
    }
    default:
        break;
    }
}

} // namespace

RS_ActionModifyHealGaps::RS_ActionModifyHealGaps(
    RS_EntityContainer& container,
    RS_GraphicView& graphicView)
    : RS_ActionInterface("Heal Gaps", container, graphicView)
    , tolerance(0.1)
    , closeContour(true)
{
    actionType = RS2::ActionModifyHealGaps;
    RS_SETTINGS->beginGroup("/Modify");
    tolerance = RS_SETTINGS->readEntry("/HealGapsTolerance", "0.1").toDouble();
    if (tolerance <= 0.0) tolerance = 0.1;
    closeContour = (RS_SETTINGS->readNumEntry("/HealGapsClose", 1) != 0);
    RS_SETTINGS->endGroup();
}

void RS_ActionModifyHealGaps::init(int status) {
    RS_ActionInterface::init(status);
//    if (status >= 0) {
//        trigger();
//    }
    if (status < 0) {
        hideOptions();
    } else {
        showOptions();
    }
}

void RS_ActionModifyHealGaps::trigger() {
    std::vector<RS_Entity*> origEdges;
    for (RS_Entity* e : *container) {
        if (!e->isSelected()) continue;
        switch (e->rtti()) {
        case RS2::EntityLine:
        case RS2::EntityArc:
        case RS2::EntitySpline:
        case RS2::EntitySplinePoints:
            origEdges.push_back(e);
            break;
        default:
            break;
        }
    }

    if (origEdges.size() < 2) {
        RS_DIALOGFACTORY->commandMessage(
            tr("Heal gaps: select at least two connected entities (line, arc or spline)."));
        finish(false);
        return;
    }

    // Clone entities so original versions can be kept in Undo stack
    std::vector<RS_Entity*> edges;
    edges.reserve(origEdges.size());
    for (RS_Entity* orig : origEdges) {
        RS_Entity* cl = orig->clone();
        cl->reparent(container);
        edges.push_back(cl);
    }

    // Build bidirectional chain with search tolerance
    double chainTol = std::max(tolerance, 1.0e-4);
    std::vector<size_t> bestChain;
    std::vector<bool> bestRev(edges.size(), false);

    for (size_t startIdx = 0; startIdx < edges.size(); ++startIdx) {
        RS_Vector s0, e0;
        if (!getEdgeEndpoints(edges[startIdx], s0, e0)) continue;

        std::list<size_t> chainIdx;
        std::vector<bool> used(edges.size(), false);
        std::vector<bool> rev(edges.size(), false);

        used[startIdx] = true;
        chainIdx.push_back(startIdx);
        RS_Vector headPt = s0;
        RS_Vector tailPt = e0;

        while (true) {
            double distBest = chainTol;
            int best = -1;
            bool atTail = true;
            bool revertBest = false;

            for (size_t j = 0; j < edges.size(); ++j) {
                if (used[j]) continue;
                RS_Vector s, e2;
                if (!getEdgeEndpoints(edges[j], s, e2)) continue;

                double dTailStart = s.distanceTo(tailPt);
                double dTailEnd   = e2.distanceTo(tailPt);
                if (dTailStart <= distBest) {
                    distBest = dTailStart; best = static_cast<int>(j); atTail = true; revertBest = false;
                }
                if (dTailEnd <= distBest) {
                    distBest = dTailEnd;   best = static_cast<int>(j); atTail = true; revertBest = true;
                }

                double dHeadEnd   = e2.distanceTo(headPt);
                double dHeadStart = s.distanceTo(headPt);
                if (dHeadEnd <= distBest) {
                    distBest = dHeadEnd;   best = static_cast<int>(j); atTail = false; revertBest = false;
                }
                if (dHeadStart <= distBest) {
                    distBest = dHeadStart; best = static_cast<int>(j); atTail = false; revertBest = true;
                }
            }

            if (best < 0) break;

            size_t bIdx = static_cast<size_t>(best);
            used[bIdx] = true;
            rev[bIdx] = revertBest;

            RS_Vector s, e2;
            getEdgeEndpoints(edges[bIdx], s, e2);

            if (atTail) {
                chainIdx.push_back(bIdx);
                tailPt = revertBest ? s : e2;
            } else {
                chainIdx.push_front(bIdx);
                headPt = revertBest ? e2 : s;
            }
        }

        if (chainIdx.size() > bestChain.size()) {
            bestChain.assign(chainIdx.begin(), chainIdx.end());
            bestRev = rev;
        }
        if (bestChain.size() == edges.size()) break;
    }

    for (size_t i = 0; i < edges.size(); ++i) {
        if (bestRev[i]) revertEdge(edges[i]);
    }

    int healedCount = 0;
    const double eps = 1.0e-9;

    // Heal internal gaps between consecutive segments in the chain
    for (size_t k = 0; k + 1 < bestChain.size(); ++k) {
// Проверить
//        setEdgeEndpoint(edges[bestChain[k]], false, target);
//        setEdgeEndpoint(edges[bestChain[k + 1]], true, target);

//        RS_Entity* curr = edges[bestChain[k]];
//        RS_Entity* next = edges[bestChain[k + 1]];
        size_t idx1 = bestChain[k];
        size_t idx2 = bestChain[k + 1];
        RS_Vector s1, e1, s2, e2;
//        if (!getEdgeEndpoints(curr, s1, e1) || !getEdgeEndpoints(next, s2, e2)) continue;
        if (!getEdgeEndpoints(edges[idx1], s1, e1) || !getEdgeEndpoints(edges[idx2], s2, e2)) continue;

        double gap = e1.distanceTo(s2);
        if (gap > eps && gap <= tolerance) {
            RS_Vector target;
//            if (curr->rtti() == RS2::EntityArc && next->rtti() != RS2::EntityArc) {
//                target = e1; // Keep arc endpoint exact
//            } else if (next->rtti() == RS2::EntityArc && curr->rtti() != RS2::EntityArc) {
            if (edges[idx1]->rtti() == RS2::EntityArc && edges[idx2]->rtti() != RS2::EntityArc) {
                target = e1;
            } else if (edges[idx2]->rtti() == RS2::EntityArc && edges[idx1]->rtti() != RS2::EntityArc) {
                target = s2; // Keep arc endpoint exact
            } else {
                target = (e1 + s2) * 0.5;
            }
//            setEdgeEndpoint(curr, false, target);
//            setEdgeEndpoint(next, true, target);
            setEdgeEndpoint(edges[idx1], false, target);
            setEdgeEndpoint(edges[idx2], true, target);
            ++healedCount;
        }
    }

    // Optionally close the loop between last and first segment
    bool closedLoop = false;
    if (closeContour && bestChain.size() >= 2) {
        size_t idxFirst = bestChain.front();
        size_t idxLast  = bestChain.back();
        RS_Vector sFirst, eFirst, sLast, eLast;
        if (edges[idxFirst] && edges[idxLast] &&
            getEdgeEndpoints(edges[idxFirst], sFirst, eFirst) &&
            getEdgeEndpoints(edges[idxLast], sLast, eLast)) {
            double endGap = eLast.distanceTo(sFirst);
            if (endGap <= eps) {
                closedLoop = true;
            } else if (endGap <= tolerance) {
                RS_Vector target;
                if (edges[idxFirst]->rtti() == RS2::EntityArc && edges[idxLast]->rtti() != RS2::EntityArc) {
                    target = sFirst;
                } else if (edges[idxLast]->rtti() == RS2::EntityArc && edges[idxFirst]->rtti() != RS2::EntityArc) {
                    target = eLast;
                } else {
                    target = (sFirst + eLast) * 0.5;
                }
                setEdgeEndpoint(edges[idxLast], false, target);
                setEdgeEndpoint(edges[idxFirst], true, target);
                ++healedCount;
                closedLoop = true;
            }
        }
    }
    // Commit changes to document with Undo support
    if (document) {
        document->startUndoCycle();
    }

    for (size_t i = 0; i < origEdges.size(); ++i) {
        origEdges[i]->setSelected(false);
        origEdges[i]->changeUndoState();
        if (document) {
            document->addUndoable(origEdges[i]);
        }
    }

    // Add cloned entities in sorted chain order
    std::vector<bool> inChain(edges.size(), false);
    for (size_t idx : bestChain) {
        inChain[idx] = true;
        edges[idx]->setSelected(true);
        container->addEntity(edges[idx]);
        if (document) {
            document->addUndoable(edges[idx]);
        }
    }

    for (size_t i = 0; i < edges.size(); ++i) {
        if (!inChain[i]) {
            edges[i]->setSelected(true);
            container->addEntity(edges[i]);
            if (document) {
                document->addUndoable(edges[i]);
            }
        }
    }

    if (document) {
        document->endUndoCycle();
    }

    graphicView->redraw(RS2::RedrawDrawing);
    int unchained = static_cast<int>(edges.size() - bestChain.size());
    QString msg = tr("Heal gaps: %1 gap(s) healed (tol=%2)").arg(healedCount).arg(tolerance);
    if (closedLoop) msg += tr(", contour closed");
    if (unchained > 0) msg += tr(", %1 not connected").arg(unchained);
    RS_DIALOGFACTORY->commandMessage(msg + ".");
    hideOptions();
    finish(false);
}

void RS_ActionModifyHealGaps::updateMouseButtonHints() {
    RS_DIALOGFACTORY->updateMouseWidget(
        tr("Set tolerance and click Apply (or enter tolerance in command line)"),
        tr("Cancel"));
}

void RS_ActionModifyHealGaps::showOptions() {
    RS_ActionInterface::showOptions();
    RS_DIALOGFACTORY->requestOptions(this, true);
}

void RS_ActionModifyHealGaps::hideOptions() {
    RS_ActionInterface::hideOptions();
    RS_DIALOGFACTORY->requestOptions(this, false);
}

void RS_ActionModifyHealGaps::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) {
        init(-1);
    }
}

void RS_ActionModifyHealGaps::commandEvent(RS_CommandEvent* e) {
    QString c = e->getCommand().trimmed();
    if (c.isEmpty()) {
        e->accept();
        trigger();
        return;
    }
    bool ok = false;
    double val = RS_Math::eval(c, &ok);
    if (ok && val >= 0.0) {
        e->accept();
        tolerance = val;
        RS_DIALOGFACTORY->requestOptions(this, true, true);
        trigger();
    }
}

void RS_ActionModifyHealGaps::updateMouseCursor() {
    graphicView->setMouseCursor(RS2::ArrowCursor);
}