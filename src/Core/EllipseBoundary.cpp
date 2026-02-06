// EllipseBoundary.cpp
// Реализация класса для установки элептических границ
// Портировано из SCAN360/MARKER.C

#include <EllipseBoundary.h>

#include <algorithm>
#include <cmath>

namespace Interferometry {

CEllipseBoundary::CEllipseBoundary() : m_imageWidth(360), m_imageHeight(290) {
  m_boundaries.resize(m_imageHeight);
}

CEllipseBoundary::~CEllipseBoundary() {}

void CEllipseBoundary::Initialize(int imageWidth, int imageHeight) {
  m_imageWidth = imageWidth;
  m_imageHeight = imageHeight;
  m_boundaries.clear();
  m_boundaries.resize(m_imageHeight);

  // По умолчанию весь кадр доступен
  SetDefaultBoundaries();
}
void CEllipseBoundary::Clear() {
  for (auto& boundary : m_boundaries) {
    boundary = RowBoundary();
  }
}

//=============================================================================
// Установка эллипса
//=============================================================================
void CEllipseBoundary::SetEllipse(const EllipseParams& ellipse, bool isOuter) {
  if (!ellipse.IsValid()) return;

  if (isOuter) {
    ApplyOuterEllipse(ellipse);
  } else {
    ApplyInnerEllipse(ellipse);
  }
}

//=============================================================================
// Применение внешнего эллипса (из ell_bld в MARKER.C)
//=============================================================================
void CEllipseBoundary::ApplyOuterEllipse(const EllipseParams& ellipse) {
  int x_center = ellipse.centerX;
  int y_center = ellipse.centerY;
  int a = ellipse.semiAxisA;
  int b = ellipse.semiAxisB;

  // Найти первую строку с установленными границами
  int firstRow = 0;
  for (int i = 0; i < m_imageHeight; i++) {
    if (m_boundaries[i].HasOuterBoundary()) {
      firstRow = i;
      break;
    }
  }

  // Если нет установленных границ, установить максимальны
  bool noExistingBoundaries =
      (firstRow == 0 && !m_boundaries[0].HasInnerBoundary());

  if (noExistingBoundaries) {
    for (int i = 0; i < m_imageHeight; i++) {
      m_boundaries[i].rightOuter = m_imageWidth - 1;
      m_boundaries[i].leftOuter = 1;
    }
  }

  // Очистить строки выше верхней границы эллипса
  for (int i = 0; i <= y_center - b && i < m_imageHeight; i++) {
    m_boundaries[i].leftOuter = 0;
    m_boundaries[i].rightOuter = 0;
  }

  // Вычислить границы для строк внутри эллипса
  int edge = std::min(y_center + b, m_imageHeight - 1);
  for (int i = std::max(0, y_center - b); i < edge; i++) {
    float x1, x2;
    CalculateEllipsePoints(ellipse, i, x1, x2);

    // Применить ограничения как в оригинале
    if (m_boundaries[i].leftOuter < x1 && m_boundaries[i].rightOuter > x1)
      m_boundaries[i].leftOuter = (int)x1;
    if (m_boundaries[i].rightOuter > x2 && x2 > m_boundaries[i].leftOuter)
      m_boundaries[i].rightOuter = (int)x2;
  }
  // Очистить строки ниже границы эллипса
  for (int i = edge; i < m_imageHeight; i++) {
    m_boundaries[i].leftOuter = 0;
    m_boundaries[i].rightOuter = 0;
  }
}

//=============================================================================
// Применение внутреннего эллипса (из ell_small в MARKER.C)
//=============================================================================
void CEllipseBoundary::ApplyInnerEllipse(const EllipseParams& ellipse) {
  int x_centr = ellipse.centerX;
  int y_centr = ellipse.centerY;
  int a = ellipse.semiAxisA;
  int b = ellipse.semiAxisB;

  // Найти первую строку с установленными внутренними границами
  int firstRow = 0;
  for (int i = 0; i < m_imageHeight; i++) {
    if (m_boundaries[i].HasInnerBoundary()) {
      firstRow = i;
      break;
    }
  }

  // Очистить строки выше верхней границы
  for (int i = 0; i < m_imageHeight && i <= y_centr - b; i++) {
    if (!m_boundaries[i].HasInnerBoundary()) {
      m_boundaries[i].leftInner = 0;
      m_boundaries[i].rightInner = 0;
    }
  }

  // Вычислить границы для строк внутри эллипса
  int edge = std::min(y_centr + b, m_imageHeight - 1);
  for (int i = std::max(0, y_centr - b); i < edge; i++) {
    float x1, x2;
    CalculateEllipsePoints(ellipse, i, x1, x2);

    // Применить ограничения как в оригинале (ell_small)
    const RowBoundary& row = m_boundaries[i];

    // Левая внутренняя граница
    if ((row.leftInner > x1 && x1 >= row.leftOuter && x1 < row.rightOuter) ||
        (row.leftInner == 0 && x1 >= row.leftOuter && x1 < row.rightOuter)) {
      m_boundaries[i].leftInner = (int)x1;
    }

    // Правая внутренняя граница
    if (row.rightInner < x2 && x2 <= row.rightOuter) {
      m_boundaries[i].rightInner = (int)x2;
    }
  }

  // Очистить строки ниже нижней границы эллипса
  for (int i = edge; i < m_imageHeight; i++) {
    if (m_boundaries[i].leftInner == 0 && m_boundaries[i].rightInner == 0) {
      continue;  // Уже очищено
    }
    m_boundaries[i].leftInner = 0;
    m_boundaries[i].rightInner = 0;
  }
}

//=============================================================================
// Вычисление точек эллипса для заданной строки
//=============================================================================
void CEllipseBoundary::CalculateEllipsePoints(const EllipseParams& ellipse,
                                              int row, float& x1,
                                              float& x2) const {
  int x_centr = ellipse.centerX;
  int y_centr = ellipse.centerY;
  int a = ellipse.semiAxisA;
  int b = ellipse.semiAxisB;

  // Формула эллипса: x^2/a^2 + y^2/b^2 = 1
  // Отсюда x = a * sqrt(1 - y^2/b^2)
  float buf = (float)(row - y_centr);
  float x = (float)a * std::sqrt(1.0f - buf * buf / ((float)b * (float)b));

  x1 = x_centr - x;
  x2 = x_centr + x;

  // Ограничение координат
  if (x1 <= 0) x1 = 1;
  if (x2 >= m_imageWidth - 1) x2 = (float)(m_imageWidth - 2);
}

//=============================================================================
// Получение границ строки
//=============================================================================
const RowBoundary& CEllipseBoundary::GetRowBoundary(int row) const {
  static RowBoundary emptyBoundary;
  if (row < 0 || row >= m_imageHeight) {
    return emptyBoundary;
  }
  return m_boundaries[row];
}

RowBoundary& CEllipseBoundary::GetRowBoundary(int row) {
  static RowBoundary emptyBoundary;
  if (row < 0 || row >= m_imageHeight) {
    return emptyBoundary;
  }
  return m_boundaries[row];
}

//=============================================================================
// Проверка точки
//=============================================================================
bool CEllipseBoundary::IsInside(int x, int y) const {
  if (y < 0 || y >= m_imageHeight) {
    return false;
  }

  const RowBoundary& boundary = m_boundaries[y];
  return boundary.IsInside(x);
}

bool CEllipseBoundary::IsInsideOuter(int x, int y) const {
  if (y < 0 || y >= m_imageHeight) {
    return false;
  }

  const RowBoundary& boundary = m_boundaries[y];
  return boundary.IsInsideOuter(x);
}

bool CEllipseBoundary::IsInsideInner(int x, int y) const {
  if (y < 0 || y >= m_imageHeight) {
    return false;
  }

  const RowBoundary& boundary = m_boundaries[y];
  return boundary.IsInsideInner(x);
}

//=============================================================================
// Сброс границ
//=============================================================================
void CEllipseBoundary::ResetOuterBoundaries() {
  for (auto& boundary : m_boundaries) {
    boundary.leftOuter = 1;
    boundary.rightOuter = m_imageWidth - 2;
  }
}

void CEllipseBoundary::ResetInnerBoundaries() {
  for (auto& boundary : m_boundaries) {
    boundary.leftInner = 0;
    boundary.rightInner = 0;
  }
}

void CEllipseBoundary::ResetAllBoundaries() {
  ResetOuterBoundaries();
  ResetInnerBoundaries();
}

void CEllipseBoundary::SetDefaultBoundaries() {
  for (int i = 0; i < m_imageHeight; i++) {
    m_boundaries[i].leftOuter = 1;
    m_boundaries[i].rightOuter = m_imageWidth - 2;
    m_boundaries[i].leftInner = 0;
    m_boundaries[i].rightInner = 0;
  }
}

//=============================================================================
// Копирование границ
//=============================================================================
void CEllipseBoundary::CopyFrom(const CEllipseBoundary& other) {
  m_imageWidth = other.m_imageWidth;
  m_imageHeight = other.m_imageHeight;
  m_boundaries = other.m_boundaries;
}

//=============================================================================
// Валидация
//=============================================================================
bool CEllipseBoundary::Validate() const {
  for (int i = 0; i < m_imageHeight; i++) {
    const RowBoundary& boundary = m_boundaries[i];

    // Проверка порядка координат
    if (boundary.HasOuterBoundary()) {
      if (boundary.leftOuter >= boundary.rightOuter) {
        return false;
      }
      if (boundary.leftOuter < 0 || boundary.rightOuter >= m_imageWidth) {
        return false;
      }
    }

    if (boundary.HasInnerBoundary()) {
      if (boundary.leftInner >= boundary.rightInner) {
        return false;
      }
      // Внутренние границы должны быть внутри внешних
      if (boundary.HasOuterBoundary()) {
        if (boundary.leftInner < boundary.leftOuter ||
            boundary.rightInner > boundary.rightOuter) {
          return false;
        }
      }
    }
  }

  return true;
}

//=============================================================================
// Вспомогательные функции
//=============================================================================
int CEllipseBoundary::ClampX(int x) const {
  if (x < 0) return 0;
  if (x >= m_imageWidth) return m_imageWidth - 1;
  return x;
}

int CEllipseBoundary::ClampY(int y) const {
  if (y < 0) return 0;
  if (y >= m_imageHeight) return m_imageHeight - 1;
  return y;
}
}  // namespace Interferometry
