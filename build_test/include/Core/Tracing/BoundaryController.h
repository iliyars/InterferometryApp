/**
 * @file BoundaryController.h
 * @brief Интерактивное управление эллиптическими границами для MFC View.
 *
 * Портировано из SCAN360/MARKER.C, функция set_ell() (строки 62-210).
 *
 * @section controller_overview Общее описание
 *
 * В оригинале SCAN360 функция set_ell() — это интерактивный цикл,
 * который читает нажатия клавиш и двигает маркер эллипса по экрану.
 * Пользователь подгоняет эллипс к границе интерферограммы и нажимает:
 * - **Space** — применить как внешний эллипс (вызов ell_bld)
 * - **Enter** — применить как внутренний эллипс (вызов ell_small)
 * - **Esc** — выйти из режима редактирования
 *
 * В порте этот цикл разбит на событийную модель MFC:
 * каждое нажатие клавиши приходит через OnKeyDown().
 *
 * @section controller_keys Управление клавиатурой
 *
 * | Клавиша          | Действие в оригинале    | Метод в порте            |
 * |------------------|-------------------------|--------------------------|
 * | Numpad 1-9       | Перемещение центра      | UpdateMarkerPosition()   |
 * | Стрелки          | Перемещение центра      | UpdateMarkerPosition()   |
 * | W / F8           | Изменение полуоси A     | UpdateEllipseSize()      |
 * | U / V            | Изменение полуоси B     | UpdateEllipseSize()      |
 * | Space            | Применить внешний       | ApplyCurrentEllipse(true)|
 * | Enter            | Применить внутренний    | ApplyCurrentEllipse(false)|
 * | Esc              | Выход                   | Finish()                 |
 *
 * @section controller_state Состояние
 *
 * Контроллер хранит:
 * - Текущие параметры эллипса (m_currentEllipse)
 * - Позицию маркера (m_markerPos)
 * - Резервную копию границ для отмены (m_savedBoundary)
 * - Приращения для автоповтора (m_deltaX, m_deltaY, m_deltaA, m_deltaB)
 *
 * В оригинале это глобальные переменные xx, yy, aa, bb (MARKER.C:65-68).
 */

#pragma once

#include "EllipseBoundary.h"

#include <afxwin.h>

namespace Interferometry
{

  /**
   * @brief Режим редактирования эллипса.
   *
   * В оригинале режим неявно определяется тем, какие клавиши нажаты.
   * Здесь выделен явно для возможного расширения (например, мышь).
   */
  enum class BoundaryEditMode
  {
    Moving,       ///< Перемещение центра маркера (Numpad / стрелки)
    ResizingAxes, ///< Изменение полуосей эллипса (W/F8, U/V)
    ResizingStep  ///< Пошаговое изменение границ (Ctrl+Numpad)
  };

  /**
   * @brief Интерактивный контроллер редактирования эллиптических границ.
   *
   * Порт функции set_ell() из MARKER.C:62-210.
   *
   * @par Жизненный цикл
   *
   * @code
   *   CBoundaryController controller;
   *   controller.Initialize(&boundary);           // привязка к CEllipseBoundary
   *   // ... в обработчике OnKeyDown:
   *   if (controller.OnKeyDown(nChar, nRepCnt, nFlags)) {
   *     Invalidate();  // перерисовать
   *   }
   * @endcode
   *
   * @par Отмена изменений
   *
   * При вызове Initialize() / SetInitialPosition() сохраняется резервная
   * копия текущих границ. Cancel() восстанавливает их.
   */
  class CBoundaryController
  {
  public:
    CBoundaryController();
    ~CBoundaryController();

    /**
     * @brief Инициализация контроллера.
     * @param boundary Указатель на объект границ (не владеет, не удаляет).
     *
     * Устанавливает начальную позицию маркера в центр изображения.
     * Сохраняет резервную копию текущих границ.
     */
    void Initialize(CEllipseBoundary *boundary);

    /** @brief Сброс контроллера в неактивное состояние. */
    void Reset();

    /**
     * @brief Обработка нажатия клавиши.
     * @param nChar   Код клавиши (VK_*).
     * @param nRepCnt Счётчик повторов.
     * @param nFlags  Флаги (не используются).
     * @return true если нужна перерисовка экрана.
     *
     * Главная функция — аналог switch в set_ell() (MARKER.C:80-200).
     */
    bool OnKeyDown(uint32_t nChar, uint32_t nRepCnt, uint32_t nFlags);

    /**
     * @brief Обработка движения мыши.
     * @param point Текущая позиция курсора.
     * @return true если нужна перерисовка.
     *
     * @note В оригинале управление только с клавиатуры.
     *       Мышь добавлена для удобства, пока не реализована.
     */
    bool OnMouseMove(CPoint point);

    /**
     * @brief Обработка клика левой кнопкой мыши.
     * @param point Позиция клика.
     * @return true если нужна перерисовка.
     *
     * Устанавливает центр эллипса в точку клика.
     */
    bool OnLButtonDown(CPoint point);

    /**
     * @brief Применение текущего эллипса к границам.
     * @param isOuter true — внешний (Space), false — внутренний (Enter).
     *
     * Делегирует вызов CEllipseBoundary::SetEllipse().
     * В оригинале: вызов ell_bld() или ell_small().
     */
    void ApplyCurrentEllipse(bool isOuter);

    /**
     * @brief Отмена всех изменений — восстановление резервной копии.
     *
     * Восстанавливает границы из m_savedBoundary и сбрасывает контроллер.
     */
    void Cancel();

    /** @brief Завершение редактирования (Esc). Сбрасывает контроллер без отмены. */
    void Finish();

    /** @brief Текущие параметры редактируемого эллипса. */
    const EllipseParams &GetCurrentEllipse() const { return m_currentEllipse; }

    /** @brief Позиция маркера (центр эллипса) на экране. */
    CPoint GetMarkerPosition() const { return m_markerPos; }

    /** @brief Активен ли контроллер (идёт редактирование)? */
    bool IsActive() const { return m_isActive; }

    /** @brief Текущий режим редактирования. */
    BoundaryEditMode GetEditMode() const { return m_editMode; }

    /**
     * @brief Установка начальной позиции маркера.
     * @param center Координаты центра эллипса.
     *
     * Сбрасывает полуоси в начальный размер (50x50) и сохраняет
     * резервную копию текущих границ.
     */
    void SetInitialPosition(CPoint center);

    /**
     * @brief Текст статуса для отображения в строке состояния.
     * @return Строка вида "A: 50  B: 50  X:180  Y:145".
     *
     * Аналог вывода в mark_on() из MARKER.C.
     */
    CString GetStatusText() const;

  private:
    /**
     * @brief Обновление позиции маркера на (dx, dy).
     * @param dx Приращение по X (-1, 0, +1).
     * @param dy Приращение по Y (-1, 0, +1).
     *
     * В оригинале: xx, yy из set_ell() (MARKER.C:65-68).
     */
    void UpdateMarkerPosition(int dx, int dy);

    /**
     * @brief Обновление размера эллипса на (da, db).
     * @param da Приращение полуоси A.
     * @param db Приращение полуоси B.
     *
     * В оригинале: aa, bb из set_ell() (MARKER.C:65-68).
     */
    void UpdateEllipseSize(int da, int db);

    /** @brief Валидация координат маркера (не выходят за пределы изображения). */
    void ValidateMarkerPosition();

    /** @brief Валидация размеров эллипса (полуоси >= 1, не больше половины изображения). */
    void ValidateEllipseSize();

  private:
    CEllipseBoundary *m_boundary;   ///< Указатель на границы (не владеет). В оригинале: глобальный arr_coord_ell.
    EllipseParams m_currentEllipse; ///< Текущие параметры редактируемого эллипса.
    CPoint m_markerPos;             ///< Позиция маркера на экране (центр эллипса).

    bool m_isActive;             ///< Активен ли редактор (аналог нахождения внутри set_ell()).
    BoundaryEditMode m_editMode; ///< Текущий режим редактирования.

    /// @name Приращения для автоповтора
    /// В оригинале: переменные xx, yy, aa, bb (MARKER.C:65-68).
    /// @{
    int m_deltaX; ///< Приращение позиции по X.
    int m_deltaY; ///< Приращение позиции по Y.
    int m_deltaA; ///< Приращение полуоси A.
    int m_deltaB; ///< Приращение полуоси B.
    /// @}

    CEllipseBoundary m_savedBoundary; ///< Резервная копия границ для отмены.
    bool m_hasBackup;                 ///< Есть ли сохранённая копия.
  };

} // namespace Interferometry
