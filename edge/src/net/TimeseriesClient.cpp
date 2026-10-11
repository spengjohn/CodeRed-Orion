#include "TimeseriesClient.hpp"

#include <libpq-fe.h>

#include <cstdio>
#include <string>

bool TimeseriesClient::init(const Config& cfg) {
    shutdown();
    last_ok_ = false;
    last_error_.clear();

    if (cfg.tigerdata_url.empty()) {
        last_error_ = "TIGERDATA_URL not set";
        return false;
    }

    PGconn* conn = PQconnectdb(cfg.tigerdata_url.c_str());
    conn_ = conn;
    if (PQstatus(conn) != CONNECTION_OK) {
        last_error_ = PQerrorMessage(conn);
        PQfinish(conn);
        conn_ = nullptr;
        return false;
    }

    last_ok_ = true;
    return true;
}

void TimeseriesClient::shutdown() {
    if (conn_ != nullptr) {
        PQfinish(static_cast<PGconn*>(conn_));
        conn_ = nullptr;
    }
    last_ok_ = false;
}

bool TimeseriesClient::post_vitals(const VitalsReading& reading) {
    PGconn* conn = static_cast<PGconn*>(conn_);
    if (conn == nullptr || PQstatus(conn) != CONNECTION_OK) {
        last_ok_ = false;
        last_error_ = "not connected";
        return false;
    }

    char unix_time_buf[32];
    char weight_buf[32];
    char ambient_buf[32];
    char baby_buf[32];
    char breaths_buf[32];
    char moved_buf[8];

    std::snprintf(unix_time_buf, sizeof(unix_time_buf), "%lld",
                  static_cast<long long>(reading.unix_time));
    std::snprintf(weight_buf, sizeof(weight_buf), "%.2f", reading.weight);
    std::snprintf(ambient_buf, sizeof(ambient_buf), "%.2f", reading.ambient_temp);
    std::snprintf(baby_buf, sizeof(baby_buf), "%.2f", reading.baby_temp);
    std::snprintf(breaths_buf, sizeof(breaths_buf), "%d", reading.breaths);
    std::snprintf(moved_buf, sizeof(moved_buf), "%s", reading.moved ? "true" : "false");

    const char* params[6] = {
        unix_time_buf,
        weight_buf,
        ambient_buf,
        baby_buf,
        breaths_buf,
        moved_buf,
    };

    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO vitals "
        "(time, unix_time, weight, ambient_temp, baby_temp, breaths, moved) "
        "VALUES (to_timestamp($1::bigint), $1::bigint, $2::numeric, $3::numeric, "
        "$4::numeric, $5::integer, $6::boolean)",
        6,
        nullptr,
        params,
        nullptr,
        nullptr,
        0
    );

    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK) {
        last_ok_ = false;
        last_error_ = PQerrorMessage(conn);
        PQclear(result);
        return false;
    }

    PQclear(result);
    last_ok_ = true;
    last_error_.clear();
    return true;
}

bool TimeseriesClient::last_ok() const {
    return last_ok_;
}

const std::string& TimeseriesClient::last_error() const {
    return last_error_;
}
