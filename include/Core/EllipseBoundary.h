
// EllipseBoundary.h
// Установка и управление эллиптическими границами рабочей области
// Портировано из SCAN360/MARKER.C

#pragma once

#include <vector>

namespace Interferometry {

// Структура для хранения границ одной строки
struct RowBoundary {
  int leftOuter;   // левая внешняя граница (arr_coord_ell[i][0])
  int leftInner;   // левая внутренняя граница (arr_coord_ell[i][1])
  int rightInner;  // правая внутренняя граница (arr_coord_ell[i][2])
  int rightOuter;  // правая внешняя граница (arr_coord_ell[i][3])

  RowBoundary() : leftOuter(0), leftInner(0), rightInner(0), rightOuter(0) {}

  // Проверка, установлены ли границы
  bool HasOuterBoundary() const { return (leftOuter != 0 || rightOuter != 0); }
  bool HasInnerBoundary() const { return (leftInner != 0 || rightInner != 0); }

  // Проверка, находится ли точка внутри границ
  bool IsInsideOuter(int x) const {
    if (!HasOuterBoundary()) return true;
    return (x >= leftOuter && x <= rightOuter);
  }

  bool IsInsideInner(int x) const {
    if (!HasInnerBoundary()) return true;
    return (x >= leftInner && x <= rightInner);
  }

  bool IsInside(int x) const { return IsInsideOuter(x) && !IsInsideInner(x); }
};

// Параметры элипса
struct EllipseParams {
  int centerX;    // Центр по Х
  int centerY;    // Центр по Y
  int semiAxisA;  // Большая полуось (по Х)
  int semiAxisB;  // Малая полуось (по Y)

  EllipseParams() : centerX(0), centerY(0), semiAxisA(0), semiAxisB(0) {}

  EllipseParams(int cx, int cy, int a, int b)
      : centerX(cx), centerY(cy), semiAxisA(a), semiAxisB(b) {}

  bool IsValid() const { return (semiAxisA > 0 && semiAxisB > 0); }
};

class CEllipseBoundary {
 public:
  CEllipseBoundary();
  ~CEllipseBoundary();

  // Инициализация
  void Initialize(int imageWidth, int imageHeight);
  void Clear();

  // Получение размеров
  int GetImageWidth() const { return m_imageWidth; }
  int GetImageHeight() const { return m_imageHeight; }

  // Установка границ эллипсом
  // isOuter: true - внешние границы (Space), flase - внутреняя (Enter)
  void SetEllipse(const EllipseParams& ellipse, bool isOuter);

  // Получение границ для строки
  const RowBoundary& GetRowBoundary(int row) const;
  RowBoundary& GetRowBoundary(int row);

  // Проверка точки
  bool IsInside(int x, int y) const;
  bool IsInsideOuter(int x, int y) const;
  bool IsInsideInner(int x, int y) const;

  // Получение всех границ
  const std::vector<RowBoundary>& GetAllBoundaries() const {
    return m_boundaries;
  }

  // Сброс границ
  void ResetOuterBoundaries();
  void ResetInnerBoundaries();
  void ResetAllBoundaries();

  // Установка границ по умолчанию (весь кадр)
  void SetDefaultBoundaries();

  // копирование границ
  void CopyFrom(const CEllipseBoundary& other);

  bool Validate() const;

 private:
  // Вычисление точек эллипса для заданной строки
  void CalculateEllipsePoints(const EllipseParams& ellipse, int row, float& x1,
                              float& x2) const;

  // Применение эллипса к границам
  void ApplyOuterEllipse(const EllipseParams& ellipse);
  void ApplyInnerEllipse(const EllipseParams& ellipse);

  // Валидация координат
  int ClampX(int x) const;
  int ClampY(int y) const;

  int m_imageWidth;   // Ширина изображения
  int m_imageHeight;  // Высота изображения

  std::vector<RowBoundary>
      m_boundaries;  // Граница для каждой строки (аналог arr_coord_ell[290][4])
};
}  // namespace Interferometry
