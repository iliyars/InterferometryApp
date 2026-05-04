
// InterferometryAppDoc.h : interface of the CInterferometryAppDoc class
//

#pragma once

#include "EllipseBoundary.h"
#include "FringeTracer.h"
#include "ImageLoader.h"
#include "TracingTypes.h"

class CInterferometryAppDoc : public CDocument {
 protected:  // create from serialization only
  CInterferometryAppDoc() noexcept;
  DECLARE_DYNCREATE(CInterferometryAppDoc)

  // Attributes
 public:
  // Operations
 public:
  // Overrides
 public:
  virtual BOOL OnNewDocument();
  virtual void Serialize(CArchive& ar);
#ifdef SHARED_HANDLERS
  virtual void InitializeSearchContent();
  virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif  // SHARED_HANDLERS

  // Implementation
 public:
  virtual ~CInterferometryAppDoc();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  // Generated message map functions
 protected:
  DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
  // Helper function that sets search content for a Search Handler
  void SetSearchContent(const CString& value);
#endif  // SHARED_HANDLERS

  // ============================================================================
  // ДАННЫЕ ПРИЛОЖЕНИЯ
  // ============================================================================
 private:
  // === Изображение ===
  Interferometry::ImageLoader m_imageLoader;
  bool m_hasImage;

  // === Границы (эллипсы) ===
  Interferometry::CEllipseBoundary m_boundary;  // Хранит границы arr_coord_ell
  std::vector<CPoint>
      m_outerEllipsePoints;  // Точки внешнего эллипса (клики мыши)
  std::vector<CPoint> m_innerEllipsePoints;  // Точки внутреннего эллипса
  bool m_outerEllipseComplete;               // Внешний эллипс построен?
  bool m_innerEllipseComplete;

  // === Трассировка ===
  Interferometry::CFringeTracer m_fringeTracer;  // Трассировщик полос
  std::vector<Interferometry::FringeLine>
      m_fringeLines;  // Все трассированные линии
  int m_nextLineId;

  // ============================================================================
  // ПУБЛИЧНЫЕ МЕТОДЫ - Изображение
  // ============================================================================
 public:
  // Загрузить изображение
  bool LoadImage(const std::wstring& filename);

  // Есть ли изображение?
  bool HasImage() const { return m_hasImage; }

  // Получить ImageLoader
  const Interferometry::ImageLoader& GetImageLoader() const {
    return m_imageLoader;
  }
  Interferometry::ImageLoader& GetImageLoader() { return m_imageLoader; }

  // Очистить изображение
  void ClearImage();

  // ============================================================================
  // ПУБЛИЧНЫЕ МЕТОДЫ - Границы (эллипсы)
  // ============================================================================
 public:
  // Добавить точку для элипса (вызывается при клике мышки)
  void AddBoundaryPoint(CPoint pt, bool isOuter);

  // Построить элипс по набранным точкам
  bool FitEllipse(bool isOuter);

  // Получить точки для отрисовки (пока эллипс не построен)
  const std::vector<CPoint>& GetOuterEllipsePoints() const { return m_outerEllipsePoints; }
  const std::vector<CPoint>& GetInnerEllipsePoints() const { return m_innerEllipsePoints; }

  // Получить границы
  const Interferometry::CEllipseBoundary& GetBoundary() const {
    return m_boundary;
  }
  // Проверить состояние границ
  bool HasOuterBoundary() const { return m_outerEllipseComplete; }
  bool HasInnerBoundary() const { return m_innerEllipseComplete; }

  // Получить статус границ (для статусбара)
  Interferometry::BoundaryStatus GetBoundaryStatus() const;

  // Очистить границы
  void ClearBoundaries();

  // ============================================================================
  // ПУБЛИЧНЫЕ МЕТОДЫ - Трассировка
  // ============================================================================
 public:
  // Трассировать полосу начиная с точки (вызывается при клике мышки)
  bool TraceFringe(CPoint startPoint);

  // Получить все линии
  const std::vector<Interferometry::FringeLine>& GetFringeLines() const {
    return m_fringeLines;
  }



  // Очистить все линии
  void ClearFringeLines();

  // Получить колличество линий
  int GetFringeLineCount() const { return (int)m_fringeLines.size(); }
};
