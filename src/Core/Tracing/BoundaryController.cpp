/**
 * @file BoundaryController.cpp
 * @brief Реализация интерактивного контроллера границ.
 *
 * Портировано из SCAN360/MARKER.C, функция set_ell() (строки 62-210).
 *
 * Оригинал set_ell() — один большой цикл с getch() внутри.
 * В порте каждое нажатие клавиши обрабатывается отдельно через OnKeyDown().
 */

#include "pch.h"

#include "BoundaryController.h"

namespace Interferometry
{

  CBoundaryController::CBoundaryController()
      : m_boundary(nullptr),
        m_isActive(false),
        m_editMode(BoundaryEditMode::Moving),
        m_deltaX(0),
        m_deltaY(0),
        m_deltaA(0),
        m_deltaB(0),
        m_hasBackup(false) {}

  CBoundaryController::~CBoundaryController() {}

  void CBoundaryController::Initialize(CEllipseBoundary *boundary)
  {
    m_boundary = boundary;
    if (m_boundary)
    {
      // Начальная позиция — центр изображения
      int centerX = m_boundary->GetImageWidth() / 2;
      int centerY = m_boundary->GetImageHeight() / 2;
      SetInitialPosition(CPoint(centerX, centerY));
    }
  }

  void CBoundaryController::Reset()
  {
    m_isActive = false;
    m_deltaX = m_deltaY = 0;
    m_deltaA = m_deltaB = 0;
    m_editMode = BoundaryEditMode::Moving;
  }

  void CBoundaryController::SetInitialPosition(CPoint center)
  {
    m_markerPos = center;
    m_currentEllipse.centerX = center.x;
    m_currentEllipse.centerY = center.y;
    m_currentEllipse.semiAxisA = 50; // Начальный размер как в оригинале
    m_currentEllipse.semiAxisB = 50;
    m_isActive = true;
    m_deltaX = m_deltaY = 0;
    m_deltaA = m_deltaB = 0;

    // Сохранить резервную копию для отмены
    if (m_boundary)
    {
      m_savedBoundary.CopyFrom(*m_boundary);
      m_hasBackup = true;
    }
  }

  //=============================================================================
  // Обработка клавиатуры
  //=============================================================================

  /**
   * @details
   * Порт switch-блока из set_ell() (MARKER.C:80-200).
   *
   * Оригинал использовал коды клавиш DOS (getch()):
   * - 72/80/75/77 — стрелки (вверх/вниз/влево/вправо)
   * - 71/73/79/81 — Numpad 7/9/1/3 (диагонали)
   * - 's','t','w','u','v' — изменение полуосей
   * - ' ' (32) — Space, применить внешний
   * - 13 — Enter, применить внутренний
   * - 27 — Esc, выход
   *
   * В порте используются VK_* коды Windows.
   */
  bool CBoundaryController::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
  {
    if (!m_isActive)
      return false;

    bool needRedraw = false;

    // Сброс приращений
    m_deltaX = m_deltaY = 0;
    m_deltaA = m_deltaB = 0;

    switch (nChar)
    {
    // ===== Применение эллипса =====
    case VK_SPACE: // Space — внешняя граница (ell_bld)
      ApplyCurrentEllipse(true);
      m_currentEllipse.semiAxisA = 50;
      m_currentEllipse.semiAxisB = 50;
      needRedraw = true;
      break;

    case VK_RETURN: // Enter — внутренняя граница (ell_small)
      ApplyCurrentEllipse(false);
      m_currentEllipse.semiAxisA = 20;
      m_currentEllipse.semiAxisB = 20;
      needRedraw = true;
      break;

    case VK_ESCAPE: // Esc — выход
      Finish();
      return true;

    // ===== Перемещение центра маркера (цифровая клавиатура) =====
    case VK_NUMPAD7:
      m_deltaX = -1;
      m_deltaY = -1;
      break;
    case VK_NUMPAD8:
      m_deltaY = -1;
      break;
    case VK_NUMPAD9:
      m_deltaX = 1;
      m_deltaY = -1;
      break;
    case VK_NUMPAD4:
      m_deltaX = -1;
      break;
    case VK_NUMPAD6:
      m_deltaX = 1;
      break;
    case VK_NUMPAD1:
      m_deltaX = -1;
      m_deltaY = 1;
      break;
    case VK_NUMPAD2:
      m_deltaY = 1;
      break;
    case VK_NUMPAD3:
      m_deltaX = 1;
      m_deltaY = 1;
      break;

    // ===== Перемещение центра маркера (стрелки) =====
    case VK_UP:
      m_deltaY = -1;
      break;
    case VK_DOWN:
      m_deltaY = 1;
      break;
    case VK_LEFT:
      m_deltaX = -1;
      break;
    case VK_RIGHT:
      m_deltaX = 1;
      break;

    // ===== Изменение осей эллипса =====
    case 'S': // s — уменьшить A
      m_deltaA = -1;
      break;
    case 'T': // t — увеличить A
      m_deltaA = 1;
      break;

    case 'W': // w — уменьшить A (альтернатива)
      m_currentEllipse.semiAxisA =
          (std::max)(1, m_currentEllipse.semiAxisA - 1);
      needRedraw = true;
      break;
    case VK_F8: // F8 — увеличить A
      m_currentEllipse.semiAxisA++;
      needRedraw = true;
      break;

    case 'U': // u — уменьшить B
      m_currentEllipse.semiAxisB =
          (std::max)(1, m_currentEllipse.semiAxisB - 1);
      needRedraw = true;
      break;
    case 'V': // v — увеличить B
      m_currentEllipse.semiAxisB++;
      needRedraw = true;
      break;

    // ===== Пошаговое изменение (Ctrl+Numpad) =====
    case VK_NUMPAD7 | 0x100:
      m_deltaA = 0;
      m_deltaB = -1;
      break;
    case VK_NUMPAD9 | 0x100:
      m_deltaA = 0;
      m_deltaB = 1;
      break;
    case VK_NUMPAD1 | 0x100:
      m_deltaA = -1;
      m_deltaB = 0;
      break;
    case VK_NUMPAD3 | 0x100:
      m_deltaA = 1;
      m_deltaB = 0;
      break;

    default:
      return false;
    }

    // Применить приращения
    if (m_deltaX != 0 || m_deltaY != 0)
    {
      UpdateMarkerPosition(m_deltaX, m_deltaY);
      needRedraw = true;
    }

    if (m_deltaA != 0 || m_deltaB != 0)
    {
      UpdateEllipseSize(m_deltaA, m_deltaB);
      needRedraw = true;
    }

    return needRedraw;
  }

  //=============================================================================
  // Обновление позиции и размеров
  //=============================================================================

  void CBoundaryController::UpdateMarkerPosition(int dx, int dy)
  {
    m_markerPos.x += dx;
    m_markerPos.y += dy;
    ValidateMarkerPosition();

    m_currentEllipse.centerX = m_markerPos.x;
    m_currentEllipse.centerY = m_markerPos.y;
  }

  void CBoundaryController::UpdateEllipseSize(int da, int db)
  {
    m_currentEllipse.semiAxisA += da;
    m_currentEllipse.semiAxisB += db;
    ValidateEllipseSize();
  }

  void CBoundaryController::ValidateMarkerPosition()
  {
    if (!m_boundary)
      return;

    int maxX = m_boundary->GetImageWidth() - 1;
    int maxY = m_boundary->GetImageHeight() - 1;

    if (m_markerPos.x <= 0)
      m_markerPos.x = 1;
    if (m_markerPos.x >= maxX)
      m_markerPos.x = maxX - 1;
    if (m_markerPos.y <= 0)
      m_markerPos.y = 1;
    if (m_markerPos.y >= maxY)
      m_markerPos.y = maxY - 1;
  }

  void CBoundaryController::ValidateEllipseSize()
  {
    if (m_currentEllipse.semiAxisA < 1)
      m_currentEllipse.semiAxisA = 1;
    if (m_currentEllipse.semiAxisB < 1)
      m_currentEllipse.semiAxisB = 1;

    if (m_boundary)
    {
      int maxA = m_boundary->GetImageWidth() / 2;
      int maxB = m_boundary->GetImageHeight() / 2;
      if (m_currentEllipse.semiAxisA > maxA)
        m_currentEllipse.semiAxisA = maxA;
      if (m_currentEllipse.semiAxisB > maxB)
        m_currentEllipse.semiAxisB = maxB;
    }
  }

  //=============================================================================
  // Применение, отмена, завершение
  //=============================================================================

  void CBoundaryController::ApplyCurrentEllipse(bool isOuter)
  {
    if (!m_boundary || !m_currentEllipse.IsValid())
      return;

    m_boundary->SetEllipse(m_currentEllipse, isOuter);
  }

  void CBoundaryController::Cancel()
  {
    if (m_hasBackup && m_boundary)
    {
      m_boundary->CopyFrom(m_savedBoundary);
    }
    Reset();
  }

  void CBoundaryController::Finish()
  {
    Reset();
  }

  //=============================================================================
  // Обработка мыши
  //=============================================================================

  bool CBoundaryController::OnMouseMove(CPoint point)
  {
    if (!m_isActive)
      return false;

    // В оригинале только клавиатура, мышь пока не реализована
    return false;
  }

  bool CBoundaryController::OnLButtonDown(CPoint point)
  {
    if (!m_isActive)
      return false;

    m_markerPos = point;
    m_currentEllipse.centerX = point.x;
    m_currentEllipse.centerY = point.y;
    ValidateMarkerPosition();

    return true;
  }

  //=============================================================================
  // Статус
  //=============================================================================

  /**
   * @details
   * Формат аналогичен выводу mark_on() в оригинале (MARKER.C:29-40),
   * где на экране отображались текущие параметры эллипса.
   */
  CString CBoundaryController::GetStatusText() const
  {
    CString status;
    status.Format(_T("A:%3d  B:%3d  X:%3d  Y:%3d"),
                  m_currentEllipse.semiAxisA,
                  m_currentEllipse.semiAxisB,
                  m_currentEllipse.centerX,
                  m_currentEllipse.centerY);
    return status;
  }

} // namespace Interferometry
