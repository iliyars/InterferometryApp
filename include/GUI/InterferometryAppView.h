
// InterferometryAppView.h : interface of the CInterferometryAppView class
//

#pragma once

class CInterferometryAppView : public CView {
 protected:  // create from serialization only
  CInterferometryAppView() noexcept;
  DECLARE_DYNCREATE(CInterferometryAppView)

  // Attributes
 public:
  CInterferometryAppDoc* GetDocument() const;

  // Operations
 public:
  // Overrides
 public:
  virtual void OnDraw(CDC* pDC);  // overridden to draw this view
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

 protected:
  virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
  virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
  virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

  // Implementation
 public:
  virtual ~CInterferometryAppView();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  // Generated message map functions
 protected:
  afx_msg void OnFilePrintPreview();
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
  // Открыть файл
  afx_msg void OnFileOpen();

  // границы
  afx_msg void OnBoundarySetOuter();  // Boundary -> Set Outer Ellipse
  afx_msg void OnBoundarySetInner();  // Boundary -> Set Inner Ellipse
  afx_msg void OnBoundaryClear();

  // Трассировка
  afx_msg void OnProcessTrace();    // Process -> Trace Fringe
  afx_msg void OnProcessClear();    // Process -> Clear Lines

  // Мышь
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

  // Клавиатура
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  DECLARE_MESSAGE_MAP()

  private:
  // Текщий режим редактирования
  Interferometry::EditMode m_currentMode;

  // ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ отрисовки
// ============================================================================
private:
// Нарисовать точки границ (пока элипс не построен)
void DrawBoundaryPoints(CDC* pDC, const std::vector<CPoint>& points, COLORREF color);

// Нарисовать элипс границ
void DrawEllipseBoundary(CDC* pDC, bool isOuter);

// Нарисовать одну трассированную линию
void DrawFringeLine(CDC* pDC, const Interferometry::FringeLine& line);

// Обновить текст в статусбаре
void UpdateStatusBar();

// Получить текст для текущего режима
CString GetModeText() const;
};

#ifndef _DEBUG  // debug version in InterferometryAppView.cpp
inline CInterferometryAppDoc* CInterferometryAppView::GetDocument() const {
  return reinterpret_cast<CInterferometryAppDoc*>(m_pDocument);
}
#endif
