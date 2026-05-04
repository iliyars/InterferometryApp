/**
 * @file BoundaryRenderer.h
 * @brief Отрисовка эллиптических границ в MFC View.
 *
 * Портировано из SCAN360/MARKER.C:
 * - mark_on()  — отрисовка маркера (строки 24-55)
 * - mark_off() — стирание маркера
 * - отрисовка границ в ell_bld() и ell_small()
 *
 * @section renderer_overview Общее описание
 *
 * В оригинале SCAN360 границы рисовались прямо в видеопамять через
 * pix(x, y, color). Маркер — 4 вертикальных/горизонтальных штриха
 * на концах полуосей.
 *
 * В порте отрисовка выделена в отдельный класс, который использует
 * MFC CDC (Device Context) и может рисовать:
 * - Установленные границы (точки на пересечении строк с эллипсами)
 * - Текущий редактируемый эллипс (контур)
 * - Маркер центра (4 штриха на концах полуосей)
 *
 * @section renderer_colors Цветовая схема
 *
 * | Элемент            | Цвет по умолчанию | Оригинал            |
 * |--------------------|-------------------|---------------------|
 * | Внешняя граница    | Белый             | Цвет 15 (белый EGA) |
 * | Внутренняя граница | Cyan              | Цвет 15 (белый)     |
 * | Маркер             | Красный           | Цвет 15 (белый)     |
 * | Текущий эллипс     | Жёлтый            | —                   |
 */

#pragma once

#include "BoundaryController.h"
#include "EllipseBoundary.h"

namespace Interferometry
{

  /**
   * @brief Параметры отрисовки границ.
   *
   * Настраиваемые цвета и флаги видимости элементов.
   * В оригинале всё рисовалось одним цветом (15 = белый в EGA палитре).
   */
  struct BoundaryRenderParams
  {
    COLORREF outerBoundaryColor;  ///< Цвет внешней границы (по умолчанию белый).
    COLORREF innerBoundaryColor;  ///< Цвет внутренней границы (по умолчанию cyan).
    COLORREF markerColor;         ///< Цвет маркера (по умолчанию красный).
    COLORREF currentEllipseColor; ///< Цвет редактируемого эллипса (по умолчанию жёлтый).
    int lineWidth;                ///< Толщина линий (по умолчанию 1 пиксель).
    bool showMarker;              ///< Показывать маркер центра.
    bool showCurrentEllipse;      ///< Показывать контур текущего эллипса.

    /** @brief Конструктор с цветами по умолчанию. */
    BoundaryRenderParams()
        : outerBoundaryColor(RGB(255, 255, 255)),
          innerBoundaryColor(RGB(0, 255, 255)),
          markerColor(RGB(255, 0, 0)),
          currentEllipseColor(RGB(255, 255, 0)),
          lineWidth(1),
          showMarker(true),
          showCurrentEllipse(true) {}
  };

  /**
   * @brief Класс отрисовки эллиптических границ и маркера.
   *
   * Порт визуализации из MARKER.C (функции mark_on, mark_off, pix).
   *
   * @par Использование
   *
   * @code
   *   CBoundaryRenderer renderer;
   *   // В обработчике OnDraw(CDC* pDC):
   *   renderer.DrawAll(pDC, &boundary, &controller);
   * @endcode
   *
   * @par Архитектурное отличие от оригинала
   *
   * В оригинале отрисовка встроена прямо в логику ell_bld() / ell_small()
   * (вызовы pix() внутри циклов вычисления границ). В порте отрисовка
   * полностью отделена от логики — CEllipseBoundary ничего не знает
   * о графике, а CBoundaryRenderer ничего не знает о вычислениях.
   */
  class CBoundaryRenderer
  {
  public:
    CBoundaryRenderer();
    ~CBoundaryRenderer();

    /** @brief Установка параметров отрисовки. */
    void SetParams(const BoundaryRenderParams &params) { m_params = params; }

    /** @brief Получение текущих параметров отрисовки. */
    const BoundaryRenderParams &GetParams() const { return m_params; }

    /**
     * @brief Отрисовка установленных границ (внешний + внутренний эллипсы).
     * @param pDC      Контекст устройства MFC.
     * @param boundary Объект границ.
     *
     * Рисует по одной точке на каждую строку для каждой из 4 границ
     * (leftOuter, rightOuter, leftInner, rightInner).
     *
     * В оригинале: вызовы pix() в ell_bld() и ell_small().
     */
    void DrawBoundaries(CDC *pDC, const CEllipseBoundary *boundary);

    /**
     * @brief Отрисовка контура текущего редактируемого эллипса.
     * @param pDC     Контекст устройства.
     * @param ellipse Параметры эллипса для отрисовки.
     */
    void DrawCurrentEllipse(CDC *pDC, const EllipseParams &ellipse);

    /**
     * @brief Отрисовка маркера центра эллипса.
     * @param center    Позиция центра.
     * @param semiAxisA Полуось A (горизонтальная).
     * @param semiAxisB Полуось B (вертикальная).
     *
     * Порт mark_on() из MARKER.C:24-55.
     * Рисует 4 штриха по 5 пикселей на концах полуосей.
     */
    void DrawMarker(CDC *pDC, CPoint center, int semiAxisA, int semiAxisB);

    /**
     * @brief Отрисовка всего: границы + текущий эллипс + маркер.
     * @param pDC        Контекст устройства.
     * @param boundary   Объект границ (может быть nullptr).
     * @param controller Контроллер редактирования (может быть nullptr).
     *
     * Удобный метод для вызова из OnDraw().
     */
    void DrawAll(CDC *pDC, const CEllipseBoundary *boundary,
                 const CBoundaryController *controller);

  private:
    /**
     * @brief Отрисовка эллипса по уравнению.
     * @param pDC     Контекст устройства.
     * @param ellipse Параметры эллипса.
     * @param color   Цвет линии.
     *
     * Использует ту же формулу, что CalculateEllipsePoints():
     * @f[
     *   x = a \cdot \sqrt{1 - \frac{(y - cy)^2}{b^2}}
     * @f]
     * Рисует по 2 точки на строку (левая и правая часть эллипса).
     */
    void DrawEllipse(CDC *pDC, const EllipseParams &ellipse, COLORREF color);

    /**
     * @brief Отрисовка креста маркера (4 штриха на концах полуосей).
     *
     * Порт mark_on() из MARKER.C:24-55.
     * Каждый штрих — 5 пикселей перпендикулярно оси.
     */
    void DrawMarkerCross(CDC *pDC, CPoint center, int semiAxisA, int semiAxisB);

  private:
    BoundaryRenderParams m_params; ///< Текущие параметры отрисовки.
  };

} // namespace Interferometry
