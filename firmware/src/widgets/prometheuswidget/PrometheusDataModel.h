#ifndef PROMETHEUS_DATA_MODEL_H
#define PROMETHEUS_DATA_MODEL_H

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class PrometheusViz {
    TEXT,
    GAUGE,
    BAR
};

struct PrometheusMetric {
    // Config (set from #defines)
    String query;
    String label;
    String unit;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    PrometheusViz viz = PrometheusViz::TEXT;
    uint32_t color = TFT_CYAN;

    // Runtime state
    float value = 0.0f;
    bool hasData = false;
    bool changed = false;
    bool error = false;
};

#endif // PROMETHEUS_DATA_MODEL_H
