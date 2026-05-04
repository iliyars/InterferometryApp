// EllipseBoundary.h
// Установка и управление эллиптическими границами рабочей области
// Портировано из SCAN360/MARKER.C
#pragma once
#include <vector>
namespace Interferometry
{
  // Структура для хранения границ одной строки
  struct RowBoundary
  {
    int leftOuter;  // левая внешняя граница (arr_coord_ell[i][0])
    int leftInner;  // левая внутренняя граница (arr_coord_ell[i][1])
    int rightInner; // правая внутренняя граница (arr_coord_ell[i][2])
    int rightOuter; // правая внешняя граница (arr_coord_ell[i][3])
    RowBoundary() : leftOuter(0), leftInner(0), rightInner(0), rightOuter(0) {}
    bool HasOuterBoundary() const { return (leftOuter != 0 || rightOuter != 0); }
    bool HasInnerBoundary() const { return (leftInner != 0 || rightInner != 0); }
    bool IsInsideOuter(int x) const
    {
      if (!HasOuterBoundary())
        return true;
      return (x >= leftOuter && x <= rightOuter);
    }
    bool IsInsideInner(int x) const
    {
      if (!HasInnerBoundary())
        return false; // нет внутренней границы — нет дыры
      return (x >= leftInner && x <= rightInner);
    }
    bool IsInside(int x) const { return IsInsideOuter(x) && !IsInsideInner(x); }
  };
  // Параметры эллипса
  struct EllipseParams
  {
    int centerX;   // Центр по X
    int centerY;   // Центр по Y
    int semiAxisA; // Большая полуось (по X)
    int semiAxisB; // Малая полуось (по Y)
    float angle;   // Угол наклона в градусах (из cv::fitEllipse)

    EllipseParams()
        : centerX(0), centerY(0), semiAxisA(0), semiAxisB(0), angle(0.0f) {}

    EllipseParams(int cx, int cy, int a, int b, float ang = 0.0f)
        : centerX(cx), centerY(cy), semiAxisA(a), semiAxisB(b), angle(ang) {}

    bool IsValid() const { return (semiAxisA > 0 && semiAxisB > 0); }
  };
  class CEllipseBoundary
  {
  public:
    CEllipseBoundary();
    ~CEllipseBoundary();
    void Initialize(int imageWidth, int imageHeight);
    void Clear();
    int GetImageWidth() const { return m_imageWidth; }
    int GetImageHeight() const { return m_imageHeight; }
    void SetEllipse(const EllipseParams &ellipse, bool isOuter);
    const RowBoundary &GetRowBoundary(int row) const;
    RowBoundary &GetRowBoundary(int row);
    bool IsInside(int x, int y) const;
    bool IsInsideOuter(int x, int y) const;
    bool IsInsideInner(int x, int y) const;
    const std::vector<RowBoundary> &GetAllBoundaries() const
    {
      return m_boundaries;
    }
    void ResetOuterBoundaries();
    void ResetInnerBoundaries();
    void ResetAllBoundaries();
    void SetDefaultBoundaries();
    void CopyFrom(const CEllipseBoundary &other);
    bool Validate() const;

  private:
    void CalculateEllipsePoints(const EllipseParams &ellipse, int row,
                                float &x1, float &x2) const;
    void ApplyOuterEllipse(const EllipseParams &ellipse);
    void ApplyInnerEllipse(const EllipseParams &ellipse);
    int ClampX(int x) const;
    int ClampY(int y) const;
    int m_imageWidth;
    int m_imageHeight;
    std::vector<RowBoundary> m_boundaries;
  };
} // namespace Interferometry
