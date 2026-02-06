
// InterferometryAppView.cpp : implementation of the CInterferometryAppView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "InterferometryApp.h"
#endif

#include "InterferometryAppDoc.h"
#include "InterferometryAppView.h"

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
END_MESSAGE_MAP()

// CInterferometryAppView construction/destruction

CInterferometryAppView::CInterferometryAppView() noexcept
{
	// TODO: add construction code here
	m_hasImage = false;

}

CInterferometryAppView::~CInterferometryAppView()
{
}

BOOL CInterferometryAppView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

// CInterferometryAppView drawing

void CInterferometryAppView::OnDraw(CDC* pDC)
{
	CInterferometryAppDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
	CRect rect;
	GetClientRect(&rect);

	if (m_hasImage) {
		CString info;
		info.Format(_T("✓ ImageLoader Test Success!\nImage: %d x %d pixels\nPress Ctrl+O to load another"),
			m_imageLoader.GetWidth(),
			m_imageLoader.GetHeight());

		CRect textRect = rect;
		textRect.bottom = 30;
		pDC->DrawText(info, &textRect, DT_CENTER | DT_TOP);
		// Попытаемся нарисовать изображение
		HBITMAP hBitmap = m_imageLoader.CreateHBITMAP();
		if (hBitmap) {
			CDC dcMem;
			dcMem.CreateCompatibleDC(pDC);
			HBITMAP hOldBitmap = (HBITMAP)dcMem.SelectObject(hBitmap);

			int width = m_imageLoader.GetWidth();
			int height = m_imageLoader.GetHeight();

			// Рисуем изображение под текстом
			pDC->BitBlt(10, 60, width, height, &dcMem, 0, 0, SRCCOPY);

			dcMem.SelectObject(hOldBitmap);
			DeleteObject(hBitmap);
		}
	}
	else
	{
		// Если нет изображения, показываем инструкцию
		CString msg = _T("ImageLoader Integration Test\n\n")
			_T("Press Ctrl+O or File → Open to load an image\n")
			_T("Supported formats: BMP, JPG, PNG\n\n")
			_T("This tests that ImageLoader.cpp is properly linked.");

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

BOOL CInterferometryAppView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CInterferometryAppView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void CInterferometryAppView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}

void CInterferometryAppView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CInterferometryAppView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CInterferometryAppView diagnostics

#ifdef _DEBUG
void CInterferometryAppView::AssertValid() const
{
	CView::AssertValid();
}

void CInterferometryAppView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CInterferometryAppDoc* CInterferometryAppView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CInterferometryAppDoc)));
	return (CInterferometryAppDoc*)m_pDocument;
}
#endif //_DEBUG


// CInterferometryAppView message handlers

void CInterferometryAppView::OnFileOpen()
{
	// Диалог выбора файла
	CFileDialog dlg(TRUE, _T("bmp"), NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("Image Files (*.bmp;*.jpg;*.png)|*.bmp;*.jpg;*.png|All Files (*.*)|*.*||"));

	if (dlg.DoModal() == IDOK) {
		CString filePath = dlg.GetPathName();

		// Конвертировать CString в std::wstring
		std::wstring wFilePath(filePath);

		// Загрузить через ImageLoader
		if (m_imageLoader.Load(wFilePath)) {
			m_hasImage = true;

			// Показать информацию
			CString msg;
			msg.Format(_T("✓ Image loaded successfully!\n\n")
				_T("Width:    %d pixels\n")
				_T("Height:   %d pixels\n")
				_T("ImageLoader is working correctly!"),
				m_imageLoader.GetWidth(),
				m_imageLoader.GetHeight());
				//m_imageLoader.GetCha());
			AfxMessageBox(msg);

			// Перерисовать
			Invalidate();
		}
		else {
			m_hasImage = false;
			AfxMessageBox(_T("✗ Failed to load image!\n\nCheck that the file is a valid BMP/JPG/PNG."));
		}
	}
}
