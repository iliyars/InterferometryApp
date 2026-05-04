// BoundaryRenderer.h
// Отрисовка эллиптических границ в MFC View
// Визуализация из MARKER.C (mark_on/mark_off)

#pragma once

#include "EllipseBoundary.h"
#include "BoundaryController.h"

namespace Interferometry {

// Параметры отрисовки
struct BoundaryRenderParams {
  COLORREF outerBoundaryColor;   // Цвет внешней границы
  COLORREF innerBoundaryColor;   // Цвет внутренней границы
  COLORREF markerColor;          // Цвет маркера
  COLORREF currentEllipseColor;  // Цвет текущего редактируемого эллипса
  int lineWidth;                 // Толщина линий
  bool showMarker;               // Показывать маркер
  bool showCurrentEllipse;       // Показывать текущий эллипс

  BoundaryRenderParams()
      : outerBoundaryColor(RGB(255, 255, 255)),   // Белый
        innerBoundaryColor(RGB(0, 255, 255)),     // Cyan
        markerColor(RGB(255, 0, 0)),              // Красный
        currentEllipseColor(RGB(255, 255, 0)),    // Желтый
        lineWidth(1),
        showMarker(true),
        showCurrentEllipse(true) {}
};

// Класс для отрисовки границ
class CBoundaryRenderer {
 public:
  CBoundaryRenderer();
  ~CBoundaryRenderer();

  // Установка параметров
  void SetParams(const BoundaryRenderParams& params) { m_params = params; }
  const BoundaryRenderParams& GetParams() const { return m_params; }

  // Отрисовка установленных границ
  void DrawBoundaries(CDC* pDC, const CEllipseBoundary* boundary);

  // Отрисовка текущего редактируемого эллипса
  void DrawCurrentEllipse(CDC* pDC, const EllipseParams& ellipse);

  // Отрисовка маркера центра
  void DrawMarker(CDC* pDC, CPoint center, int semiAxisA, int semiAxisB);

  // Отрисовка полного состояния (границы + текущий эллипс + маркер)
  void DrawAll(CDC* pDC, const CEllipseBoundary* boundary,
               const CBoundaryController* controller);

 private:
  // Отрисовка одной строки границ
  void DrawRowBoundary(CDC* pDC, int row, const RowBoundary& boundary);

  // Отрисовка эллипса по уравнению
  void DrawEllipse(CDC* pDC, const EllipseParams& ellipse, COLORREF color);

  // Отрисовка креста маркера (как в mark_on)
  void DrawMarkerCross(CDC* pDC, CPoint center, int semiAxisA, int semiAxisB);

 private:
  BoundaryRenderParams m_params;
};

}  // namespace Interferometry
