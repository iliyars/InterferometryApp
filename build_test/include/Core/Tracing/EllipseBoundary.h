/**
 * @file EllipseBoundary.h
 * @brief Установка и управление эллиптическими границами рабочей области интерферограммы.
 *
 * Портировано из SCAN360/MARKER.C (функции ell_bld, ell_small)
 * и SCAN360/STEP.C (функция inside).
 *
 * @section overview Общее описание
 *
 * Интерферограмма — круглое (эллиптическое) изображение. Рабочая область —
 * кольцо между двумя эллипсами: внешним (граница оптического элемента) и
 * внутренним (мёртвая зона, например, отверстие в зеркале).
 *
 * Для каждой строки изображения хранятся 4 x-координаты:
 *
 * @code
 *   строка y:
 *
 *   0  [leftOuter]  [leftInner]  [rightInner]  [rightOuter]  WIDTH
 *   ├────┤═══════════├────────────┤═══════════════├────────────┤
 *   вне  │ РАБОЧАЯ   │  МЁРТВАЯ   │   РАБОЧАЯ     │   вне
 *        │ ЗОНА      │  ЗОНА      │   ЗОНА        │
 * @endcode
 *
 * @section original Соответствие оригиналу SCAN360
 *
 * | Оригинал (глобальное)            | Порт (C++)                             |
 * |----------------------------------|----------------------------------------|
 * | arr_coord_ell[y][0..3]           | CEllipseBoundary::m_boundaries[y]      |
 * | inside(x, y)                     | CEllipseBoundary::IsInside(x, y)       |
 * | ell_bld(a, b, cx, cy)            | CEllipseBoundary::SetEllipse(e, true)  |
 * | ell_small(a, b, cx, cy)          | CEllipseBoundary::SetEllipse(e, false) |
 * | Инициализация в WORK.C:189-195   | CEllipseBoundary::SetDefaultBoundaries() |
 */

#pragma once

#include <vector>

namespace Interferometry
{

  /**
   * @brief Границы рабочей области для одной строки изображения.
   *
   * Аналог одной строки массива @c arr_coord_ell[y][0..3] в SCAN360.
   *
   * Хранит x-координаты пересечения строки с двумя эллипсами:
   * - leftOuter / rightOuter — внешний эллипс (граница зрачка)
   * - leftInner / rightInner — внутренний эллипс (мёртвая зона)
   *
   * Значение 0 означает "граница не установлена".
   *
   * @note Инвариант (когда обе границы установлены):
   *       leftOuter <= leftInner <= rightInner <= rightOuter
   */
  struct RowBoundary
  {
    int leftOuter;  ///< Левая точка внешнего эллипса. Оригинал: arr_coord_ell[y][0]
    int leftInner;  ///< Левая точка внутреннего эллипса. Оригинал: arr_coord_ell[y][1]
    int rightInner; ///< Правая точка внутреннего эллипса. Оригинал: arr_coord_ell[y][2]
    int rightOuter; ///< Правая точка внешнего эллипса. Оригинал: arr_coord_ell[y][3]

    /** @brief Конструктор по умолчанию. Все границы = 0 (не установлены). */
    RowBoundary() : leftOuter(0), leftInner(0), rightInner(0), rightOuter(0) {}

    /**
     * @brief Установлена ли внешняя граница?
     * @return true если хотя бы одна из leftOuter/rightOuter != 0.
     *
     * В оригинале: строка "пустая" (вне эллипса) когда [0]==0 && [3]==0.
     */
    bool HasOuterBoundary() const { return (leftOuter != 0 || rightOuter != 0); }

    /**
     * @brief Установлена ли внутренняя граница (мёртвая зона)?
     * @return true если хотя бы одна из leftInner/rightInner != 0.
     */
    bool HasInnerBoundary() const { return (leftInner != 0 || rightInner != 0); }

    /**
     * @brief Находится ли точка x внутри внешнего эллипса?
     * @param x Координата x проверяемой точки.
     * @return true если leftOuter <= x <= rightOuter.
     *
     * Оригинал (STEP.C:653):
     * @code
     *   if(xt < arr_coord_ell[yt][0] || xt > arr_coord_ell[yt][3]) return(-1);
     * @endcode
     * Операторы @c < и @c > (нестрогие на границе) — точки НА границе считаются внутри.
     *
     * @note Если внешний эллипс не установлен (HasOuterBoundary()==false),
     *       возвращает false — строка считается вне рабочей области.
     */
    bool IsInsideOuter(int x) const
    {
      if (!HasOuterBoundary())
        return false;
      return (x >= leftOuter && x <= rightOuter);
    }

    /**
     * @brief Находится ли точка x внутри мёртвой зоны (внутреннего эллипса)?
     * @param x Координата x проверяемой точки.
     * @return true если leftInner < x < rightInner (строгие неравенства).
     *
     * Оригинал (STEP.C:654):
     * @code
     *   if(xt > arr_coord_ell[yt][1] && xt < arr_coord_ell[yt][2]) return(-1);
     * @endcode
     * Операторы @c > и @c < (строгие) — точки НА границе дырки считаются
     * снаружи дырки (т.е. доступны для трассировки).
     *
     * @note Если внутренний эллипс не установлен, возвращает false
     *       (нет мёртвой зоны — точка НЕ внутри дырки).
     */
    bool IsInsideInner(int x) const
    {
      if (!HasInnerBoundary())
        return false;
      return (x > leftInner && x < rightInner);
    }

    /**
     * @brief Находится ли точка в рабочей зоне (между двумя эллипсами)?
     * @param x Координата x проверяемой точки.
     * @return true если внутри внешнего эллипса И снаружи внутреннего.
     *
     * Эквивалент @c inside(x,y)==0 в оригинале.
     */
    bool IsInside(int x) const { return IsInsideOuter(x) && !IsInsideInner(x); }
  };

  /**
   * @brief Параметры одного эллипса.
   *
   * Описывает эллипс уравнением:
   * @f[
   *   \frac{(x - centerX)^2}{semiAxisA^2} + \frac{(y - centerY)^2}{semiAxisB^2} = 1
   * @f]
   *
   * В оригинале MARKER.C параметры передаются как аргументы:
   *   ell_bld(a, b, x_centr, y_centr).
   */
  struct EllipseParams
  {
    int centerX;   ///< Центр эллипса по X (x_centr в оригинале)
    int centerY;   ///< Центр эллипса по Y (y_centr в оригинале)
    int semiAxisA; ///< Полуось по X — горизонтальная (a в оригинале)
    int semiAxisB; ///< Полуось по Y — вертикальная (b в оригинале)
    float angle;  // < Градусы наклона эллипса
    /** @brief Конструктор по умолчанию (нулевой эллипс). */
    EllipseParams() : centerX(0), centerY(0), semiAxisA(0), semiAxisB(0), angle(0) {}

    /**
     * @brief Конструктор с параметрами.
     * @param cx Центр по X.
     * @param cy Центр по Y.
     * @param a  Полуось по X.
     * @param b  Полуось по Y.
     */
  EllipseParams(int cx, int cy, int a, int b, float ang = 0.0)
        : centerX(cx), centerY(cy), semiAxisA(a), semiAxisB(b), angle(ang)
    {
    }

    /** @brief Валиден ли эллипс (обе полуоси > 0)? */
    bool IsValid() const { return (semiAxisA > 0 && semiAxisB > 0); }
  };

  /**
   * @brief Хранилище эллиптических границ рабочей области интерферограммы.
   *
   * Аналог глобального массива @c arr_coord_ell[290][4] в SCAN360.
   *
   * @section lifecycle Жизненный цикл
   *
   * @code
   *   CEllipseBoundary boundary;
   *   boundary.Initialize(width, height);          // создаёт массив, весь кадр доступен
   *   boundary.SetEllipse(outerParams, true);      // внешний эллипс (Space в оригинале)
   *   boundary.SetEllipse(innerParams, false);     // внутренний эллипс (Enter в оригинале)
   *   bool ok = boundary.IsInside(x, y);           // проверка точки
   * @endcode
   *
   * @section behavior Поведение при множественных вызовах
   *
   * - Внешний эллипс **сужает** область: повторный вызов SetEllipse(_, true)
   *   даст пересечение двух эллипсов.
   * - Внутренний эллипс **расширяет** мёртвую зону: повторный вызов
   *   SetEllipse(_, false) даст объединение двух дырок.
   *
   * @section init Инициализация по умолчанию
   *
   * В оригинале (WORK.C:189-195):
   * @code
   *   arr_coord_ell[i][0] = 1;             // leftOuter = 1
   *   arr_coord_ell[i][1] = 0;             // leftInner = 0 (нет)
   *   arr_coord_ell[i][2] = 0;             // rightInner = 0 (нет)
   *   arr_coord_ell[i][3] = IMAGE_SIZE-2;  // rightOuter = 358
   * @endcode
   * Весь кадр доступен, кроме крайних столбцов (0 и IMAGE_SIZE-1).
   */
  class CEllipseBoundary
  {
  public:
    CEllipseBoundary();
    ~CEllipseBoundary();

    /**
     * @brief Инициализация: создаёт массив границ размером imageHeight строк.
     * @param imageWidth  Ширина изображения (IMAGE_SIZE в оригинале, обычно 360).
     * @param imageHeight Высота изображения (290 в оригинале).
     *
     * Заполняет границы по умолчанию: весь кадр доступен.
     * Аналог: mem_size() + цикл инициализации в WORK.C.
     */
    void Initialize(int imageWidth, int imageHeight);

    /**
     * @brief Сброс всех границ в ноль (вся область закрыта, IsInside всегда false).
     */
    void Clear();

    /** @brief Ширина изображения (IMAGE_SIZE). */
    int GetImageWidth() const { return m_imageWidth; }

    /** @brief Высота изображения. */
    int GetImageHeight() const { return m_imageHeight; }

    /**
     * @brief Установка эллиптической границы.
     * @param ellipse Параметры эллипса (центр + полуоси).
     * @param isOuter true — внешний (ell_bld), false — внутренний (ell_small).
     *
     * В оригинале MARKER.C:set_ell():
     * - Нажатие Space — ell_bld(a, b, x, y) — внешний
     * - Нажатие Enter — ell_small(a, b, x, y) — внутренний
     */
    void SetEllipse(const EllipseParams &ellipse, bool isOuter);

    /**
     * @brief Получение границ конкретной строки (const).
     * @param row Номер строки (0 .. imageHeight-1).
     * @return Ссылка на RowBoundary. Для невалидного row — пустая статическая структура.
     */
    const RowBoundary &GetRowBoundary(int row) const;

    /** @copydoc GetRowBoundary(int) const */
    RowBoundary &GetRowBoundary(int row);

    /**
     * @brief Проверка, находится ли точка в рабочей зоне.
     * @param x Координата x.
     * @param y Координата y (номер строки).
     * @return true если точка внутри внешнего эллипса и снаружи внутреннего.
     *
     * Порт функции inside() из STEP.C:648-657.
     * В оригинале: return 0 = OK, return -1 = вне области.
     */
    bool IsInside(int x, int y) const;

    /**
     * @brief Проверка только внешнего эллипса.
     * @return true если точка внутри внешнего эллипса.
     */
    bool IsInsideOuter(int x, int y) const;

    /**
     * @brief Проверка только внутреннего эллипса (мёртвой зоны).
     * @return true если точка внутри мёртвой зоны.
     */
    bool IsInsideInner(int x, int y) const;

    /** @brief Доступ ко всему массиву границ. */
    const std::vector<RowBoundary> &GetAllBoundaries() const
    {
      return m_boundaries;
    }

    /** @brief Сброс внешних границ: весь кадр доступен (leftOuter=1, rightOuter=width-2). */
    void ResetOuterBoundaries();

    /** @brief Сброс внутренних границ: нет мёртвой зоны (leftInner=0, rightInner=0). */
    void ResetInnerBoundaries();

    /** @brief Полный сброс (внешние + внутренние). */
    void ResetAllBoundaries();

    /**
     * @brief Установка границ по умолчанию — весь кадр доступен.
     *
     * Аналог инициализации в WORK.C:189-195.
     */
    void SetDefaultBoundaries();

    /** @brief Глубокое копирование границ из другого объекта. */
    void CopyFrom(const CEllipseBoundary &other);

    /**
     * @brief Проверка инвариантов (для отладки).
     * @return true если все строки корректны:
     *         leftOuter < rightOuter, leftInner < rightInner,
     *         внутренний эллипс внутри внешнего.
     */
    bool Validate() const;

  private:
    /**
     * @brief Вычисление x-координат пересечения эллипса со строкой.
     * @param[in]  ellipse Параметры эллипса.
     * @param[in]  row     Номер строки.
     * @param[out] x1      Левый край (cx - x).
     * @param[out] x2      Правый край (cx + x).
     *
     * Формула (MARKER.C:239):
     * @f[
     *   x = a \cdot \sqrt{1 - \frac{(row - cy)^2}{b^2}}
     * @f]
     *
     * @note Результат уже ограничен: x1 >= 1, x2 <= imageWidth-2.
     */
    void CalculateEllipsePoints(const EllipseParams &ellipse, int row, float &x1,
                                float &x2) const;

    /**
     * @brief Применение внешнего эллипса — порт ell_bld() из MARKER.C:212-253.
     *
     * Сужает текущие внешние границы до пересечения с новым эллипсом.
     * Строки вне эллипса обнуляются (leftOuter=0, rightOuter=0).
     */
    void ApplyOuterEllipse(const EllipseParams &ellipse);

    /**
     * @brief Применение внутреннего эллипса — порт ell_small() из MARKER.C:257-297.
     *
     * Расширяет мёртвую зону до объединения с новым эллипсом.
     * Строки вне обоих эллипсов (старого и нового) обнуляются.
     * Строки внутри старого но вне нового — сохраняются (объединение).
     */
    void ApplyInnerEllipse(const EllipseParams &ellipse);

    /** @brief Ограничение x в пределах [0, imageWidth-1]. */
    int ClampX(int x) const;

    /** @brief Ограничение y в пределах [0, imageHeight-1]. */
    int ClampY(int y) const;

    int m_imageWidth;  ///< Ширина изображения. В оригинале: IMAGE_SIZE (360).
    int m_imageHeight; ///< Высота изображения. В оригинале: 290 (жёстко задано).

    /**
     * @brief Массив границ — по одному RowBoundary на каждую строку.
     *
     * m_boundaries[y] эквивалентен arr_coord_ell[y][0..3] в оригинале.
     * Размер: m_imageHeight элементов.
     */
    std::vector<RowBoundary> m_boundaries;
  };

} // namespace Interferometry
