// BoundaryRenderer.cpp
// Реализация отрисовки границ
// Визуализация из MARKER.C

#include "BoundaryRenderer.h"

#include <cmath>

namespace Interferometry {

CBoundaryRenderer::CBoundaryRenderer() {}

CBoundaryRenderer::~CBoundaryRenderer() {}

//=============================================================================
// Отрисовка всех установленных границ
//=============================================================================
void CBoundaryRenderer::DrawBoundaries(CDC* pDC,
                                       const CEllipseBoundary* boundary) {
  if (!pDC || !boundary) return;

  CPen outerPen(PS_SOLID, m_params.lineWidth, m_params.outerBoundaryColor);
  CPen innerPen(PS_SOLID, m_params.lineWidth, m_params.innerBoundaryColor);
  CPen* pOldPen = pDC->SelectObject(&outerPen);

  int height = boundary->GetImageHeight();

  // Отрисовка внешних границ
  for (int y = 0; y < height; y++) {
    const RowBoundary& row = boundary->GetRowBoundary(y);

    if (row.HasOuterBoundary()) {
      // Левая граница
      pDC->SetPixel(row.leftOuter, y, m_params.outerBoundaryColor);
      // Правая граница
      pDC->SetPixel(row.rightOuter, y, m_params.outerBoundaryColor);
    }
  }

  // Отрисовка внутренних границ
  pDC->SelectObject(&innerPen);

  for (int y = 0; y < height; y++) {
    const RowBoundary& row = boundary->GetRowBoundary(y);

    if (row.HasInnerBoundary()) {
      // Левая внутренняя граница
      pDC->SetPixel(row.leftInner, y, m_params.innerBoundaryColor);
      // Правая внутренняя граница
      pDC->SetPixel(row.rightInner, y, m_params.innerBoundaryColor);
    }
  }

  pDC->SelectObject(pOldPen);
}

//=============================================================================
// Отрисовка текущего редактируемого эллипса
//=============================================================================
void CBoundaryRenderer::DrawCurrentEllipse(CDC* pDC,
                                           const EllipseParams& ellipse) {
  if (!pDC || !ellipse.IsValid()) return;

  DrawEllipse(pDC, ellipse, m_params.currentEllipseColor);
}

//=============================================================================
// Отрисовка маркера (как в mark_on из MARKER.C)
//=============================================================================
void CBoundaryRenderer::DrawMarker(CDC* pDC, CPoint center, int semiAxisA,
                                   int semiAxisB) {
  if (!pDC) return;

  DrawMarkerCross(pDC, center, semiAxisA, semiAxisB);
}

//=============================================================================
// Отрисовка креста маркера (4 отметки на концах осей)
//=============================================================================
void CBoundaryRenderer::DrawMarkerCross(CDC* pDC, CPoint center, int semiAxisA,
                                        int semiAxisB) {
  CPen pen(PS_SOLID, 1, m_params.markerColor);
  CPen* pOldPen = pDC->SelectObject(&pen);

  const int markerSize = 5;  // Размер отметки (5 пикселей как в оригинале)

  // Правая отметка (x + a)
  int xRight = center.x + semiAxisA;
  for (int dy = -2; dy <= 2; dy++) {
    pDC->SetPixel(xRight, center.y + dy, m_params.markerColor);
  }

  // Левая отметка (x - a)
  int xLeft = center.x - semiAxisA;
  for (int dy = -2; dy <= 2; dy++) {
    pDC->SetPixel(xLeft, center.y + dy, m_params.markerColor);
  }

  // Нижняя отметка (y + b)
  int yBottom = center.y + semiAxisB;
  for (int dx = -2; dx <= 2; dx++) {
    pDC->SetPixel(center.x + dx, yBottom, m_params.markerColor);
  }

  // Верхняя отметка (y - b)
  int yTop = center.y - semiAxisB;
  for (int dx = -2; dx <= 2; dx++) {
    pDC->SetPixel(center.x + dx, yTop, m_params.markerColor);
  }

  pDC->SelectObject(pOldPen);
}

//=============================================================================
// Отрисовка эллипса по уравнению
//=============================================================================
void CBoundaryRenderer::DrawEllipse(CDC* pDC, const EllipseParams& ellipse,
                                    COLORREF color) {
  CPen pen(PS_SOLID, m_params.lineWidth, color);
  CPen* pOldPen = pDC->SelectObject(&pen);

  int cx = ellipse.centerX;
  int cy = ellipse.centerY;
  int a = ellipse.semiAxisA;
  int b = ellipse.semiAxisB;

  // Рисуем эллипс через уравнение (как в ell_bld)
  // x^2/a^2 + y^2/b^2 = 1

  int yMin = (std::max)(0, cy - b);
  int yMax = cy + b;

  for (int y = yMin; y <= yMax; y++) {
    float dy = (float)(y - cy);
    float x = a * (std::sqrt)(1.0f - (dy * dy) / ((float)b * (float)b));

    int x1 = (int)(cx - x);
    int x2 = (int)(cx + x);

    // Левая часть эллипса
    pDC->SetPixel(x1, y, color);
    // Правая часть эллипса
    pDC->SetPixel(x2, y, color);
  }

  pDC->SelectObject(pOldPen);
}

//=============================================================================
// Отрисовка всего (границы + текущий эллипс + маркер)
//=============================================================================
void CBoundaryRenderer::DrawAll(CDC* pDC, const CEllipseBoundary* boundary,
                                const CBoundaryController* controller) {
  if (!pDC) return;

  // Рисуем установленные границы
  if (boundary) {
    DrawBoundaries(pDC, boundary);
  }

  // Рисуем текущий редактируемый эллипс и маркер
  if (controller && controller->IsActive()) {
    const EllipseParams& ellipse = controller->GetCurrentEllipse();
    CPoint markerPos = controller->GetMarkerPosition();

    if (m_params.showCurrentEllipse) {
      DrawCurrentEllipse(pDC, ellipse);
    }

    if (m_params.showMarker) {
      DrawMarker(pDC, markerPos, ellipse.semiAxisA, ellipse.semiAxisB);
    }
  }
}

}  // namespace Interferometry
