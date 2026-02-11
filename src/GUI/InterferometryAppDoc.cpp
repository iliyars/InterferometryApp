
// InterferometryAppDoc.cpp : implementation of the CInterferometryAppDoc class
//
#include "pch.h"
#include "framework.h"

// SHARED_HANDLERS can be defined in an ATL project implementing preview,
// thumbnail and search filter handlers and allows sharing of document code with
// that project.
#ifndef SHARED_HANDLERS
#include "InterferometryApp.h"
#endif

#include <propkey.h>

#include "InterferometryAppDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CInterferometryAppDoc

IMPLEMENT_DYNCREATE(CInterferometryAppDoc, CDocument)

BEGIN_MESSAGE_MAP(CInterferometryAppDoc, CDocument)
END_MESSAGE_MAP()

// CInterferometryAppDoc construction/destruction

CInterferometryAppDoc::CInterferometryAppDoc() noexcept { m_hasImage = false; }

CInterferometryAppDoc::~CInterferometryAppDoc() {}

BOOL CInterferometryAppDoc::OnNewDocument() {
  if (!CDocument::OnNewDocument()) return FALSE;

  // TODO: add reinitialization code here
  // (SDI documents will reuse this document)

  ClearImage();
  ClearBoundaries();
  ClearFringeLines();

  return TRUE;
}

// CInterferometryAppDoc serialization

void CInterferometryAppDoc::Serialize(CArchive& ar) {
  if (ar.IsStoring()) {
    // TODO: add storing code here
  } else {
    // TODO: add loading code here
  }
}

#ifdef SHARED_HANDLERS

// Support for thumbnails
void CInterferometryAppDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds) {
  // Modify this code to draw the document's data
  dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

  CString strText = _T("TODO: implement thumbnail drawing here");
  LOGFONT lf;

  CFont* pDefaultGUIFont =
      CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
  pDefaultGUIFont->GetLogFont(&lf);
  lf.lfHeight = 36;

  CFont fontDraw;
  fontDraw.CreateFontIndirect(&lf);

  CFont* pOldFont = dc.SelectObject(&fontDraw);
  dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
  dc.SelectObject(pOldFont);
}

// Support for Search Handlers
void CInterferometryAppDoc::InitializeSearchContent() {
  CString strSearchContent;
  // Set search contents from document's data.
  // The content parts should be separated by ";"

  // For example:  strSearchContent = _T("point;rectangle;circle;ole object;");
  SetSearchContent(strSearchContent);
}

void CInterferometryAppDoc::SetSearchContent(const CString& value) {
  if (value.IsEmpty()) {
    RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
  } else {
    CMFCFilterChunkValueImpl* pChunk = nullptr;
    ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
    if (pChunk != nullptr) {
      pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
      SetChunkValue(pChunk);
    }
  }
}

#endif  // SHARED_HANDLERS

// CInterferometryAppDoc diagnostics

#ifdef _DEBUG
void CInterferometryAppDoc::AssertValid() const { CDocument::AssertValid(); }

void CInterferometryAppDoc::Dump(CDumpContext& dc) const {
  CDocument::Dump(dc);
}
#endif  //_DEBUG

// CInterferometryAppDoc commands

bool CInterferometryAppDoc::LoadImage(const std::wstring& filename) {
  if (m_imageLoader.Load(filename)) {
    m_hasImage = true;

    ClearBoundaries();
    ClearFringeLines();

    m_boundary.Initialize(m_imageLoader.GetWidth(), m_imageLoader.GetHeight());

    SetModifiedFlag(TRUE);
    UpdateAllViews(NULL);

    return true;
  }

  m_hasImage = false;
  return false;
}

void CInterferometryAppDoc::ClearImage() {
  m_hasImage = false;
  SetModifiedFlag(TRUE);
  UpdateAllViews(NULL);
}

// ============================================================================
// РЕАЛИЗАЦИЯ МЕТОДОВ - Границы (эллипсы)
// ============================================================================

void CInterferometryAppDoc::AddBoundaryPoint(CPoint pt, bool isOuter) {
  if (!m_hasImage) return;

  int width = m_imageLoader.GetWidth();
  int height = m_imageLoader.GetHeight();

  if (pt.x < 0 || pt.x >= width || pt.y < 0 || pt.y >= height) return;

  // Добавить точку в соответсвующй массив
  if (isOuter) {
    m_outerEllipsePoints.push_back(pt);
  } else {
    m_innerEllipsePoints.push_back(pt);
  }

  SetModifiedFlag(TRUE);
  UpdateAllViews(NULL);
}

bool CInterferometryAppDoc::FitEllipse(bool isOuter) {
  // Получить массив точек
  std::vector<CPoint>& points =
      isOuter ? m_outerEllipsePoints : m_innerEllipsePoints;

  // Проверить что достаточно точек
  if (points.size() < 5) {
    AfxMessageBox(_T("Need at least 5 points to fit an ellipse!"));
    return false;
  }

  // Конвертировать CPoint -> cv::Point2f Для OpenCV
  std::vector<cv::Point2f> cvPoints;
  for (const auto& pt : points) {
    cvPoints.push_back(cv::Point2f((float)pt.x, (float)pt.y));
  }

  try {
    // Построить эллипс методом наименьших квадратов
    cv::RotatedRect ellipse = cv::fitEllipse(cvPoints);

    // Конвертация в наши параметры
    Interferometry::EllipseParams params;
    params.centerX = (int)ellipse.center.x;
    params.centerY = (int)ellipse.center.y;
    params.semiAxisA = (int)(ellipse.size.width / 2.0f);
    params.semiAxisB = (int)(ellipse.size.width / 2.0f);
    // angle игнорируем, так как в оригинальном коде эллипс без поворота

    // Применить эллипс к границам
    m_boundary.SetEllipse(params, isOuter);

    if (isOuter) {
      m_outerEllipseComplete = true;
    } else {
      m_innerEllipseComplete = true;
    }

    SetModifiedFlag(TRUE);
    UpdateAllViews(NULL);

    return true;
  } catch (cv::Exception& e) {
    CString msg;
    msg.Format(_T("Failed to fit ellipse: %s"), CString(e.what()));
    AfxMessageBox(msg);
    return false;
  }
}

Interferometry::BoundaryStatus CInterferometryAppDoc::GetBoundaryStatus()
    const {
  Interferometry::BoundaryStatus status;
  status.hasOuterBoundary = m_outerEllipseComplete;
  status.hasInnerBoundary = m_innerEllipseComplete;
  status.outerPointCount = (int)m_outerEllipsePoints.size();
  status.innerPointCount = (int)m_outerEllipsePoints.size();
  return status;
}

void CInterferometryAppDoc::ClearBoundaries() {
  m_outerEllipsePoints.clear();
  m_innerEllipsePoints.clear();
  m_outerEllipseComplete = false;
  m_innerEllipseComplete = false;

  // Сбросить границы (если изобравжение загружено)
  if (m_hasImage) {
    m_boundary.Initialize(m_imageLoader.GetWidth(), m_imageLoader.GetHeight());
  }

  SetModifiedFlag(TRUE);
  UpdateAllViews(NULL);
}

// ============================================================================
// РЕАЛИЗАЦИЯ МЕТОДОВ - Трассировка
// ============================================================================

bool CInterferometryAppDoc::TraceFringe(CPoint startPoint) {
  if (!m_hasImage) {
    AfxMessageBox(_T("No image loaded!"));
    return false;
  }

  // Проверить что границы установлены
  if (!m_outerEllipseComplete || !m_innerEllipseComplete) {
    AfxMessageBox(_T("PLease set both outer and inner boundaries first!"));
    return false;
  }

  // Проверить что точка внутри изображения
  int width = m_imageLoader.GetWidth();
  int height = m_imageLoader.GetHeight();

  if (startPoint.x < 0 || startPoint.x >= width || startPoint.y < 0 ||
      startPoint.y >= height) {
    return false;
  }

  // Инициализация трассировки
  if (!m_fringeTracer.IsInitialize()) {
    m_fringeTracer.Initialize(m_imageLoader.GetImage(), m_boundary);
  }

  // Трассировать линию
  std::vector<Interferometry::CTracerPoint> tracedPoints =
      m_fringeTracer.TraceLine(startPoint.x, startPoint.y);

  // Проверить что получили точки
  if (tracedPoints.empty()) {
    AfxMessageBox(_T("Tracing failed! Try clicking on a clearer fringe."));
    return false;
  }

  // Создать новую линию
  Interferometry::FringeLine newLine(m_nextLineId);
  newLine.points = tracedPoints;

  // Назначить цвет (циклически меняем цвета)
  COLORREF colors[] = {
      RGB(255, 255, 0),  // Желтый
      RGB(0, 255, 255),  // Cyan
      RGB(255, 0, 255),  // Magenta
      RGB(0, 255, 0),    // Зеленый
      RGB(255, 128, 0),  // Оранжевый
  };
  int colorIndex = (m_nextLineId - 1) % 5;
  newLine.color = colors[colorIndex];

  // Добавить линию в список
  m_fringeLines.push_back(newLine);
  m_nextLineId++;

  SetModifiedFlag(TRUE);
  UpdateAllViews(NULL);

  return true;
}

void CInterferometryAppDoc::ClearFringeLines() {
  m_fringeLines.clear();
  m_nextLineId = 1;

  SetModifiedFlag(TRUE);
  UpdateAllViews(NULL);
}
