#include "PrometheusWidget.h"
#include "config_helper.h"

// Gauge arc geometry (angles: 0=top, clockwise)
static const int GAUGE_CX = 120;
static const int GAUGE_CY = 115;
static const int GAUGE_R = 95;
static const int GAUGE_IR = 70;
static const int GAUGE_START = 225;  // lower-left
static const int GAUGE_SWEEP = 270;  // total arc span

// Bar geometry
static const int BAR_X = 90;
static const int BAR_Y = 45;
static const int BAR_W = 60;
static const int BAR_H = 145;

// Background arc colour (~25% grey in RGB565)
static const uint16_t ARC_BG = 0x4208;

PrometheusWidget::PrometheusWidget(ScreenManager &manager) : Widget(manager) {
#ifdef PROMETHEUS_UPDATE_INTERVAL
    m_updateInterval = (unsigned long)PROMETHEUS_UPDATE_INTERVAL * 1000UL;
#endif

#ifdef PROMETHEUS_QUERY_1
    parseConfig(PROMETHEUS_QUERY_1, m_metrics[m_metricCount++]);
#endif
#ifdef PROMETHEUS_QUERY_2
    parseConfig(PROMETHEUS_QUERY_2, m_metrics[m_metricCount++]);
#endif
#ifdef PROMETHEUS_QUERY_3
    parseConfig(PROMETHEUS_QUERY_3, m_metrics[m_metricCount++]);
#endif
#ifdef PROMETHEUS_QUERY_4
    parseConfig(PROMETHEUS_QUERY_4, m_metrics[m_metricCount++]);
#endif
#ifdef PROMETHEUS_QUERY_5
    parseConfig(PROMETHEUS_QUERY_5, m_metrics[m_metricCount++]);
#endif
}

void PrometheusWidget::parseConfig(const char *config, PrometheusMetric &m) {
    // Format: "promql;label;unit;min;max;viz;color"
    // Delimiter is ';' — safe because ';' is not valid PromQL syntax.
    // 'color' is an optional RGB565 hex literal, e.g. 0x07E0 (green).
    String s(config);
    String fields[7];
    int fieldIdx = 0, start = 0;
    for (int i = 0; i <= (int)s.length() && fieldIdx < 7; i++) {
        if (i == (int)s.length() || s[i] == ';') {
            fields[fieldIdx++] = s.substring(start, i);
            start = i + 1;
        }
    }

    m.query  = fields[0];
    m.label  = fields[1];
    m.unit   = fields[2];
    m.minVal = fields[3].length() ? fields[3].toFloat() : 0.0f;
    m.maxVal = fields[4].length() ? fields[4].toFloat() : 1.0f;
    if (m.maxVal <= m.minVal) m.maxVal = m.minVal + 1.0f;

    String viz = fields[5];
    viz.toLowerCase();
    if (viz == "gauge")     m.viz = PrometheusViz::GAUGE;
    else if (viz == "bar")  m.viz = PrometheusViz::BAR;
    else                    m.viz = PrometheusViz::TEXT;

    // Optional colour field — strtoul with base 0 auto-detects "0x" prefix
    if (fields[6].length() > 1) {
        unsigned long parsed = strtoul(fields[6].c_str(), nullptr, 0);
        m.color = parsed ? (uint32_t)parsed : TFT_CYAN;
    }
}

void PrometheusWidget::setup() {
}

void PrometheusWidget::update(bool force) {
    if (!force && m_lastUpdate != 0 && (millis() - m_lastUpdate) < m_updateInterval)
        return;

    setBusy(true);
    for (int i = 0; i < m_metricCount; i++) {
        fetchMetric(m_metrics[i]);
    }
    setBusy(false);
    m_lastUpdate = millis();
}

bool PrometheusWidget::fetchMetric(PrometheusMetric &m) {
#ifndef PROMETHEUS_URL
    return false;
#else
    String url = String(PROMETHEUS_URL) + "/api/v1/query?query=" + urlEncode(m.query);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);

#if defined(PROMETHEUS_USER) && defined(PROMETHEUS_PASS)
    http.setAuthorization(PROMETHEUS_USER, PROMETHEUS_PASS);
#endif

    int code = http.GET();
    bool ok = false;

    if (code > 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());
        if (!err && doc["status"] == "success") {
            const char *resultType = doc["data"]["resultType"];

            if (strcmp(resultType, "vector") == 0) {
                JsonArray results = doc["data"]["result"].as<JsonArray>();
                if (results.size() > 0) {
                    m.value   = atof(results[0]["value"][1].as<const char *>());
                    m.hasData = true;
                    m.error   = false;
                    m.changed = true;
                    ok = true;
                }
            } else if (strcmp(resultType, "scalar") == 0) {
                // scalar result: [timestamp, "value"]
                m.value   = atof(doc["data"]["result"][1].as<const char *>());
                m.hasData = true;
                m.error   = false;
                m.changed = true;
                ok = true;
            }
        } else {
            Serial.println("[Prometheus] JSON error or status != success for: " + m.query);
        }
    } else {
        Serial.printf("[Prometheus] HTTP error %d for: %s\n", code, m.query.c_str());
    }

    if (!ok) {
        m.error   = true;
        m.changed = true;
    }

    http.end();
    return ok;
#endif
}

void PrometheusWidget::draw(bool force) {
    m_manager.setFont(DEFAULT_FONT);
    for (int i = 0; i < m_metricCount; i++) {
        if (m_metrics[i].changed || force) {
            drawMetric(i, m_metrics[i]);
            m_metrics[i].changed = false;
        }
    }
}

void PrometheusWidget::drawMetric(int screen, PrometheusMetric &m) {
    if (!m.hasData && !m.error) return;
    if (m.error) { drawNoData(screen); return; }

    switch (m.viz) {
        case PrometheusViz::GAUGE: drawGauge(screen, m); break;
        case PrometheusViz::BAR:   drawBar(screen, m);   break;
        default:                   drawText(screen, m);  break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TEXT visualisation
// ─────────────────────────────────────────────────────────────────────────────
void PrometheusWidget::drawText(int screen, PrometheusMetric &m) {
    m_manager.selectScreen(screen);
    m_manager.fillScreen(TFT_BLACK);

    // Thin decorative ring
    m_manager.drawSmoothArc(120, 120, 118, 115, 0, 360, m.color, TFT_BLACK);

    // Label (top)
    m_manager.setFontColor(m.color, TFT_BLACK);
    m_manager.drawString(m.label.length() ? m.label : m.query,
                         120, 58, 18, Align::MiddleCenter);

    // Value (large, auto-sized to ~100px wide)
    String valStr = formatValue(m.value);
    if (m.unit.length()) valStr += " " + m.unit;
    m_manager.setFontColor(TFT_WHITE, TFT_BLACK);
    m_manager.drawFittedString(valStr, 120, 120, 200, 50, Align::MiddleCenter);

    // Divider
    m_manager.drawLine(40, 148, 200, 148, ARC_BG);

    // Update timestamp hint (last refreshed indicator — just a dim dot row)
    m_manager.setFontColor(0x39E7, TFT_BLACK); // dim grey
    m_manager.drawString("PROMETHEUS", 120, 178, 11, Align::MiddleCenter);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAUGE visualisation — smooth arc speedometer style
// ─────────────────────────────────────────────────────────────────────────────
void PrometheusWidget::drawGauge(int screen, PrometheusMetric &m) {
    m_manager.selectScreen(screen);
    m_manager.fillScreen(TFT_BLACK);

    float pct = (m.value - m.minVal) / (m.maxVal - m.minVal);
    pct = pct < 0.0f ? 0.0f : (pct > 1.0f ? 1.0f : pct);

    // Background arc (full sweep)
    m_manager.drawSmoothArc(GAUGE_CX, GAUGE_CY, GAUGE_R, GAUGE_IR,
                             GAUGE_START, GAUGE_START + GAUGE_SWEEP,
                             ARC_BG, TFT_BLACK);

    // Value arc (filled portion)
    if (pct > 0.01f) {
        uint32_t endAngle = GAUGE_START + (uint32_t)(pct * GAUGE_SWEEP);
        m_manager.drawSmoothArc(GAUGE_CX, GAUGE_CY, GAUGE_R, GAUGE_IR,
                                 GAUGE_START, endAngle,
                                 m.color, TFT_BLACK, true);
    }

    // Value text inside arc
    String valStr = formatValue(m.value);
    m_manager.setFontColor(TFT_WHITE, TFT_BLACK);
    m_manager.drawFittedString(valStr, GAUGE_CX, GAUGE_CY - 8, 130, 44, Align::MiddleCenter);

    // Unit (small, below value)
    if (m.unit.length()) {
        m_manager.setFontColor(0x8410, TFT_BLACK); // mid-grey
        m_manager.drawString(m.unit, GAUGE_CX, GAUGE_CY + 26, 13, Align::MiddleCenter);
    }

    // Label (below arc)
    m_manager.setFontColor(m.color, TFT_BLACK);
    m_manager.drawString(m.label.length() ? m.label : m.query,
                         GAUGE_CX, 200, 15, Align::MiddleCenter);

    // Min / max ticks (small text at arc endpoints)
    m_manager.setFontColor(0x4208, TFT_BLACK);
    m_manager.drawString(formatValue(m.minVal), 28,  196, 11, Align::MiddleCenter);
    m_manager.drawString(formatValue(m.maxVal), 212, 196, 11, Align::MiddleCenter);
}

// ─────────────────────────────────────────────────────────────────────────────
// BAR visualisation — vertical level indicator
// ─────────────────────────────────────────────────────────────────────────────
void PrometheusWidget::drawBar(int screen, PrometheusMetric &m) {
    m_manager.selectScreen(screen);
    m_manager.fillScreen(TFT_BLACK);

    float pct = (m.value - m.minVal) / (m.maxVal - m.minVal);
    pct = pct < 0.0f ? 0.0f : (pct > 1.0f ? 1.0f : pct);
    int fillH = (int)(pct * BAR_H);

    // Bar background
    m_manager.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, ARC_BG);

    // Bar fill (grows from bottom)
    if (fillH > 0) {
        m_manager.fillRect(BAR_X, BAR_Y + BAR_H - fillH, BAR_W, fillH, m.color);
    }

    // Bar border
    m_manager.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, 0x8410);

    // Tick marks at 25 / 50 / 75 %
    uint16_t tickColor = 0x2945;
    for (int t = 1; t <= 3; t++) {
        int ty = BAR_Y + BAR_H - (int)(t * 0.25f * BAR_H);
        m_manager.drawLine(BAR_X, ty, BAR_X + BAR_W, ty, tickColor);
    }

    // Value text (above bar)
    String valStr = formatValue(m.value);
    if (m.unit.length()) valStr += " " + m.unit;
    m_manager.setFontColor(TFT_WHITE, TFT_BLACK);
    m_manager.drawFittedString(valStr, 120, 28, 220, 30, Align::MiddleCenter);

    // Label (below bar)
    m_manager.setFontColor(m.color, TFT_BLACK);
    m_manager.drawString(m.label.length() ? m.label : m.query,
                         120, 207, 15, Align::MiddleCenter);
}

// ─────────────────────────────────────────────────────────────────────────────
// Error / no-data state
// ─────────────────────────────────────────────────────────────────────────────
void PrometheusWidget::drawNoData(int screen) {
    m_manager.selectScreen(screen);
    m_manager.fillScreen(TFT_BLACK);
    m_manager.drawSmoothArc(120, 120, 118, 115, 0, 360, TFT_RED, TFT_BLACK);
    m_manager.setFontColor(TFT_RED, TFT_BLACK);
    m_manager.drawString("N/A", 120, 105, 36, Align::MiddleCenter);
    m_manager.setFontColor(0x8410, TFT_BLACK);
    m_manager.drawString("PROMETHEUS", 120, 148, 11, Align::MiddleCenter);
}

// ─────────────────────────────────────────────────────────────────────────────
// Button handling — middle button forces an immediate refresh
// ─────────────────────────────────────────────────────────────────────────────
void PrometheusWidget::buttonPressed(uint8_t buttonId, ButtonState state) {
    if (buttonId == BUTTON_OK && state == BTN_SHORT) {
        update(true);
    }
}

String PrometheusWidget::getName() {
    return "Prometheus";
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
String PrometheusWidget::formatValue(float val) {
    if (isnan(val)) return "NaN";
    if (isinf(val)) return val > 0 ? "+Inf" : "-Inf";

    float absVal = fabsf(val);
    char buf[16];

    if (absVal >= 1e9f)       snprintf(buf, sizeof(buf), "%.2fG", val / 1e9f);
    else if (absVal >= 1e6f)  snprintf(buf, sizeof(buf), "%.2fM", val / 1e6f);
    else if (absVal >= 1e3f)  snprintf(buf, sizeof(buf), "%.2fk", val / 1e3f);
    else if (absVal >= 100.f) snprintf(buf, sizeof(buf), "%.1f",  val);
    else                      snprintf(buf, sizeof(buf), "%.2f",  val);

    return String(buf);
}

String PrometheusWidget::urlEncode(const String &str) {
    String out;
    out.reserve(str.length() * 2);
    for (int i = 0; i < (int)str.length(); i++) {
        char c = str[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            out += buf;
        }
    }
    return out;
}
