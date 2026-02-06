// BoundaryController.cpp
// Реализация интерактивного контроллера границ
// Портировано из SCAN360/MARKER.C функция set_ell()

#include "BoundaryController.h"

namespace Interferometry {
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

void CBoundaryController::Initialize(CEllipseBoundary* boundary) {
  m_boundary = boundary;
  if (m_boundary) {
    // Начальная позиция - центр изображения
    int centerX = m_boundary->GetImageWidth() / 2;
    int centerY = m_boundary->GetImageHeight() / 2;
    SetInitialPosition(CPoint(centerX, centerY));
  }
}

void CBoundaryController::Reset() {
  m_isActive = false;
  m_deltaX = m_deltaY = 0;
  m_deltaA = m_deltaB = 0;
  m_editMode = BoundaryEditMode::Moving;
}

void CBoundaryController::SetInitialPosition(CPoint center) {
  m_markerPos = center;
  m_currentEllipse.centerX = center.x;
  m_currentEllipse.centerY = center.y;
  m_currentEllipse.semiAxisA = 50;  // Начальный размер как в оригинале
  m_currentEllipse.semiAxisB = 50;
  m_isActive = true;
  m_deltaX = m_deltaY = 0;
  m_deltaA = m_deltaB = 0;

  // Сохранить резервную копию для отмены
  if (m_boundary) {
    m_savedBoundary.CopyFrom(*m_boundary);
    m_hasBackup = true;
  }
}

//=============================================================================
// Обработка клавиатуры (главная функция как switch в set_ell)
//=============================================================================
bool CBoundaryController::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  if (!m_isActive) {
    return false;
  }

  bool needRedraw = false;

  // Сброс приращений
  m_deltaX = m_deltaY = 0;
  m_deltaA = m_deltaB = 0;

  switch (nChar) {
    // ===== Применение эллипса =====
    case VK_SPACE:  // Space - внешняя граница
      ApplyCurrentEllipse(true);
      m_currentEllipse.semiAxisA = 50;
      m_currentEllipse.semiAxisB = 50;
      needRedraw = true;
      break;

    case VK_RETURN:  // Enter - внутренняя граница
      ApplyCurrentEllipse(false);
      m_currentEllipse.semiAxisA = 20;
      m_currentEllipse.semiAxisB = 20;
      needRedraw = true;
      break;

    case VK_ESCAPE:  // Esc - выход
      Finish();
      return true;

    // ===== Перемещение центра маркера (цифровая клавиатура) =====
    case VK_NUMPAD7:  // 7 - влево-вверх
      m_deltaX = -1;
      m_deltaY = -1;
      break;
    case VK_NUMPAD8:  // 8 - вверх
      m_deltaY = -1;
      break;
    case VK_NUMPAD9:  // 9 - вправо-вверх
      m_deltaX = 1;
      m_deltaY = -1;
      break;
    case VK_NUMPAD4:  // 4 - влево
      m_deltaX = -1;
      break;
    case VK_NUMPAD6:  // 6 - вправо
      m_deltaX = 1;
      break;
    case VK_NUMPAD1:  // 1 - влево-вниз
      m_deltaX = -1;
      m_deltaY = 1;
      break;
    case VK_NUMPAD2:  // 2 - вниз
      m_deltaY = 1;
      break;
    case VK_NUMPAD3:  // 3 - вправо-вниз
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

    // ===== Изменение осей эллипса (Shift + стрелки) =====
    case 'S':  // s - уменьшить A
      m_deltaA = -1;
      break;
    case 'T':  // t - увеличить A
      m_deltaA = 1;
      break;

    case 'W':  // w - уменьшить A (альтернатива)
      m_currentEllipse.semiAxisA = (std::max)(1, m_currentEllipse.semiAxisA - 1);
      needRedraw = true;
      break;
    case VK_F8:  // F8 - увеличить A
      m_currentEllipse.semiAxisA++;
      needRedraw = true;
      break;

    case 'U':  // u - уменьшить B
      m_currentEllipse.semiAxisB = (std::max)(1, m_currentEllipse.semiAxisB - 1);
      needRedraw = true;
      break;
    case 'V':  // v - увеличить B
      m_currentEllipse.semiAxisB++;
      needRedraw = true;
      break;

    // ===== Изменение границы на шаг (Ctrl + стрелки) =====
    case VK_NUMPAD7 | 0x100:  // Ctrl+7
      m_deltaA = 0;
      m_deltaB = -1;
      break;
    case VK_NUMPAD9 | 0x100:  // Ctrl+9
      m_deltaA = 0;
      m_deltaB = 1;
      break;
    case VK_NUMPAD1 | 0x100:  // Ctrl+1
      m_deltaA = -1;
      m_deltaB = 0;
      break;
    case VK_NUMPAD3 | 0x100:  // Ctrl+3
      m_deltaA = 1;
      m_deltaB = 0;
      break;

    default:
      return false;
  }
  // Применить приращения
  if (m_deltaX != 0 || m_deltaY != 0) {
    UpdateMarkerPosition(m_deltaX, m_deltaY);
    needRedraw = true;
  }

  if (m_deltaA != 0 || m_deltaB != 0) {
    UpdateEllipseSize(m_deltaA, m_deltaB);
    needRedraw = true;
  }

  return needRedraw;
}

//=============================================================================
// Обновление позиции маркера
//=============================================================================
void CBoundaryController::UpdateMarkerPosition(int dx, int dy) {
  m_markerPos.x += dx;
  m_markerPos.y += dy;
  ValidateMarkerPosition();

  m_currentEllipse.centerX = m_markerPos.x;
  m_currentEllipse.centerY = m_markerPos.y;
}

//=============================================================================
// Обновление размера эллипса
//=============================================================================
void CBoundaryController::UpdateEllipseSize(int da, int db) {
  m_currentEllipse.semiAxisA += da;
  m_currentEllipse.semiAxisB += db;
  ValidateEllipseSize();
}

//=============================================================================
// Валидация координат маркера
//=============================================================================
void CBoundaryController::ValidateMarkerPosition() {
  if (!m_boundary) return;

  int maxX = m_boundary->GetImageWidth() - 1;
  int maxY = m_boundary->GetImageHeight() - 1;

  if (m_markerPos.x <= 0) m_markerPos.x = 1;
  if (m_markerPos.x >= maxX) m_markerPos.x = maxX - 1;
  if (m_markerPos.y <= 0) m_markerPos.y = 1;
  if (m_markerPos.y >= maxY) m_markerPos.y = maxY - 1;
}

//=============================================================================
// Валидация размеров эллипса
//=============================================================================
void CBoundaryController::ValidateEllipseSize() {
  if (m_currentEllipse.semiAxisA < 1) m_currentEllipse.semiAxisA = 1;
  if (m_currentEllipse.semiAxisB < 1) m_currentEllipse.semiAxisB = 1;

  // Ограничение максимальным размером изображения
  if (m_boundary) {
    int maxA = m_boundary->GetImageWidth() / 2;
    int maxB = m_boundary->GetImageHeight() / 2;
    if (m_currentEllipse.semiAxisA > maxA) m_currentEllipse.semiAxisA = maxA;
    if (m_currentEllipse.semiAxisB > maxB) m_currentEllipse.semiAxisB = maxB;
  }
}

//=============================================================================
// Применение эллипса к границам
//=============================================================================
void CBoundaryController::ApplyCurrentEllipse(bool isOuter) {
  if (!m_boundary || !m_currentEllipse.IsValid()) {
    return;
  }

  m_boundary->SetEllipse(m_currentEllipse, isOuter);
}

//=============================================================================
// Отмена изменений
//=============================================================================
void CBoundaryController::Cancel() {
  if (m_hasBackup && m_boundary) {
    m_boundary->CopyFrom(m_savedBoundary);
  }
  Reset();
}

//=============================================================================
// Завершение редактирования
//=============================================================================
void CBoundaryController::Finish() { Reset(); }

//=============================================================================
// Обработка мыши
//=============================================================================
bool CBoundaryController::OnMouseMove(CPoint point) {
  if (!m_isActive) return false;

  // Можно добавить перемещение маркера мышью
  // Пока не реализовано - в оригинале только клавиатура
  return false;
}

bool CBoundaryController::OnLButtonDown(CPoint point) {
  if (!m_isActive) return false;

  // Можно добавить установку центра кликом мыши
  m_markerPos = point;
  m_currentEllipse.centerX = point.x;
  m_currentEllipse.centerY = point.y;
  ValidateMarkerPosition();

  return true;
}

//=============================================================================
// Текст статуса для отображения (как в mark_on)
//=============================================================================
CString CBoundaryController::GetStatusText() const {
  CString status;
  status.Format(_T("A:%3d  B:%3d  X:%3d  Y:%3d"), m_currentEllipse.semiAxisA,
                m_currentEllipse.semiAxisB, m_currentEllipse.centerX,
                m_currentEllipse.centerY);
  return status;
}

}  // namespace Interferometry
