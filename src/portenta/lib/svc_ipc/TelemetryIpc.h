#pragma once

namespace gms_edge {

// Shared struct between M7 and M4 cores
struct TelemetryIpc {
    float air_humidity;
    float air_temperature;
    float soil_moisture;
    float soil_temperature;
    float soil_conductivity;
    float soil_ph;
    float soil_nitrogen;
    float soil_phosphorus;
    float soil_potassium;
    float soil_salinity;
    float soil_tds;
};

} // namespace gms_edge
