// TracingProjectData.h

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "ImageLoader.h"
#include "EllipseBoundary.h"
#include "FringeTracer.h"
#include "TracingTypes.h"

namespace Interferometry
{

  // Этапы обработки - строго последовательно.
  // каждый следующий требует завершения предыдущего.
  enum class ProcessingStage
  {
    Empty,        // Ничего не загружено
    ImageLoaded,  // Изображение загружено
    BoundarySet,  // Внешний эллипс установлен
    Traced,       // Полосы трассированы
    Approximated, // Аппроксимация выполнена
    PhaseComputed // Фаза вычислена
  };

  // Результат трассировки - все линии + метаданные
  struct TracingResult
  {
    std::vector<FringeLine> lines;

    void Clear() { lines.clear(); }
    bool IsEmpty() const { return lines.empty(); }
    size_t LineCount() const { return lines.size(); }
  };

  /// Центральный класс проекта Трассировки.
  ///
  /// Владеет всеми данными и core-модулями.
  /// Не зависит от MFC - можно использовать в консольных тестах.
  ///
  /// Инвариант: m_stage всегда отражает реальное состояние данных.
  /// при откате (ResetToStage) данные последующийх этапов очищаются.
  class TracingProjectData
  {
  public:
    TracingProjectData();
    ~TracingProjectData();

    // === Текщий этап ===
    ProcessingStage GetStage() const { return m_stage; }
    bool IsAtLeast(ProcessingStage stage) const
    {
      return m_stage >= stage;
    }

    // === изображение ===
    bool LoadImage(const std::string &path);
    bool HasImage() const
    {
      return IsAtLeast(ProcessingStage::ImageLoaded);
    }
    const ImageLoader &GetImage() const { return m_imageLoader; }

    // === Границы ===
    void SetOuterEllipse(const EllipseParams &params);
    void SetInnerElliepse(const EllipseParams &params);
    void ResetBoundaries();
    bool HasBoundary() const
    {
      return IsAtLeast(ProcessingStage::BoundarySet);
    }
    bool IsOuterSet() const { return m_outerSet; }
    bool IsInnerSet() const { return m_innerSet; }
    const CEllipseBoundary GetBoundary() const { return m_boundary; }
    CEllipseBoundary &GetBoundaryMut() { return m_boundary; }

    // === Трассировка ===
    bool TraceFringeAt(int x, int y);
    void ClearTracing();
    bool HasTracing() const
    {
      return IsAtLeast(ProcessingStage::Traced);
    }
    const TracingResult &GetTracingResult() const
    {
      return m_tracingResult;
    }

    // === Управление этапами ===
    /// Откатить до указанного этапа.
    /// Все данные последующих этапов очищаются.
    void ResetToStage(ProcessingStage stage);

    /// Полный сброс.
    void ResetAll();

  private:
    ProcessingStage m_stage;

    // Core-модули
    ImageLoader m_imageLoader;
    CEllipseBoundary m_boundary;
    CFringeTracer m_tracer;

    // Флаги границ
    bool m_outerSet;
    bool m_innerSet;

    // Результаты этапов
    TracingResult m_tracingResult;
    // ApproximationResult m_approxResult;  // будущее
    // PhaseMap m_phaseMap;                 // будущее

    // пересчитать m_stage по реальному состояния данных.
    void RecalcStage();
  };

} // namespace Interferometry
