
// InterferometryAppView.cpp : implementation of the CInterferometryAppView
// class
//
#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview,
// thumbnail and search filter handlers and allows sharing of document code with
// that project.
#ifndef SHARED_HANDLERS
#include "InterferometryApp.h"
#endif

#include "InterferometryAppDoc.h"
#include "InterferometryAppView.h"
#include "MainFrm.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CInterferometryAppView

IMPLEMENT_DYNCREATE(CInterferometryAppView, CView)

BEGIN_MESSAGE_MAP(CInterferometryAppView, CView)
// Standard printing commands
ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CInterferometryAppView::OnFilePrintPreview)
ON_WM_CONTEXTMENU()
ON_WM_RBUTTONUP()

ON_COMMAND(ID_FILE_OPEN, &CInterferometryAppView::OnFileOpen)

// Границы
ON_COMMAND(ID_BOUNDARY_SET_OUTER, &CInterferometryAppView::OnBoundarySetOuter)
ON_COMMAND(ID_BOUNDARY_SET_INNER, &CInterferometryAppView::OnBoundarySetInner)
ON_COMMAND(ID_BOUNDARY_CLEAR, &CInterferometryAppView::OnBoundaryClear)

// Трассировка
ON_COMMAND(ID_PROCESS_TRACE, &CInterferometryAppView::OnProcessTrace)
ON_COMMAND(ID_PROCESS_CLEAR, &CInterferometryAppView::OnProcessClear)

// События
ON_WM_LBUTTONDOWN()
ON_WM_KEYDOWN()
ON_WM_MOUSEMOVE()

END_MESSAGE_MAP()

// CInterferometryAppView construction/destruction

CInterferometryAppView::CInterferometryAppView() noexcept
    : m_currentMode(Interferometry::EditMode::MODE_NONE),
      m_imageOffsetX(10),
      m_imageOffsetY(60),
      m_cacheBitmap(NULL) {}

CInterferometryAppView::~CInterferometryAppView()
{
  InvalidateChachedBitmap();
}

BOOL CInterferometryAppView::PreCreateWindow(CREATESTRUCT &cs)
{
  // TODO: Modify the Window class or styles here by modifying
  //  the CREATESTRUCT cs

  return CView::PreCreateWindow(cs);
}

// ========================
// Преобразование координат окна -> координат изображения
// ===============================
CPoint CInterferometryAppView::WindowToImage(CPoint windowPt) const
{
  return CPoint(windowPt.x - m_imageOffsetX, windowPt.y - m_imageOffsetY);
}

// ========================
// Кэширование HBITMAP
// ========================
void CInterferometryAppView::InvalidateCachedBitmap()
{
  if (m_chachedBitmap)
  {
    ::DeleteObject(m_chacheBitmap);
    m_chacheBitmap = NULL;
  }
}

void CInterferometryAppView::EnsureCachedBitmap()
{
  if (m_cachedBitmap)
    return;

  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc || !pDoc->HasImage())
    return;

  const Interferometry::ImageLoader &loader = pDoc->GetImageLoader();
  m_cachedBitmap = loader.CreateHBITMAP();
}

// CInterferometryAppView drawing

void CInterferometryAppView::OnDraw(CDC *pDC)
{
  CInterferometryAppDoc *pDoc = GetDocument();
  ASSERT_VALID(pDoc);
  if (!pDoc)
    return;

  CRect rect;
  GetClientRect(&rect);

  if (pDoc->HasImage())
  {
    const Interferometry::ImageLoader &loader = pDoc->GetImageLoader();
    int width = loader.GetWidth();
    int height = loader.GetHeight();

    // Информация о размере
    CString info;
    info.Format(
        _T("Image: %d x %d | Mode: %s"),
        width, height, (LPCTSTR)GetModeText());
    CRect textRect = rect;
    textRect.bottom = m_imageOffsetY - 5;
    pDC->DrawText(info, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Рисуем изображение с кэшированием
    EnsureCachedBitmap();
    if (m_cachedBitmap)
    {
      CDC dcMem;
      dcMem.CreateCompatibleDC(pDC);
      HBITMAP holdBitmap = (HBITMAP)dcMem.SelectObject(m_cachedBitmap);

      pDC->BitBlt(m_imageOffsetX, m_imageOffsetY, width, height, &dcMem, 0, 0,
                  SRCCOPY);

      dcMem.SelectObject(holdBitmap);
      // НЕ удаляем m_cachedBitmap — он кэширован!
    }

    // === Все дальнейшие координаты в пространстве изображения ===
    // Сдвигаем начало координат DC на offset изображения
    pDC->SetViewportOrg(m_imageOffsetX, m_imageOffsetY);

    // === 2. Рисуем точки границ ===
    // ИСПРАВЛЕНО: первое условие теперь MODE_SET_OUTER_BOUNDARY (был баг — оба
    // были INNER)
    if (m_currentMode == Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY)
    {
      DrawBoundaryPoints(pDC, pDoc->GetOuterEllipsePoints(),
                         RGB(255, 0, 0)); // Красный
    }
    else if (m_currentMode ==
             Interferometry::EditMode::MODE_SET_INNER_BOUNDARY)
    {
      DrawBoundaryPoints(pDC, pDoc->GetInnerEllipsePoints(),
                         RGB(0, 0, 255)); // Синий
    }

    // === 3. Рисуем эллипсы границ ===
    if (pDoc->HasOuterBoundary())
    {
      DrawEllipseBoundary(pDC, true);
    }
    if (pDoc->HasInnerBoundary())
    {
      DrawEllipseBoundary(pDC, false);
    }

    // === 4. Рисуем трассированные линии ===
    const auto &lines = pDoc->GetFringeLines();
    for (const auto &line : lines)
    {
      DrawFringeLine(pDC, line);
    }

    // Восстановить начало координат
    pDC->SetViewportOrg(0, 0);
  }
  else
  {
    // ===== НЕТ ИЗОБРАЖЕНИЯ - показываем инструкцию =====
    CString msg =
        _T("Interferometry Application\n\n")
        _T("Press Ctrl+O or File -> Open to load an interferogram image\n")
        _T("Supported formats: BMP, JPG, PNG");

    pDC->DrawText(msg, &rect, DT_CENTER | DT_VCENTER);
  }
}
// CInterferometryAppView printing

void CInterferometryAppView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
  AFXPrintPreview(this);
#endif
}

BOOL CInterferometryAppView::OnPreparePrinting(CPrintInfo *pInfo)
{
  // default preparation
  return DoPreparePrinting(pInfo);
}

void CInterferometryAppView::OnBeginPrinting(CDC * /*pDC*/,
                                             CPrintInfo * /*pInfo*/)
{
  // TODO: add extra initialization before printing
}

void CInterferometryAppView::OnEndPrinting(CDC * /*pDC*/,
                                           CPrintInfo * /*pInfo*/)
{
  // TODO: add cleanup after printing
}

void CInterferometryAppView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
  ClientToScreen(&point);
  OnContextMenu(this, point);
}

void CInterferometryAppView::OnContextMenu(CWnd * /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
  theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x,
                                                point.y, this, TRUE);
#endif
}

// CInterferometryAppView diagnostics

#ifdef _DEBUG
void CInterferometryAppView::AssertValid() const { CView::AssertValid(); }

void CInterferometryAppView::Dump(CDumpContext &dc) const { CView::Dump(dc); }

CInterferometryAppDoc *CInterferometryAppView::GetDocument()
    const // non-debug version is inline
{
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CInterferometryAppDoc)));
  return (CInterferometryAppDoc *)m_pDocument;
}
#endif //_DEBUG

// CInterferometryAppView message handlers

void CInterferometryAppView::OnFileOpen()
{
  // Диалог выбора файла
  CFileDialog dlg(TRUE, _T("bmp"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                  _T("Image Files (*.bmp;*.jpg;*.png)|*.bmp;*.jpg;*.png|All ")
                  _T("Files (*.*)|*.*||"));

  if (dlg.DoModal() == IDOK)
  {
    CInterferometryAppDoc *pDoc = GetDocument();
    ASSERT_VALID(pDoc);
    if (!pDoc)
      return;

    CString filePath = dlg.GetPathName();
    std::wstring wFilePath(filePath);

    // Сбросить кэш при загрузке нового изображения
    InvalidateCachedBitmap();

    if (pDoc->LoadImage(wFilePath))
    {
      // Сбросить режим
      m_currentMode = Interferometry::EditMode::MODE_NONE;
      UpdateStatusBar();

      const Interferometry::ImageLoader &loader = pDoc->GetImageLoader();
      CString msg;
      msg.Format(
          _T("Image loaded: %d x %d pixels\n\n")
          _T("Next steps:\n")
          _T("1. Set outer boundary (Boundary -> Set Outer)\n")
          _T("2. Set inner boundary (Boundary -> Set Inner)\n")
          _T("3. Trace fringes (Process -> Trace)"),
          loader.GetWidth(), loader.GetHeight());
      AfxMessageBox(msg);
    }
    else
    {
      AfxMessageBox(
          _T("Failed to load image!\n\nCheck that the file is a valid image."));
    }
  }
}

void CInterferometryAppView::OnBoundarySetOuter()
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc || !pDoc->HasImage())
  {
    AfxMessageBox(_T("Please load an image first!"));
    return;
  }

  // Переключить в режим установки внешней границы
  m_currentMode = Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY;
  UpdateStatusBar();

  AfxMessageBox(_T("Click 8-12 points on outer boundary. \nPress ENTER when done"));
}

void CInterferometryAppView::OnBoundarySetInner()
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc || !pDoc->HasImage())
  {
    AfxMessageBox(_T("Please load an image first!"));
    return;
  }

  if (!pDoc->HasOuterBoundary())
  {
    AfxMessageBox(_T("Please set outer boundary first!"));
    return;
  }

  // Переключить в режим установки внутренней границы
  m_currentMode = Interferometry::EditMode::MODE_SET_INNER_BOUNDARY;
  UpdateStatusBar();
  Invalidate();

  AfxMessageBox(_T("Click 8-12 points on the inner boundary.\nPress ENTER when done."));
}

void CInterferometryAppView::OnBoundaryClear()
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  if (AfxMessageBox(_T("Clear all boundaries?"), MB_YESNO) == IDYES)
  {
    pDoc->ClearBoundaries();
    m_currentMode = Interferometry::EditMode::MODE_NONE;
    UpdateStatusBar();
  }
}

void CInterferometryAppView::OnProcessTrace()
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc || !pDoc->HasImage())
  {
    AfxMessageBox(_T("Please load image first!"));
    return;
  }

  if (!pDoc->HasOuterBoundary() || !pDoc->HasInnerBoundary())
  {
    AfxMessageBox(_T("Please set both boundaries first!"));
    return;
  }

  // Переключить в режим трассировки
  m_currentMode = Interferometry::EditMode::MODE_TRACE_FRINGE;
  UpdateStatusBar();
  Invalidate();

  AfxMessageBox(_T("CLick on fringe to trace in."));
}

void CInterferometryAppView::OnProcessClear()
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  if (pDoc->GetFringeLineCount() > 0)
  {
    if (AfxMessageBox(_T("Clear all traced lines?"), MB_YESNO) == IDYES)
    {
      pDoc->ClearFringeLines();
    }
  }
}

// === Обработчики событий ===

void CInterferometryAppView::OnLButtonDown(UINT nFlags, CPoint point)
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  // Преобразуем координаты окна в координаты изобравжения
  CPoint imagePt = WindowToImage(point);

  // Проверка что клик внутри изобравжения
  if (pDoc->HasImage())
  {
    int w = pDoc->GetImageLoader().GetWidth();
    int h = pDoc->GetImageLoader().GetHeight();
    if (imagePt.x < 0 || imagePt.x >= w || imagePt.y < 0 || imagePt.y >= h)
    {
      // Клик вне изобравжения - игнорируем в рабочих режимах
      if (m_currentMode != Interferometry::EditMode::MODE_NONE)
      {
        CView::OnLButtonDown(nFlags, point);
        return;
      }
    }
  }

  switch (m_currentMode)
  {
  case Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY:
    // Добавить точку для внешней границы
    pDoc->AddBoundaryPoint(imagePt, true);
    UpdateStatusBar();
    Invalidate();
    break;

  case Interferometry::EditMode::MODE_SET_INNER_BOUNDARY:
    // Добавить точку для внутренней границы
    pDoc->AddBoundaryPoint(imagePt, false);
    UpdateStatusBar();
    Invalidate();
    break;

  case Interferometry::EditMode::MODE_TRACE_FRINGE:
    // Трассировать полосу
    if (pDoc->TraceFringe(imagePt))
    {
      CString msg;
      msg.Format(_T("Line %d traced (%d points)"),
                 pDoc->GetFringeLineCount(),
                 (int)pDoc->GetFringeLines().back().GetPointCount());
      // Показываем в статусбаре вместо MessageBox
      CMainFrame *pMainFrame = (CMainFrame *)AfxGetMainWnd();
      if (pMainFrame)
        pMainFrame->SetMessageText(msg);
      Invalidate();
    }
    break;

  default:
    // Режим просмотра
    break;
  }

  CView::OnLButtonDown(nFlags, point);
}

void CInterferometryAppView::OnMouseMove(UINT nFlags, CPoint point)
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (pDoc && pDoc->HasImage())
  {
    CPoint imagePt = WindowToImage(point);
    int w = pDoc->GetImageLoader().GetWidth();
    int h = pDoc->GetImageLoader().GetHeight();

    if (imagePt.x >= 0 && imagePt.x < w && imagePt.y >= 0 && imagePt.y < h)
    {
      // Показать координаты и интенсивность пикселя в статусбаре
      unsigned char pixel = pDoc->GetImageLoader().GetPixel(imagePt.x, imagePt.y);
      CString info;
      info.Format(_T("X:%d Y:%d I:%d"), imagePt.x, imagePt.y, (int)pixel);

      CMainFrame *pMainFrame = (CMainFrame *)AfxGetMainWnd();
      if (pMainFrame)
      {
        // Показывать в правой панели статусбара
        pMainFrame->SetMessageText(GetModeText() + _T(" | ") + info);
      }
    }
  }

  CView::OnMouseMove(nFlags, point);
}

void CInterferometryAppView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  // ENTER - завершить ввод точек и построить эллипс
  if (nChar == VK_RETURN)
  {
    if (m_currentMode == Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY)
    {
      auto status = pDoc->GetBoundaryStatus();
      if (status.outerPointCount < 5)
      {
        AfxMessageBox(_T("Need at least 5 points! Please add more points."));
      }
      else if (status.outerPointCount > 20)
      {
        AfxMessageBox(_T("Too many points! Maximum is 20."));
      }
      else
      {
        // Построить эллипс
        if (pDoc->FitEllipse(true))
        {
          m_currentMode = Interferometry::EditMode::MODE_NONE;
          UpdateStatusBar();
          Invalidate();
          AfxMessageBox(_T("Outer boundary set!\n\nNext: Set inner boundary")
                        _T("(Boundary -> Set Inner)"));
        }
      }
    }
    else if (m_currentMode == Interferometry::EditMode::MODE_SET_INNER_BOUNDARY)
    {
      auto status = pDoc->GetBoundaryStatus();
      if (status.innerPointCount < 5)
      {
        AfxMessageBox(_T("Need at least 5 points! Please add more points."));
      }
      else if (status.innerPointCount > 20)
      {
        AfxMessageBox(_T("Too many points! Maximum is 20."));
      }
      else
      {
        // Построить эллипс
        if (pDoc->FitEllipse(false))
        {
          m_currentMode = Interferometry::EditMode::MODE_NONE;
          UpdateStatusBar();
          Invalidate();
          AfxMessageBox(_T("Inner boundary set!\n\nReady to trace!")
                        _T("Process -> Trace"));
        }
      }
    }
  }
  // ESC - отменить текущий режим
  else if (nChar == VK_ESCAPE)
  {
    m_currentMode = Interferometry::EditMode::MODE_NONE;
    UpdateStatusBar();
    Invalidate();
  }

  CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

// === Вспомогательные методы
void CInterferometryAppView::DrawBoundaryPoints(CDC *pDC,
                                                const std::vector<CPoint> &points,
                                                COLORREF color)
{
  if (points.empty())
    return;

  // Создать перо и кисть
  CPen pen(PS_SOLID, 2, color);
  CBrush brush(color);
  CPen *poldPen = pDC->SelectObject(&pen);
  CBrush *poldBrush = pDC->SelectObject(&brush);

  // Нарисовать кружки
  for (const auto &pt : points)
  {
    pDC->Ellipse(pt.x - 4, pt.y - 4, pt.x + 4, pt.y + 4);
  }

  // Нарисовать линию между точками
  if (points.size() > 1)
  {
    CPen dashedPen(PS_DOT, 1, color);
    pDC->SelectObject(&dashedPen);

    pDC->MoveTo(points[0]);
    for (size_t i = 1; i < points.size(); i++)
    {
      pDC->LineTo(points[i]);
    }
  }

  pDC->SelectObject(poldPen);
  pDC->SelectObject(poldBrush);
}

void CInterferometryAppView::DrawEllipseBoundary(CDC *pDC, bool isOuter)
{
  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  const Interferometry::CEllipseBoundary &boundary = pDoc->GetBoundary();
  const auto &rows = boundary.GetAllBoundaries();

  if (rows.empty())
    return;

  // Выбрать цвет
  COLORREF color = isOuter ? RGB(255, 255, 255) : RGB(0, 255, 255); // Белый / Cyan
  CPen pen(PS_SOLID, 1, color);
  CPen *poldPen = pDC->SelectObject(&pen);

  int height = (int)rows.size();

  int firstY = -1;
  for (int y = 0; y < height; y++)
  {
    bool hasBoundary = isOuter ? rows[y].HasOuterBoundary() : rows[y].HasInnerBoundary();
    if (hasBoundary)
    {
      int leftX = isOuter ? rows[y].LeftOuter : rows[y].leftInner;
      int rightX = isOuter ? rows[y].rightOuter : rows[y].rightInner;
      if (leftX > 0 || rightX > 0)
      {
        if (firstY < 0)
        {
          firstY = y;
          pDC->MoveTo(leftX, y);
        }
        // Левая дуга
        pDC->LineTo(leftX, y);
      }
    }
  }

  // Правая дуга
  firstY = -1;
  for (int y = 0; y < height; y++)
  {
    bool hasBoundary =
        isOuter ? rows[y].HasOuterBoundary() : rows[y].HasInnerBoundary();
    if (hasBoundary)
    {
      int rightX = isOuter ? rows[y].rightOuter : rows[y].rightInner;
      if (rightX > 0)
      {
        if (firstY < 0)
        {
          firstY = y;
          pDC->MoveTo(rightX, y);
        }
        pDC->LineTo(rightX, y);
      }
    }
  }

  pDC->SelectObject(poldPen);
}

void CInterferometryAppView::DrawFringeLine(CDC *pDC,
   const Interferometry::FringeLine &line)
{
  if (line.IsEmpty())
    return;

  CPen pen(PS_SOLID, 2, line.color);
  CPen *pOldPen = pDC->SelectObject(&pen);

  const auto &points = line.points;
  pDC->MoveTo(points[0].x, points[0].y);

  for (size_t i = 1; i < points.size(); i++)
  {
    pDC->LineTo(points[i].x, points[i].y);
  }

  // Нарисовать начальную точку маркером
  CBrush brush(line.color);
  CBrush *pOldBrush = pDC->SelectObject(&brush);
  pDC->Ellipse(points[0].x - 3, points[0].y - 3, points[0].x + 3,
               points[0].y + 3);

  pDC->SelectObject(pOldBrush);
  pDC->SelectObject(pOldPen);
}

void CInterferometryAppView::UpdateStatusBar()
{
  CMainFrame *pMainFrame = (CMainFrame *)AfxGetMainWnd();
  if (!pMainFrame)
    return;

  CInterferometryAppDoc *pDoc = GetDocument();
  if (!pDoc)
    return;

  // Построить текст статуса
  CString statusText = GetModeText();

  // Добавить информацию о границах
  auto status = pDoc->GetBoundaryStatus();
  CString boundaryInfo;

  if (m_currentMode == Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY)
  {
    boundaryInfo.Format(_T(" | Points: %d/8-12"), status.outerPointCount);
  }
  else if (m_currentMode == Interferometry::EditMode::MODE_SET_INNER_BOUNDARY)
  {
    boundaryInfo.Format(_T(" | Points: %d/8-12"), status.innerPointCount);
  }
  else
  {
    boundaryInfo.Format(_T(" | Boundaries: %s/%s | Lines: %d"),
                        status.hasOuterBoundary ? _T("Outer") : _T("-"),
                        status.hasInnerBoundary ? _T("Inner") : _T("-"),
                        pDoc->GetFringeLineCount());
  }

  statusText += boundaryInfo;

  // Установить в статусбар (индекс 0 - первая панель)
  pMainFrame->SetMessageText(statusText);
}

CString CInterferometryAppView::GetModeText() const
{
  switch (m_currentMode)
  {
  case Interferometry::EditMode::MODE_SET_OUTER_BOUNDARY:
    return _T("MODE: Setting outer boundary - Click points, press ENTER when done");

  case Interferometry::EditMode::MODE_SET_INNER_BOUNDARY:
    return _T("MODE: Setting inner boundary - Click points, press ENTER when done");

  case Interferometry::EditMode::MODE_TRACE_FRINGE:
    return _T("MODE: Tracing - Click on a fringe to trace it");

  default:
    return _T("Ready");
  }
}
