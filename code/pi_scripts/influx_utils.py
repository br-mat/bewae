########################################################################################################################
# influx_utils.py
# Shared InfluxDB utilities for creating clients and writing data points.
########################################################################################################################

import logging
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS, WriteApi

logger = logging.getLogger(__name__)


# ---- get_client ----
# Create and return an InfluxDB client from config dict.
# Expects config keys: server, port, db_org, and a token key such as db_token.
def get_client(config: dict, token_key: str = "db_token") -> InfluxDBClient:
    url = config["server"] + config["port"]
    return InfluxDBClient(url=url, token=config[token_key], org=config["db_org"])


# ---- get_write_api ----
# Create and return a synchronous write API from an existing client.
# Create this once per script run and pass it to write_point; do not call per write.
def get_write_api(client: InfluxDBClient) -> WriteApi:
    return client.write_api(write_options=SYNCHRONOUS)


# ---- write_point ----
# Write a single data point to InfluxDB.
# write_api: created once via get_write_api() and reused across calls.
# tags: dict of tag key-value pairs, e.g. {"location": "Graz", "source": "openweathermap"}
# data: dict of field key-value pairs; all values must be int or float.
# timestamp: optional datetime/pd.Timestamp; when None, InfluxDB uses server arrival time.
def write_point(write_api: WriteApi, bucket: str, measurement: str, tags: dict, data: dict, timestamp=None) -> None:
    point = Point(measurement)

    if timestamp is not None:
        point = point.time(timestamp)

    for tag_key, tag_val in tags.items():
        point = point.tag(tag_key, tag_val)

    for field_key, field_val in data.items():
        if isinstance(field_val, (int, float)):
            point = point.field(field_key, field_val)
        else:
            raise ValueError(f"Invalid value for field '{field_key}': {field_val!r}; only int/float allowed.")

    write_api.write(bucket=bucket, record=point)
    ts_info = f" @{timestamp}" if timestamp else ""
    logger.info(f"Wrote {len(data)} fields to [{bucket}] {measurement} tags={tags}{ts_info}")
