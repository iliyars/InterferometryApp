
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

CInterferometryAppDoc::CInterferometryAppDoc() noexcept {}

CInterferometryAppDoc::~CInterferometryAppDoc() {}

BOOL CInterferometryAppDoc::OnNewDocument()
{
  if (!CDocument::OnNewDocument())
    return FALSE;

  m_project.ResetAll();
  return TRUE;
}

void CInterferometryAppDoc::NotifyDataChanged()
{
  UpdateAllViews(NULL);
  SetModifiedFlag(TRUE);
}

bool CInterferometryAppDoc::OpenImageDialog()
{
  CFileDialog dlg(TRUE, nullptr, nullptr,
                  OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                  _T("Images (*.bmp;*.png;*.jpg;*.tif)|*.bmp;*.png;*.jpg;*.tif|All (*.*)|*.*||"));

  if (dlg.DoModal() != IDOK)
    return false;

  // CString -> std::string
  CT2CA ascii(dlg.GetPathName());
  std::string path(ascii);

  if (!m_project.LoadImage(path))
  {
    AfxMessageBox(_T("Не удалось загрузить изображение."));
    return false;
  }

  SetTitle(dlg.GetFileTitle());
  NotifyDataChanged();
  return true;
}
