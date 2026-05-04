/**
 * @file PolynomialApproximator.cpp
 * @brief Реализация аппроксимации ортогональными полиномами Форсайта.
 *
 * Портировано из SCAN360/APPROXIM.C.
 *
 * @par Структура оригинала → порт
 *
 * - APPROXIM.C:40-206  (approxim)  → Approximate(), PreparePoints(), ComputeApproximation()
 * - APPROXIM.C:209-218 (alfa)      → ComputeAlpha()
 * - APPROXIM.C:221-232 (beta)      → ComputeBeta()
 * - APPROXIM.C:234-245 (norma)     → ComputeNorm()
 * - work.c: poly()                  → EvalPolynomial()
 *
 * Убрано: графика (pix, outtextxy), DOS-аллокация (farcalloc),
 * глобальные переменные, модификация curve_line на месте.
 */

#include "pch.h"
#include "PolynomialApproximator.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "FringeTracer.h"

namespace Interferometry
{

  //=============================================================================
  // ApproximationResult
  //=============================================================================

  /**
   * @details
   * Вычисляет полином по схеме Горнера:
   * y = c[n] * x^n + … + c[1] * x + c[0]
   *   = ((c[n] * x + c[n-1]) * x + c[n-2]) * x + … + c[0]
   */
  double ApproximationResult::Evaluate(double x) const
  {
    if (!valid || coefficients.empty())
      return 0.0;

    int deg = (int)coefficients.size() - 1;
    double result = coefficients[deg];
    for (int i = deg - 1; i >= 0; i--)
    {
      result = result * x + coefficients[i];
    }
    return result;
  }

  //=============================================================================
  // Конструктор / Деструктор
  //=============================================================================

  CPolynomialApproximator::CPolynomialApproximator()
      : m_numPoints(0), m_degree(0)
  {
    std::memset(m_orthoCoeffs, 0, sizeof(m_orthoCoeffs));
    std::memset(m_projections, 0, sizeof(m_projections));
    std::memset(m_coefficients, 0, sizeof(m_coefficients));
  }

  CPolynomialApproximator::~CPolynomialApproximator() {}

  //=============================================================================
  // Публичные методы
  //=============================================================================

  /**
   * @details
   * Преобразует CTracerPoint → double[], вызывает PreparePoints()
   * для выбора монотонной оси, затем ComputeApproximation().
   */
  ApproximationResult CPolynomialApproximator::Approximate(
      const std::vector<CTracerPoint> &points, int degree)
  {
    ApproximationResult result;
    m_lastError.clear();

    if (points.size() < 3)
    {
      m_lastError = "Недостаточно точек для аппроксимации (нужно >= 3)";
      return result;
    }

    if (degree < 1)
      degree = 1;
    if (degree > MAX_DEGREE)
      degree = MAX_DEGREE;
    m_degree = degree;

    if (!PreparePoints(points))
    {
      return result;
    }

    if (!ComputeApproximation())
    {
      return result;
    }

    // Заполнить результат
    result.degree = m_degree;
    result.coefficients.assign(m_coefficients, m_coefficients + m_degree);
    result.xMin = m_xx.front();
    result.xMax = m_xx.back();
    result.valid = true;

    return result;
  }

  /**
   * @details
   * Прямой вход: массивы уже подготовлены (монотонный x).
   * Не вызывает PreparePoints().
   */
  ApproximationResult CPolynomialApproximator::Approximate(
      const double *xx, const double *yy, int numPoints, int degree)
  {
    ApproximationResult result;
    m_lastError.clear();

    if (numPoints < 3)
    {
      m_lastError = "Недостаточно точек (нужно >= 3)";
      return result;
    }

    if (degree < 1)
      degree = 1;
    if (degree > MAX_DEGREE)
      degree = MAX_DEGREE;

    m_xx.assign(xx, xx + numPoints);
    m_yy.assign(yy, yy + numPoints);
    m_numPoints = numPoints;
    m_degree = degree;

    // Ограничение степени по количеству точек (APPROXIM.C:95)
    if (m_numPoints < m_degree + 2)
    {
      m_degree = m_numPoints - 2;
    }
    if (m_degree < 1)
    {
      m_lastError = "Степень полинома < 1 после ограничения по числу точек";
      return result;
    }

    if (!ComputeApproximation())
    {
      return result;
    }

    result.degree = m_degree;
    result.coefficients.assign(m_coefficients, m_coefficients + m_degree);
    result.xMin = m_xx.front();
    result.xMax = m_xx.back();
    result.valid = true;

    return result;
  }

  //=============================================================================
  // Подготовка точек
  //=============================================================================

  /**
   * @details
   * Порт APPROXIM.C:71-101.
   *
   * Оригинал хранит точки как curve_line[line][2*i] (x), [2*i+1] (y).
   * Для аппроксимации нужна монотонная ось (параметр кривой).
   * Оригинал использует y-координату (вертикальную) как независимую:
   *
   * @code
   *   // APPROXIM.C:72-76: если y[0] < y[1] — прямой порядок
   *   yy[i] = curve_line[line][2*i];     // ← это x изображения → "y" функции
   *   xx[i] = curve_line[line][2*i+1];   // ← это y изображения → "x" аргумент
   * @endcode
   *
   * То есть в оригинале:
   * - xx[] = строка изображения (независимая, монотонная по ходу полосы)
   * - yy[] = столбец изображения (зависимая, аппроксимируемая)
   *
   * Порт сохраняет эту семантику:
   * - m_xx[] ← координата, вдоль которой полоса монотонна
   * - m_yy[] ← другая координата (аппроксимируемая)
   *
   * Если полоса горизонтальна (x монотонен) — m_xx = point.x, m_yy = point.y.
   * Если вертикальна (y монотонен) — m_xx = point.y, m_yy = point.x.
   */
  bool CPolynomialApproximator::PreparePoints(
      const std::vector<CTracerPoint> &points)
  {
    int n = (int)points.size();

    // Определить, по какой оси полоса более монотонна
    // (как в оригинале: сравнение y[0] с y[1] и y[2])
    // APPROXIM.C:71-77:
    //   if(curve_line[line][1] < curve_line[line][3] ||
    //      curve_line[line][1] < curve_line[line][5])
    //
    // [1] и [3] — это y-координаты точек 0 и 1
    // Если y растёт — y монотонна, берём y как независимую

    int dy01 = points[1].y - points[0].y;
    int dy02 = (n >= 3) ? (points[2].y - points[0].y) : dy01;
    int dx01 = points[1].x - points[0].x;
    int dx02 = (n >= 3) ? (points[2].x - points[0].x) : dx01;

    bool useYasIndependent = (std::abs(dy01) + std::abs(dy02)) >=
                             (std::abs(dx01) + std::abs(dx02));

    m_xx.resize(n);
    m_yy.resize(n);

    if (useYasIndependent)
    {
      // y — независимая (как в оригинале: xx[i] = curve_line[...][2*i+1])
      if (dy01 > 0 || dy02 > 0)
      {
        // Прямой порядок (APPROXIM.C:72-76)
        for (int i = 0; i < n; i++)
        {
          m_xx[i] = (double)points[i].y;
          m_yy[i] = (double)points[i].x;
        }
      }
      else if (dy01 < 0 || dy02 < 0)
      {
        // Реверс (APPROXIM.C:77-88)
        for (int i = 0; i < n; i++)
        {
          m_xx[i] = (double)points[n - 1 - i].y;
          m_yy[i] = (double)points[n - 1 - i].x;
        }
      }
      else
      {
        m_lastError = "Точки с одинаковой y-координатой — нет монотонности";
        return false;
      }
    }
    else
    {
      // x — независимая
      if (dx01 > 0 || dx02 > 0)
      {
        for (int i = 0; i < n; i++)
        {
          m_xx[i] = (double)points[i].x;
          m_yy[i] = (double)points[i].y;
        }
      }
      else if (dx01 < 0 || dx02 < 0)
      {
        for (int i = 0; i < n; i++)
        {
          m_xx[i] = (double)points[n - 1 - i].x;
          m_yy[i] = (double)points[n - 1 - i].y;
        }
      }
      else
      {
        m_lastError = "Точки с одинаковой x-координатой — нет монотонности";
        return false;
      }
    }

    m_numPoints = n;

    // Ограничение степени (APPROXIM.C:95)
    if (m_numPoints < m_degree + 2)
    {
      m_degree = m_numPoints - 2;
    }
    if (m_degree < 1)
    {
      m_lastError = "Недостаточно точек для полинома степени >= 1";
      return false;
    }

    return true;
  }

  //=============================================================================
  // Вычисление аппроксимации
  //=============================================================================

  /**
   * @details
   * Порт APPROXIM.C:117-191.
   *
   * @par Шаг 1 — Обнуление (APPROXIM.C:117-124)
   *
   * @par Шаг 2 — Полином 0-го порядка (APPROXIM.C:126-134)
   * @code
   *   a[0][0] = 1 / sqrt(num_point)   // Q0(x) = const = 1/√N
   * @endcode
   *
   * @par Шаг 3 — Полином 1-го порядка (APPROXIM.C:136-147)
   * @code
   *   a[1][0] = -mean(x);  a[1][1] = 1;  // Q1(x) = x - mean(x)
   *   нормируется на ||Q1||
   * @endcode
   *
   * @par Шаг 4 — Рекуррентное построение Q2…Q_{step_pol-1} (APPROXIM.C:166-176)
   * @code
   *   for k=2..step_pol-1:
   *     α = ComputeAlpha(k-1)
   *     β = ComputeBeta(k-2, k-1)
   *     a[k][j] = a[k-1][j-1] - α*a[k-1][j] - β*a[k-2][j]
   *     нормировка на ||Qk||
   * @endcode
   *
   * @par Шаг 5 — Проекции y на базис (APPROXIM.C:179-183)
   * @code
   *   b[k] = Σ yy[i] * Qk(xx[i])     // <y, Qk>
   * @endcode
   *
   * @par Шаг 6 — Пересчёт в обычные коэффициенты (APPROXIM.C:184-191)
   * @code
   *   c[i] = Σ b[j] * a[j][i]   для j=i..step_pol-1
   * @endcode
   */
  bool CPolynomialApproximator::ComputeApproximation()
  {
    // --- Шаг 1: обнуление ---
    std::memset(m_orthoCoeffs, 0, sizeof(m_orthoCoeffs));
    std::memset(m_projections, 0, sizeof(m_projections));
    std::memset(m_coefficients, 0, sizeof(m_coefficients));

    if (m_numPoints < 2)
    {
      m_lastError = "Число точек < 2";
      return false;
    }

    // --- Шаг 2: Q0(x) = 1/sqrt(N) ---
    // APPROXIM.C:134
    m_orthoCoeffs[0][0] = 1.0 / std::sqrt((double)m_numPoints);

    // --- Шаг 3: Q1(x) = (x - mean(x)) / ||Q1|| ---
    // APPROXIM.C:140-147
    double sumX = 0.0;
    for (int i = 0; i < m_numPoints; i++)
    {
      sumX += m_xx[i];
    }
    m_orthoCoeffs[1][0] = -sumX / (double)m_numPoints;
    m_orthoCoeffs[1][1] = 1.0;

    double norm1 = ComputeNorm(1);
    if (norm1 <= 0.0)
    {
      m_lastError = "Норма Q1 == 0 (все точки совпадают по x?)";
      return false;
    }
    double lambda = std::sqrt(norm1);
    m_orthoCoeffs[1][0] /= lambda;
    m_orthoCoeffs[1][1] /= lambda;

    // --- Шаг 4: рекуррентное построение Q2…Q_{degree-1} ---
    // APPROXIM.C:166-176
    for (int k = 2; k < m_degree; k++)
    {
      double alf = ComputeAlpha(k - 1);
      double bet = ComputeBeta(k - 2, k - 1);

      for (int j = 0; j <= k; j++)
      {
        // a[k][k-j] = a[k-1][k-j-1] - alf*a[k-1][k-j] - bet*a[k-2][k-j]
        double prev1 = (k - j - 1 >= 0) ? m_orthoCoeffs[k - 1][k - j - 1] : 0.0;
        double prev1_same = m_orthoCoeffs[k - 1][k - j];
        double prev2_same = m_orthoCoeffs[k - 2][k - j];
        m_orthoCoeffs[k][k - j] = prev1 - alf * prev1_same - bet * prev2_same;
      }

      double normK = ComputeNorm(k);
      if (normK <= 0.0)
      {
        m_lastError = "Норма Q" + std::to_string(k) + " == 0";
        return false;
      }
      lambda = std::sqrt(normK);
      for (int j = 0; j <= k; j++)
      {
        m_orthoCoeffs[k][j] /= lambda;
      }
    }

    // --- Шаг 5: проекции <y, Qk> ---
    // APPROXIM.C:179-183
    for (int k = 0; k < m_degree; k++)
    {
      m_projections[k] = 0.0;
      for (int i = 0; i < m_numPoints; i++)
      {
        m_projections[k] +=
            m_yy[i] * EvalPolynomial(m_xx[i], k, m_orthoCoeffs[k]);
      }
    }

    // --- Шаг 6: пересчёт в обычные коэффициенты ---
    // APPROXIM.C:184-191
    //   approx_lines[line][i] = Σ b[j] * a[j][i]  для j=i..step_pol-1
    for (int i = 0; i < m_degree; i++)
    {
      m_coefficients[i] = 0.0;
      for (int j = i; j < m_degree; j++)
      {
        m_coefficients[i] += m_projections[j] * m_orthoCoeffs[j][i];
      }
    }

    return true;
  }

  //=============================================================================
  // Вычисление полинома
  //=============================================================================

  /**
   * @details
   * Порт poly() из work.c (не показан в исходниках, восстановлен по вызовам).
   *
   * Вычисляет: c[0] + c[1]*x + c[2]*x² + … + c[degree]*x^degree
   * по схеме Горнера: result = ((c[n]*x + c[n-1])*x + c[n-2])*x + …
   */
  double CPolynomialApproximator::EvalPolynomial(double x, int degree,
                                                 const double *coeffs)
  {
    if (degree < 0)
      return 0.0;

    double result = coeffs[degree];
    for (int i = degree - 1; i >= 0; i--)
    {
      result = result * x + coeffs[i];
    }
    return result;
  }

  //=============================================================================
  // Коэффициенты рекуррентного соотношения
  //=============================================================================

  /**
   * @details
   * Порт alfa() из APPROXIM.C:209-218:
   * @code
   *   for(i=0; i<num_point; i++) {
   *     p = poly(xx[i], rang, a[rang]);
   *     s += xx[i] * p * p;
   *   }
   * @endcode
   */
  double CPolynomialApproximator::ComputeAlpha(int k) const
  {
    double s = 0.0;
    for (int i = 0; i < m_numPoints; i++)
    {
      double p = EvalPolynomial(m_xx[i], k, m_orthoCoeffs[k]);
      s += m_xx[i] * p * p;
    }
    return s;
  }

  /**
   * @details
   * Порт beta() из APPROXIM.C:221-232:
   * @code
   *   for(i=0; i<num_point; i++) {
   *     p1 = poly(xx[i], rang1, a[rang1]);
   *     p2 = poly(xx[i], rang2, a[rang2]);
   *     s += xx[i] * p1 * p2;
   *   }
   * @endcode
   */
  double CPolynomialApproximator::ComputeBeta(int k1, int k2) const
  {
    double s = 0.0;
    for (int i = 0; i < m_numPoints; i++)
    {
      double p1 = EvalPolynomial(m_xx[i], k1, m_orthoCoeffs[k1]);
      double p2 = EvalPolynomial(m_xx[i], k2, m_orthoCoeffs[k2]);
      s += m_xx[i] * p1 * p2;
    }
    return s;
  }

  /**
   * @details
   * Порт norma() из APPROXIM.C:234-245:
   * @code
   *   for(i=0; i<num_point; i++) {
   *     p = poly(xx[i], rang, a[rang]);
   *     s += p * p;
   *   }
   * @endcode
   */
  double CPolynomialApproximator::ComputeNorm(int k) const
  {
    double s = 0.0;
    for (int i = 0; i < m_numPoints; i++)
    {
      double p = EvalPolynomial(m_xx[i], k, m_orthoCoeffs[k]);
      s += p * p;
    }
    return s;
  }

} // namespace Interferometry
