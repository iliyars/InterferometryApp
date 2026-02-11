// TracingTypes.h
// Типы данных для трассировки полос

#pragma once

#include <vector>

#include "FringeTracer.h"

namespace Interferometry {

// Одна трассированная линия полосы
struct FringeLine {
  std::vector<CTracerPoint> points;  // Точки линии
  COLORREF color;                    // Цвет для отрисовки
  int id;                            // ID линии (порядковый номер)

  FringeLine() : color(RGB(255, 255, 0)), id(0) {}

  FringeLine(int lineId, COLORREF lineColor = RGB(255, 255, 0))
      : id(lineId), color(lineColor) {}

  // Получить количество точек
  size_t GetPointCount() const { return points.size(); }

  // Проверить есть ли точки
  bool IsEmpty() const { return points.empty(); }
};

// Режимы редактирования
enum class EditMode {
  MODE_NONE = 0,            // Просмотр (ничего не делаем)
  MODE_SET_OUTER_BOUNDARY,  // Установка внешней границы (клики мышкой)
  MODE_SET_INNER_BOUNDARY,  // Установка внутренней границы (клики мышкой)
  MODE_TRACE_FRINGE         // Трассировка полосы (клик на полосу)
};

// Статусы операций
struct BoundaryStatus {
  bool hasOuterBoundary;  // Внешняя граница установлена?
  bool hasInnerBoundary;  // Внутренняя граница установлена?
  int outerPointCount;    // Сколько точек во внешней границе
  int innerPointCount;    // Сколько точек во внутренней границе

  BoundaryStatus()
      : hasOuterBoundary(false),
        hasInnerBoundary(false),
        outerPointCount(0),
        innerPointCount(0) {}
};

}  // namespace Interferometry
