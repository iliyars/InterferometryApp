/**
 * @file EllipseBoundary.cpp
 * @brief Реализация CEllipseBoundary — управление эллиптическими границами.
 *
 * Портировано из SCAN360:
 * - MARKER.C: ell_bld() (строки 212-253), ell_small() (строки 257-297)
 * - STEP.C:   inside() (строки 648-657)
 * - WORK.C:   инициализация arr_coord_ell (строки 189-195)
 */

#include "pch.h"

#include <EllipseBoundary.h>

#include <algorithm>
#include <cmath>

namespace Interferometry
{

  /// @name Конструктор / Деструктор
  /// @{

  /**
   * @brief Конструктор по умолчанию.
   *
   * Создаёт массив 290 строк (как в оригинале), все границы = 0.
   *
   * @warning После конструктора IsInside() всегда возвращает false.
   *          Нужно вызвать Initialize() или SetDefaultBoundaries().
   */
  CEllipseBoundary::CEllipseBoundary() : m_imageWidth(360), m_imageHeight(290)
  {
    m_boundaries.resize(m_imageHeight);
  }

  CEllipseBoundary::~CEllipseBoundary() {}

  /// @}

  /// @name Инициализация и сброс
  /// @{

  void CEllipseBoundary::Initialize(int imageWidth, int imageHeight)
  {
    m_imageWidth = imageWidth;
    m_imageHeight = imageHeight;
    m_boundaries.clear();
    m_boundaries.resize(m_imageHeight);

    // По умолчанию весь кадр доступен
    SetDefaultBoundaries();
  }

  void CEllipseBoundary::Clear()
  {
    for (auto &boundary : m_boundaries)
    {
      boundary = RowBoundary();
    }
  }

  /// @}

  //=============================================================================
  // Установка эллипса
  //=============================================================================

  void CEllipseBoundary::SetEllipse(const EllipseParams &ellipse, bool isOuter)
  {
    if (!ellipse.IsValid())
      return;

    if (isOuter)
      ApplyOuterEllipse(ellipse);
    else
      ApplyInnerEllipse(ellipse);
  }

  /**
   * @brief Применение внешнего эллипса.
   *
   * Порт ell_bld() из MARKER.C:212-253.
   *
   * @par Алгоритм оригинала
   *
   * 1. **Первый вызов** (все [0]==0): инициализировать [3] = IMAGE_SIZE-1.
   *    Это нужно, чтобы условие "[3] > x1" на шаге 4 сработало.
   *    (MARKER.C:219-222)
   *
   * 2. **Стирание** старых границ на экране — пропущено в порте,
   *    визуализация в отдельном слое. (MARKER.C:226-230)
   *
   * 3. **Обнуление** строк выше верхнего края эллипса (y <= cy-b).
   *    (MARKER.C:231-234)
   *
   * 4. **Вычисление** границ для строк внутри эллипса (cy-b+1 .. cy+b).
   *    Для каждой строки вычисляется x по формуле эллипса, затем:
   *    @code
   *      if([0] < x1 && [3] > x1) [0] = x1;  // сдвинуть leftOuter вправо
   *      if([3] > x2 && x2 > [0]) [3] = x2;  // сдвинуть rightOuter влево
   *    @endcode
   *    Т.е. внешний эллипс **сужает** существующую область.
   *    (MARKER.C:236-248)
   *
   * 5. **Обнуление** строк ниже нижнего края эллипса (y > cy+b).
   *    (MARKER.C:249-252)
   *
   * @param ellipse Параметры эллипса (центр + полуоси).
   */
  void CEllipseBoundary::ApplyOuterEllipse(const EllipseParams &ellipse)
  {
    int x_center = ellipse.centerX;
    int y_center = ellipse.centerY;
    int a = ellipse.semiAxisA;
    int b = ellipse.semiAxisB;

    // --- Шаг 1: Проверка первого вызова ---
    // Оригинал (MARKER.C:219-222):
    //   for(i=0; arr_coord_ell[i][0]==0 && i<290; i++);
    //   if(i==290)
    //     for(i=0; i<290; i++) arr_coord_ell[i][3] = IMAGE_SIZE-1;
    bool foundExisting = false;
    for (int i = 0; i < m_imageHeight; i++)
    {
      if (m_boundaries[i].HasOuterBoundary())
      {
        foundExisting = true;
        break;
      }
    }

    if (!foundExisting)
    {
      for (int i = 0; i < m_imageHeight; i++)
      {
        m_boundaries[i].rightOuter = m_imageWidth - 1;
      }
    }

    // --- Шаг 3: Обнуление строк выше эллипса ---
    // Оригинал (MARKER.C:231-234):
    //   for(i=0; i <= y_centr-b; i++) { [3]=0; [0]=0; }
    int topEdge = (std::max)(0, y_center - b);
    for (int i = 0; i <= topEdge && i < m_imageHeight; i++)
    {
      m_boundaries[i].leftOuter = 0;
      m_boundaries[i].rightOuter = 0;
    }

    // --- Шаг 4: Вычисление границ для строк внутри эллипса ---
    // Оригинал (MARKER.C:235-248):
    //   if((edge = y_centr+b) > 290) edge = 290;
    //   for(i=i; i < edge; i++) { ... }
    int bottomEdge = y_center + b;
    if (bottomEdge >= m_imageHeight)
      bottomEdge = m_imageHeight - 1;

    for (int i = topEdge + 1; i <= bottomEdge; i++)
    {
      float x1, x2;
      CalculateEllipsePoints(ellipse, i, x1, x2);

      int ix1 = (int)x1;
      int ix2 = (int)x2;

      // Оригинал (MARKER.C:242):
      //   if(arr_coord_ell[i][0] < x1 && arr_coord_ell[i][3] > x1)
      //     arr_coord_ell[i][0] = x1;
      if (m_boundaries[i].leftOuter < ix1 && m_boundaries[i].rightOuter > ix1)
      {
        m_boundaries[i].leftOuter = ix1;
      }

      // Оригинал (MARKER.C:243):
      //   if(arr_coord_ell[i][3] > x2 && x2 > arr_coord_ell[i][0])
      //     arr_coord_ell[i][3] = x2;
      if (m_boundaries[i].rightOuter > ix2 && ix2 > m_boundaries[i].leftOuter)
      {
        m_boundaries[i].rightOuter = ix2;
      }
    }

    // --- Шаг 5: Обнуление строк ниже эллипса ---
    // Оригинал (MARKER.C:249-252):
    //   for(i=i; i<290; i++) { [3]=0; [0]=0; }
    for (int i = bottomEdge + 1; i < m_imageHeight; i++)
    {
      m_boundaries[i].leftOuter = 0;
      m_boundaries[i].rightOuter = 0;
    }
  }

  /**
   * @brief Применение внутреннего эллипса (мёртвая зона).
   *
   * Порт ell_small() из MARKER.C:257-297.
   *
   * @par Что делает внутренний эллипс
   *
   * Внутренний эллипс задаёт "мёртвую зону" — область внутри интерферограммы,
   * где трассировка запрещена (например, отверстие в зеркале Кассегрена).
   * Точки ВНУТРИ дырки исключаются из рабочей области.
   *
   * @par Алгоритм оригинала (построчный разбор)
   *
   * Оригинал ell_small() **объединяет** старый и новый внутренние эллипсы:
   *
   * 1. (стр. 262-266) Стирание пикселей СТАРОГО внутреннего эллипса (визуализация).
   *
   * 2. (стр. 268-271) Прокрутка по строкам где [1]==0 и y <= cy-b.
   *    Эти строки и так нулевые — по сути NOP, просто двигает счётчик i.
   *
   * 3. (стр. 272-276) Если СТАРЫЙ эллипс начинался ВЫШЕ нового —
   *    его строки **сохраняются** (рисуются на экране, данные не трогаются).
   *
   * 4. (стр. 278-289) Основной цикл: для строк нового эллипса
   *    **расширяется** мёртвая зона:
   *    @code
   *      // leftInner: берём МЕНЬШЕЕ (ближе к левому краю)
   *      if(([1] > x1 || [1]==0) && x1 >= [0] && x1 < [3])
   *        [1] = x1;
   *
   *      // rightInner: берём БОЛЬШЕЕ (ближе к правому краю)
   *      if([2] < x2 && x2 <= [3])
   *        [2] = x2;
   *    @endcode
   *
   * 5. (стр. 290-294) Если СТАРЫЙ эллипс продолжается НИЖЕ нового —
   *    его строки **сохраняются**.
   *
   * 6. (стр. 295-296) Строки после конца ОБОИХ эллипсов — обнуляются.
   *
   * @par Итого
   *
   * Обнуление leftInner/rightInner происходит только для строк ВНЕ
   * обоих эллипсов (и старого, и нового). Это даёт ОБЪЕДИНЕНИЕ.
   *
   * @param ellipse Параметры эллипса (центр + полуоси).
   */
  void CEllipseBoundary::ApplyInnerEllipse(const EllipseParams &ellipse)
  {
    int x_centr = ellipse.centerX;
    int y_centr = ellipse.centerY;
    int a = ellipse.semiAxisA;
    int b = ellipse.semiAxisB;

    int newTop = (std::max)(0, y_centr - b);
    int newBottom = (std::min)(y_centr + b, m_imageHeight - 1);

    // --- Найти диапазон СТАРОГО внутреннего эллипса ---
    // Оригинал (MARKER.C:262):
    //   for(i=0; arr_coord_ell[i][1]==0 && i<290; i++);
    int oldTop = m_imageHeight; // "нет старого" по умолчанию
    int oldBottom = -1;
    for (int i = 0; i < m_imageHeight; i++)
    {
      if (m_boundaries[i].HasInnerBoundary())
      {
        if (oldTop == m_imageHeight)
          oldTop = i;
        oldBottom = i;
      }
    }

    // --- Вычислить объединённый диапазон ---
    // Строки, покрытые хотя бы одним из двух эллипсов — сохраняются.
    // Строки вне обоих — обнуляются.
    int unionTop, unionBottom;
    if (oldTop == m_imageHeight)
    {
      // Старого эллипса нет — работаем только с новым
      unionTop = newTop;
      unionBottom = newBottom;
    }
    else
    {
      unionTop = (std::min)(oldTop, newTop);
      unionBottom = (std::max)(oldBottom, newBottom);
    }

    // --- Обнулить строки ВЫШЕ объединённого диапазона ---
    for (int i = 0; i < unionTop && i < m_imageHeight; i++)
    {
      m_boundaries[i].leftInner = 0;
      m_boundaries[i].rightInner = 0;
    }

    // --- Основной цикл: вычислить границы для строк НОВОГО эллипса ---
    // Строки между старым и новым (вне нового но внутри старого) —
    // старые значения сохранены, мы их не трогаем.
    for (int i = newTop; i <= newBottom; i++)
    {
      float x1, x2;
      CalculateEllipsePoints(ellipse, i, x1, x2);

      int ix1 = (int)x1;
      int ix2 = (int)x2;

      const RowBoundary &row = m_boundaries[i];

      // Оригинал (MARKER.C:284-285):
      //   if(([1] > x1 && x1 >= [0] && x1 < [3]) ||
      //      ([1] == 0  && x1 >= [0] && x1 < [3]))
      //     [1] = x1;
      //
      // Смысл: leftInner = x1 если:
      //   - Новый x1 левее текущего [1] (расширяем дырку влево), ИЛИ
      //   - [1]==0 (ещё нет внутреннего эллипса на этой строке)
      //   И x1 лежит внутри внешнего эллипса ([0] <= x1 < [3])
      if (ix1 >= row.leftOuter && ix1 < row.rightOuter)
      {
        if (row.leftInner == 0 || ix1 < row.leftInner)
        {
          m_boundaries[i].leftInner = ix1;
        }
      }

      // Оригинал (MARKER.C:286):
      //   if([2] < x2 && x2 <= [3])
      //     [2] = x2;
      //
      // Смысл: rightInner = x2 если новый x2 правее текущего [2]
      //   (расширяем дырку вправо) И x2 внутри внешнего
      if (ix2 <= row.rightOuter)
      {
        if (row.rightInner == 0 || ix2 > row.rightInner)
        {
          m_boundaries[i].rightInner = ix2;
        }
      }
    }

    // --- Обнулить строки НИЖЕ объединённого диапазона ---
    // Оригинал (MARKER.C:295-296):
    //   for(i=i; i<290; i++) { [2]=0; [1]=0; }
    for (int i = unionBottom + 1; i < m_imageHeight; i++)
    {
      m_boundaries[i].leftInner = 0;
      m_boundaries[i].rightInner = 0;
    }
  }

  /**
   * @brief Вычисление x-координат пересечения эллипса со строкой.
   *
   * Формула из MARKER.C:239:
   * @code
   *   x = (int)(0.5 + (float)a * sqrt(1.0 - buf*buf / ((float)b*(float)b)));
   *   x1 = x_centr - x;
   *   x2 = x_centr + x;
   * @endcode
   *
   * @param[in]  ellipse Параметры эллипса.
   * @param[in]  row     Номер строки (y-координата).
   * @param[out] x1      Левый край эллипса на этой строке.
   * @param[out] x2      Правый край эллипса на этой строке.
   *
   * @note Если строка вне эллипса (|row - cy| >= b), x1 == x2 == cx.
   * @note Результат ограничен: x1 >= 1.0, x2 <= imageWidth - 2.
   */
  void CEllipseBoundary::CalculateEllipsePoints(const EllipseParams &ellipse,
                                                int row, float &x1,
                                                float &x2) const
  {
    int x_centr = ellipse.centerX;
    int y_centr = ellipse.centerY;
    int a = ellipse.semiAxisA;
    int b = ellipse.semiAxisB;

    float buf = (float)(row - y_centr);
    float bSquared = (float)b * (float)b;

    float ratio = buf * buf / bSquared;
    if (ratio >= 1.0f)
    {
      // Строка за пределами эллипса
      x1 = (float)x_centr;
      x2 = (float)x_centr;
      return;
    }

    // +0.5f — округление к ближайшему, как в оригинале: (int)(0.5 + ...)
    float x = 0.5f + (float)a * std::sqrt(1.0f - ratio);

    x1 = (float)x_centr - x;
    x2 = (float)x_centr + x;

    // Ограничение координат (MARKER.C:240-241)
    if (x1 < 1.0f)
      x1 = 1.0f;
    if (x2 >= (float)(m_imageWidth - 1))
      x2 = (float)(m_imageWidth - 2);
  }

  /// @name Доступ к границам
  /// @{

  const RowBoundary &CEllipseBoundary::GetRowBoundary(int row) const
  {
    static RowBoundary emptyBoundary;
    if (row < 0 || row >= m_imageHeight)
      return emptyBoundary;
    return m_boundaries[row];
  }

  RowBoundary &CEllipseBoundary::GetRowBoundary(int row)
  {
    static RowBoundary emptyBoundary;
    if (row < 0 || row >= m_imageHeight)
      return emptyBoundary;
    return m_boundaries[row];
  }

  /// @}

  /// @name Проверка принадлежности точки
  /// @{

  /**
   * @details
   * Порт inside() из STEP.C:648-657:
   * @code
   *   if(xt<0 || xt>=IMAGE_SIZE || yt<0 || yt>=290) return(-1);
   *   if(xt < [yt][0] || xt > [yt][3])              return(-1);  // вне внешнего
   *   if(xt > [yt][1] && xt < [yt][2])              return(-1);  // внутри дырки
   *   return(0);                                                  // OK
   * @endcode
   */
  bool CEllipseBoundary::IsInside(int x, int y) const
  {
    if (y < 0 || y >= m_imageHeight)
      return false;
    if (x < 0 || x >= m_imageWidth)
      return false;
    return m_boundaries[y].IsInside(x);
  }

  bool CEllipseBoundary::IsInsideOuter(int x, int y) const
  {
    if (y < 0 || y >= m_imageHeight)
      return false;
    return m_boundaries[y].IsInsideOuter(x);
  }

  bool CEllipseBoundary::IsInsideInner(int x, int y) const
  {
    if (y < 0 || y >= m_imageHeight)
      return false;
    return m_boundaries[y].IsInsideInner(x);
  }

  /// @}

  /// @name Сброс границ
  /// @{

  void CEllipseBoundary::ResetOuterBoundaries()
  {
    for (auto &b : m_boundaries)
    {
      b.leftOuter = 1;
      b.rightOuter = m_imageWidth - 2;
    }
  }

  void CEllipseBoundary::ResetInnerBoundaries()
  {
    for (auto &b : m_boundaries)
    {
      b.leftInner = 0;
      b.rightInner = 0;
    }
  }

  void CEllipseBoundary::ResetAllBoundaries()
  {
    ResetOuterBoundaries();
    ResetInnerBoundaries();
  }

  /**
   * @details Воспроизводит инициализацию из WORK.C:189-195:
   * @code
   *   arr_coord_ell[i][0] = 1;
   *   arr_coord_ell[i][1] = 0;
   *   arr_coord_ell[i][2] = 0;
   *   arr_coord_ell[i][3] = IMAGE_SIZE - 2;
   * @endcode
   */
  void CEllipseBoundary::SetDefaultBoundaries()
  {
    for (int i = 0; i < m_imageHeight; i++)
    {
      m_boundaries[i].leftOuter = 1;
      m_boundaries[i].rightOuter = m_imageWidth - 2;
      m_boundaries[i].leftInner = 0;
      m_boundaries[i].rightInner = 0;
    }
  }

  /// @}

  void CEllipseBoundary::CopyFrom(const CEllipseBoundary &other)
  {
    m_imageWidth = other.m_imageWidth;
    m_imageHeight = other.m_imageHeight;
    m_boundaries = other.m_boundaries;
  }

  bool CEllipseBoundary::Validate() const
  {
    for (int i = 0; i < m_imageHeight; i++)
    {
      const RowBoundary &b = m_boundaries[i];

      if (b.HasOuterBoundary())
      {
        if (b.leftOuter >= b.rightOuter)
          return false;
        if (b.leftOuter < 0 || b.rightOuter >= m_imageWidth)
          return false;
      }

      if (b.HasInnerBoundary())
      {
        if (b.leftInner >= b.rightInner)
          return false;
        if (b.HasOuterBoundary())
        {
          if (b.leftInner < b.leftOuter || b.rightInner > b.rightOuter)
            return false;
        }
      }
    }
    return true;
  }

  /// @name Вспомогательные функции
  /// @{

  int CEllipseBoundary::ClampX(int x) const
  {
    if (x < 0)
      return 0;
    if (x >= m_imageWidth)
      return m_imageWidth - 1;
    return x;
  }

  int CEllipseBoundary::ClampY(int y) const
  {
    if (y < 0)
      return 0;
    if (y >= m_imageHeight)
      return m_imageHeight - 1;
    return y;
  }

  /// @}

} // namespace Interferometry
