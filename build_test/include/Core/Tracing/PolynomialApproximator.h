/**
 * @file PolynomialApproximator.h
 * @brief Аппроксимация трассированных полос ортогональными полиномами Форсайта.
 *
 * Портировано из SCAN360/APPROXIM.C.
 *
 * @section approx_overview Общее описание
 *
 * После трассировки полоса представлена набором дискретных точек {x, y}.
 * Эти точки зашумлены — трассировщик шагает с дискретностью в пиксель
 * и не всегда точно попадает на гребень. Аппроксимация сглаживает
 * полосу, заменяя набор точек гладким полиномом y(x).
 *
 * @section approx_method Метод Форсайта
 *
 * Используются ортогональные полиномы, построенные рекуррентно:
 * @f[
 *   A \cdot Q_k(x) = x \cdot Q_{k-1}(x) - \alpha \cdot Q_{k-1}(x) - \beta \cdot Q_{k-2}(x)
 * @f]
 * где:
 * - @f$ \alpha = \sum_j x_j \cdot Q_{k-1}(x_j)^2 @f$
 * - @f$ \beta  = \sum_j x_j \cdot Q_{k-1}(x_j) \cdot Q_{k-2}(x_j) @f$
 * - @f$ A = \sqrt{\sum_j Q_k(x_j)^2} @f$ (нормировка)
 *
 * Ортогональность гарантирует численную устойчивость даже для
 * полиномов высокой степени (до 19 в оригинале).
 *
 * @section approx_result Результат
 *
 * Коэффициенты обычного полинома @f$ y = c_0 + c_1 x + c_2 x^2 + \ldots @f$,
 * полученные пересчётом из ортогонального базиса.
 *
 * @section approx_mapping Соответствие оригиналу APPROXIM.C
 *
 * | Оригинал                        | Порт (C++)                           |
 * |---------------------------------|--------------------------------------|
 * | approxim(int line)              | Approximate(points, degree)          |
 * | a[20][20]                       | m_orthoCoeffs                        |
 * | b[20]                           | m_projections                        |
 * | approx_lines[line][0..19]       | m_coefficients (результат)           |
 * | alfa(rang)                      | ComputeAlpha(k)                      |
 * | beta(rang1, rang2)              | ComputeBeta(k1, k2)                  |
 * | norma(rang)                     | ComputeNorm(k)                       |
 * | poly(x, deg, coeffs)            | EvalPolynomial(x, deg, coeffs)       |
 * | num_point, xx[], yy[]           | m_numPoints, m_xx, m_yy              |
 * | step_pol                        | m_degree                             |
 */

#pragma once

#include <string>
#include <vector>

namespace Interferometry
{

  struct CTracerPoint;

  /**
   * @brief Результат аппроксимации одной полосы.
   */
  struct ApproximationResult
  {
    std::vector<double> coefficients; ///< Коэффициенты полинома c[0] + c[1]*x + c[2]*x² + …
    int degree;                       ///< Степень полинома.
    double xMin;                      ///< Минимальный x (начало полосы).
    double xMax;                      ///< Максимальный x (конец полосы).
    bool valid;                       ///< Аппроксимация успешна.

    ApproximationResult() : degree(0), xMin(0), xMax(0), valid(false) {}

    /**
     * @brief Вычислить значение аппроксимирующего полинома в точке x.
     * @param x Координата (в пределах [xMin, xMax]).
     * @return y(x) = c[0] + c[1]*x + c[2]*x² + …
     */
    double Evaluate(double x) const;
  };

  /**
   * @brief Аппроксимация кривых ортогональными полиномами Форсайта.
   *
   * Порт APPROXIM.C из SCAN360.
   *
   * @par Использование
   *
   * @code
   *   CPolynomialApproximator approx;
   *
   *   // Из трассированных точек
   *   auto result = approx.Approximate(tracedPoints, 8);
   *
   *   // Вычислить гладкую кривую
   *   for (double x = result.xMin; x <= result.xMax; x += 1.0) {
   *     double y = result.Evaluate(x);
   *     // отрисовать (y, x) — NB: в интерферограмме x=строка, y=столбец
   *   }
   * @endcode
   *
   * @par Оси координат
   *
   * В оригинале APPROXIM.C полоса аппроксимируется как y(x),
   * где x — координата, вдоль которой полоса монотонна.
   * Оригинал автоматически выбирает ось:
   * - Если y[0] < y[1] (полоса идёт «вниз») — прямой порядок
   * - Если y[0] > y[1] (полоса идёт «вверх») — реверс точек
   * - Если y[0] == y[1] — ошибка (нет монотонности)
   *
   * В порте эта логика сохранена в методе PreparePoints().
   */
  class CPolynomialApproximator
  {
  public:
    CPolynomialApproximator();
    ~CPolynomialApproximator();

    /**
     * @brief Аппроксимация набора трассированных точек.
     * @param points Точки полосы (из CFringeTracer::TraceLine).
     * @param degree Желаемая степень полинома (2–19). Будет уменьшена
     *               если точек недостаточно (нужно >= degree + 2).
     * @return Результат с коэффициентами полинома.
     */
    ApproximationResult Approximate(const std::vector<CTracerPoint> &points,
                                    int degree);

    /**
     * @brief Аппроксимация произвольных массивов x[], y[].
     * @param xx       Массив x-координат (должен быть монотонным).
     * @param yy       Массив y-координат (зависимая переменная).
     * @param numPoints Количество точек.
     * @param degree   Степень полинома.
     * @return Результат с коэффициентами.
     */
    ApproximationResult Approximate(const double *xx, const double *yy,
                                    int numPoints, int degree);

    /** @brief Максимальная поддерживаемая степень полинома. */
    static constexpr int MAX_DEGREE = 19;

    /** @brief Текст последней ошибки. */
    std::string GetLastError() const { return m_lastError; }

  private:
    /**
     * @brief Подготовка точек: выбор осей и обеспечение монотонности.
     * @param points Исходные точки.
     * @return true если точки подготовлены успешно.
     *
     * Порт блока APPROXIM.C:71-94.
     * Определяет, по какой оси полоса монотонна,
     * и при необходимости реверсирует порядок.
     */
    bool PreparePoints(const std::vector<CTracerPoint> &points);

    /**
     * @brief Построение ортогонального базиса и вычисление коэффициентов.
     * @return true если вычисление успешно.
     *
     * Порт основного цикла APPROXIM.C:117-191.
     */
    bool ComputeApproximation();

    /**
     * @brief Вычисление полинома: p(x) = c[0] + c[1]*x + c[2]*x² + … + c[deg]*x^deg.
     * @param x      Аргумент.
     * @param degree Степень полинома.
     * @param coeffs Массив коэффициентов [0..degree].
     * @return Значение полинома.
     *
     * Порт функции poly() из work.h/work.c.
     * Использует схему Горнера для числовой устойчивости.
     */
    static double EvalPolynomial(double x, int degree, const double *coeffs);

    /**
     * @brief Коэффициент α рекуррентного соотношения Форсайта.
     * @param k Номер полинома (Q_{k}).
     * @return @f$ \alpha = \sum_j x_j \cdot Q_k(x_j)^2 @f$
     *
     * Порт alfa() из APPROXIM.C:209-218.
     */
    double ComputeAlpha(int k) const;

    /**
     * @brief Коэффициент β рекуррентного соотношения Форсайта.
     * @param k1 Номер первого полинома (Q_{k1}).
     * @param k2 Номер второго полинома (Q_{k2}).
     * @return @f$ \beta = \sum_j x_j \cdot Q_{k1}(x_j) \cdot Q_{k2}(x_j) @f$
     *
     * Порт beta() из APPROXIM.C:221-232.
     */
    double ComputeBeta(int k1, int k2) const;

    /**
     * @brief Квадрат нормы ортогонального полинома.
     * @param k Номер полинома.
     * @return @f$ \|Q_k\|^2 = \sum_j Q_k(x_j)^2 @f$
     *
     * Порт norma() из APPROXIM.C:234-245.
     */
    double ComputeNorm(int k) const;

    /// @name Рабочие данные
    /// @{
    std::vector<double> m_xx; ///< Независимая переменная (монотонная ось).
    std::vector<double> m_yy; ///< Зависимая переменная.
    int m_numPoints;          ///< Количество точек. Оригинал: num_point.
    int m_degree;             ///< Степень полинома. Оригинал: step_pol.

    /**
     * @brief Коэффициенты ортогональных полиномов.
     *
     * m_orthoCoeffs[k][j] — коэффициент при x^j в Q_k(x).
     * Оригинал: a[20][20].
     */
    double m_orthoCoeffs[MAX_DEGREE + 1][MAX_DEGREE + 1];

    /**
     * @brief Проекции y на ортогональный базис.
     *
     * m_projections[k] = <y, Q_k> = Σ y_j * Q_k(x_j).
     * Оригинал: b[20].
     */
    double m_projections[MAX_DEGREE + 1];

    /**
     * @brief Результирующие коэффициенты обычного полинома.
     *
     * m_coefficients[j] — коэффициент при x^j.
     * Оригинал: approx_lines[line][0..19].
     */
    double m_coefficients[MAX_DEGREE + 1];
    /// @}

    std::string m_lastError; ///< Описание последней ошибки.
  };

} // namespace Interferometry
