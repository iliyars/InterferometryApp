// TracingProjectData.cpp

#include "TracingProjectData.h"

namespace Interferometry
{

  TracingProjectData::TracingProjectData()
      : m_stage(ProcessingStage::Empty), m_outerSet(false), m_innerSet(false)
  {
  }

  TracingProjectData::~TracingProjectData() = default;

  // ===============================================
  // Изображение
  // ===============================================

  bool TracingProjectData::LoadImage(const std::string &path)
  {
    // Загрузка нового изображения, сбрасывает всё последующее
    ResetToStage(ProcessingStage::Empty);

    if (!m_imageLoader.Load(path))
      return;

    // Инициализировать границы под размер изображения
    m_boundary.Initialize(m_imageLoader.GetWidth(), m_imageLoader.GetHeight());

    m_stage = ProcessingStage::ImageLoaded;
    return true;
  }

  // ======================================================
  // Границы
  // ======================================================

  void TracingProjectData::SetOuterEllipse(const EllipseParams &params)
  {
    // Новые границы -> старая трассировка невалидна
    if (HasTracing())
      ClearTracing();

    m_boundary.SetEllipse(params, /*isOuter=*/true);
    m_outerSet = true;

    RecalcStage();
  }

  void TracingProjectData::SetInnerElliepse(const EllipseParams &params)
  {
    if (HasTracing())
      ClearTracing();

    m_boundary.SetEllipse(params, /*isOuter=*/false);
    m_innerSet = true;

    RecalcStage();
  }

  void TracingProjectData::ResetBoundaries()
  {
    if (HasImage())
    {
      m_boundary.Initialize(m_imageLoader.GetWidth(), m_imageLoader.GetHeight());
    }
    m_outerSet = false;
    m_innerSet = false;
    ClearTracing();

    RecalcStage();
  }

  // ================================================================
  // Трассировка
  // ================================================================

  bool TracingProjectData::TraceFringeAt(int x, int y)
  {
    if (!HasBoundary())
      return false;

    // Инициализируем трассировщик каждый раз, потому что границы могли измениться
    m_tracer.Initialize(m_imageLoader.GetImage(), m_boundary);

    auto points = m_tracer.TraceLine(x, y);

    if (points.empty())
      return false;

    // Создать линию
    FringeLine line;
    line.points = std::move(points);
    line.id = static_cast<int>(m_tracingResult.lines.size());

    m_tracingResult.lines.push_back(std::move(line));

    RecalcStage();
    return true;
  }

  void TracingProjectData::ClearTracing()
  {
    m_tracingResult.Clear();
    RecalcStage();
  }

  // ========================================================
  // управление этапами
  // ========================================================

  void TracingProjectData::ResetToStage(ProcessingStage stage)
  {
    // Очищаем от конца к началу
    // (будущее) if (stage < ProcessingStage::PhaseComputed)
    //     m_phaseMap.Clear();

    // (будущее) if (stage < ProcessingStage::Approximated)
    //     m_approxResult.Clear();

    if (stage < ProcessingStage::Traced)
      m_tracingResult.Clear();

    if (stage < ProcessingStage::BoundarySet)
    {
      if (HasImage())
      {
        m_boundary.Initialize(m_imageLoader.GetWidth(), m_imageLoader.GetHeight());
      }
      m_outerSet = false;
      m_innerSet = false;
    }

    if (stage < ProcessingStage::ImageLoaded)
    {
      // ImageLoader не имеет метода Clear - просто сбрасывает флаги
      m_outerSet = false;
      m_innerSet = false;
    }

    RecalcStage();
  }

  void TracingProjectData::ResetAll()
  {
    ResetToStage(ProcessingStage::Empty);
  }

  // =============================================================
  // Приватные
  // =============================================================

  void TracingProjectData::RecalcStage()
  {
    // Определяем stage снизу вверх
    if (!m_imageLoader.HasImage())
    {
      m_stage = ProcessingStage::Empty;
      return;
    }

    if (!m_outerSet)
    {
      m_stage = ProcessingStage::ImageLoaded;
      return;
    }

    if (m_tracingResult.IsEmpty())
    {
      m_stage = ProcessingStage::BoundarySet;
      return;
    }

    m_stage = ProcessingStage::Traced;

    // Потом:
    // if (!m_approxResult.IsEmpty())
    //     m_stage = ProcessingStage::Approximated;
    // if (!m_phaseMap.IsEmpty())
    //     m_stage = ProcessingStage::PhaseComputed;
  }
} // namespace Interferometry
