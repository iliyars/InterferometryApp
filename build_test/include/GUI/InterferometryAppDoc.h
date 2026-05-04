
// InterferometryAppDoc.h : interface of the CInterferometryAppDoc class
//

#pragma once

#include "TracingProjectData.h"

class CInterferometryAppDoc : public CDocument {
 protected:  // create from serialization only
  CInterferometryAppDoc() noexcept;
  DECLARE_DYNCREATE(CInterferometryAppDoc)

 public:
  // Единственная точка доступа к данным для всех View
  Interferometry::TracingProjectData& GetProject() { return m_project; }
  const Interferometry::TracingProjectData& GetProject() const {
    return m_project;
  }

  // MFC-спцифичные обёртки (диалоги, уведомления)
  bool OpenImageDialog();
  void NotifyDataChanged();

  // MFC overrides
  BOOL OnNewDocument() override;
  virtual ~CInterferometryAppDoc();

 private:
  Interferometry::TracingProjectData m_project;

 protected:
  DECLARE_MESSAGE_MAP()
};
