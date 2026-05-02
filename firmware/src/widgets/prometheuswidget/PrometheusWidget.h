#ifndef PROMETHEUS_WIDGET_H
#define PROMETHEUS_WIDGET_H

#include "PrometheusDataModel.h"
#include "Widget.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

class PrometheusWidget : public Widget {
public:
    PrometheusWidget(ScreenManager &manager);
    void setup() override;
    void update(bool force = false) override;
    void draw(bool force = false) override;
    void buttonPressed(uint8_t buttonId, ButtonState state) override;
    String getName() override;

private:
    static const int MAX_METRICS = 5;

    PrometheusMetric m_metrics[MAX_METRICS];
    int m_metricCount = 0;
    unsigned long m_lastUpdate = 0;
    unsigned long m_updateInterval = 30000UL;

    void parseConfig(const char *config, PrometheusMetric &metric);
    bool fetchMetric(PrometheusMetric &metric);

    void drawMetric(int screen, PrometheusMetric &metric);
    void drawText(int screen, PrometheusMetric &metric);
    void drawGauge(int screen, PrometheusMetric &metric);
    void drawBar(int screen, PrometheusMetric &metric);
    void drawNoData(int screen);

    String formatValue(float value);
    String urlEncode(const String &str);
};

#endif // PROMETHEUS_WIDGET_H
