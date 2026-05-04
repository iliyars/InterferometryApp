#pragma once

/**
 * @file PolynomialApproximator.h
 * @brief Аппроксимация ортогональными полиномами Форсайта.
 *
 * Портировано из SCAN360/APPROXIM.C.
 */

#include <string>
#include <vector>

namespace Interferometry
{

  struct CTracerPoint;

  // ============================================================================
  // Результат аппроксимации
  // ============================================================================

  struct ApproximationResult
  {
    bool valid = false;
    int degree = 0;
    std::vector<double> coefficients; // обычные коэф-ты полинома
    double xMin = 0.0;
    double xMax = 0.0;

    /// Вычислить значение полинома в точке x (схема Горнера)
    double Evaluate(double x) const;
  };

  // ============================================================================
  // Аппроксиматор
  // ============================================================================

  class CPolynomialApproximator
  {
  public:
    static constexpr int MAX_DEGREE = 20;
    static constexpr int MAX_POINTS = 2000;

    CPolynomialApproximator();
    ~CPolynomialApproximator();

    // --- Публичный интерфейс ---

    /// Аппроксимация по вектору точек трассировщика
    ApproximationResult Approximate(const std::vector<CTracerPoint> &points,
                                    int degree);

    /// Аппроксимация по готовым массивам (x монотонен)
    ApproximationResult Approximate(const double *xx, const double *yy,
                                    int numPoints, int degree);

    const std::string &GetLastError() const { return m_lastError; }

  private:
    // --- Подготовка точек ---
    bool PreparePoints(const std::vector<CTracerPoint> &points);

    // --- Ядро алгоритма Форсайта ---
    bool ComputeApproximation();

    // --- Вспомогательные ---
    double EvalPolynomial(double x, int degree, const double *coeffs) const;
    double ComputeAlpha(int k) const;
    double ComputeBeta(int k1, int k2) const;
    double ComputeNorm(int k) const;

    // --- Данные ---
    std::vector<double> m_xx; // независимая ось (монотонная)
    std::vector<double> m_yy; // зависимая ось

    int m_numPoints;
    int m_degree;

    double m_orthoCoeffs[MAX_DEGREE][MAX_DEGREE]; // коэф-ты ортогональных полиномов
    double m_projections[MAX_DEGREE];             // проекции <y, Qk>
    double m_coefficients[MAX_DEGREE];            // итоговые коэф-ты полинома

    std::string m_lastError;
  };

} // namespace Interferometry
