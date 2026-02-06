// BoundaryController.h
// Интерактивное управление эллиптическими границами для MFC View
// Портировано из SCAN360/MARKER.C функция set_ell()

#pragma once

#include "EllipseBoundary.h"
#include <afxwin.h>  

namespace Interferometry {
// Режим редактирования
enum class BoundaryEditMode {
  Moving,        // Перемещение центра маркера
  ResizingAxes,  // Изменение осей элипса
  ResizingStep   // Изменение границ пошагово
};

// Состояние интерактивного редактора границ
class CBoundaryController {
 public:
  CBoundaryController();
  ~CBoundaryController();

  // Инициализация
  void Initialize(CEllipseBoundary* boundary);
  void Reset();

  // Обработка пользовательского ввода
  // Возвращает true, если нужна перерисовка
  bool OnKeyDown(uint32_t nChar, uint32_t nRepCnt, uint32_t nFlags);
  bool OnMouseMove(CPoint point);
  bool OnLButtonDown(CPoint point);

  // Примнение текщео элипса
  void ApplyCurrentEllipse(bool isOuter);

  // Отмена изменений
  void Cancel();

  // Завершение редактирования
  void Finish();

  // Получение текщих парамтеров для отображения
  const EllipseParams& GetCurrentEllipse() const { return m_currentEllipse; }
  CPoint GetMarkerPosition() const { return m_markerPos; }
  bool IsActive() const { return m_isActive; }
  BoundaryEditMode GetEditMode() const { return m_editMode; }

  // Установка начальной позиции
  void SetInitialPosition(CPoint center);

  // Статус для отображения
  CString GetStatusText() const;

 private:
  // Обновление позиции маркера
  void UpdateMarkerPosition(int dx, int dy);
  void UpdateEllipseSize(int da, int db);
  void UpdateEllipseAxis(int da, int db);

  // Валидация координат
  void ValidateMarkerPosition();
  void ValidateEllipseSize();

  // Вычисление приращений для автоповтора
  void CalculateIncrements(uint32_t nChar);

 private:
  CEllipseBoundary* m_boundary;    // Ссылка на границы
  EllipseParams m_currentEllipse;  // Текщие параметры эллипса
  CPoint m_markerPos;              // позиция маркера (центр эллипса)

  bool m_isActive;              // активен ли редактор
  BoundaryEditMode m_editMode;  // Текщий режим редактирования

  // Приращения для автоповтора (как xx, yy, aa, bb в оригинале)
  int m_deltaX;
  int m_deltaY;
  int m_deltaA;
  int m_deltaB;

  // Сохранение состояния для отмены
  CEllipseBoundary m_savedBoundary;
  bool m_hasBackup;
};
}  // namespace Interferometry
