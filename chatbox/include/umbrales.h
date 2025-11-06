// ===== UMBRALES DE RIEGO - MODO DÍA (6:00 - 18:00) =====
const float TEMP_MAX_DIA = 30.0;      // °C - No regar si temp > 30°C
const float LUZ_MAX_DIA = 50000.0;    // Lux - No regar si luz solar muy fuerte
const int HUMEDAD_MIN_DIA = 35;       // % - Activar riego si < 35%

// ===== UMBRALES DE RIEGO - MODO NOCHE (18:00 - 6:00) =====
const float TEMP_MIN_NOCHE = 12.0;    // °C - No regar si temp < 12°C
const int HUMEDAD_MIN_NOCHE = 30;     // % - Activar riego si < 30%

// ===== UMBRAL COMÚN =====
const int HUMEDAD_CANCELAR = 70;      // % - Cancelar riego si > 70%
