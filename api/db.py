import psycopg
from psycopg.rows import dict_row


def get_connection():
    __DB_CONFIG = {
        "host": "localhost",
        "port": "5432",
        "dbname": "jrhyaku",
        "user": "developer"
    }

    return psycopg.connect(**__DB_CONFIG, row_factory=dict_row)


def _attach_completion_pct(conn, line: dict) -> dict:
    with conn.cursor() as cur:
        cur.execute(
            "SELECT COUNT(*) AS total, "
            "COUNT(*) FILTER (WHERE visited_at IS NOT NULL) as visited "
            "FROM stations WHERE line_cd = %s",
            (line["line_cd"],),
        )

        counts = cur.fetchone()
        total, visited = counts["total"], counts["visited"]
        line["completion_pct"] = (visited / total * 100) if total > 0 else 0.0
    return line


def fetch_lines() -> list[dict]:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("SELECT * FROM lines ORDER BY line_cd")
            lines = cur.fetchall()
        return [_attach_completion_pct(conn, line) for line in lines]


def fetch_line(line_cd: int) -> dict | None:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("SELECT * FROM lines WHERE line_cd = %s", (line_cd,))
            line = cur.fetchone()

        if line is None:
            return None
        return _attach_completion_pct(conn, line)


def fetch_stations_on_line(line_cd: int) -> list[dict]:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT * FROM stations WHERE line_cd = %s ORDER BY station_cd",
                (line_cd,),
            )
            return cur.fetchall()


def fetch_stations() -> list[dict]:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("SELECT * FROM stations ORDER BY station_cd")
            return cur.fetchall()


def fetch_station(station_cd: int) -> dict | None:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT * FROM stations WHERE station_cd = %s",
                (station_cd,),
            )
            return cur.fetchone()


def insert_line(line) -> dict:
    with get_connection() as conn:
        with conn.cursor() as cur:
            try:
                cur.execute(
                    "INSERT INTO lines (line_cd, line_name, route_color, line_type) "
                    "VALUES (%s, %s, %s, %s) RETURNING *",
                    (line.line_cd, line.line_name, line.route_color, line.line_type),
                )
            except psycopg.errors.UniqueViolation:
                conn.rollback()
                raise ValueError(f"line_cd={line.line_cd} already exists")

            new_line = cur.fetchone()
            conn.commit()
        return _attach_completion_pct(conn, new_line)


def insert_station(line_cd: int, station) -> dict:
    with get_connection() as conn:
        with conn.cursor() as cur:
            try:
                cur.execute(
                    "INSERT INTO stations (station_cd, station_name, lon, lat, line_cd) "
                    "VALUES (%s, %s, %s, %s, %s) RETURNING *",
                    (
                        station.station_cd,
                        station.station_name,
                        station.lon,
                        station.lat,
                        line_cd,
                    ),
                )
            except psycopg.errors.UniqueViolation:
                conn.rollback()
                raise ValueError(f"station_cd={station.station_cd} already exists")

            new_station = cur.fetchone()
            conn.commit()
        return new_station


def update_line(line_cd: int, line) -> dict | None:
    fields = line.model_dump(exclude_unset=True)
    if not fields:
        return fetch_line(line_cd)

    set_clause = ", ".join(f"{col} = %s" for col in fields)
    values = list(fields.values()) + [line_cd]
 
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(
                f"UPDATE lines SET {set_clause} WHERE line_cd = %s RETURNING *",
                values,
            )

            updated = cur.fetchone()
            conn.commit()

        if updated is None:
            return None
        return _attach_completion_pct(conn, updated)


def update_station(station_cd: int, station_update) -> dict | None:
    fields = station_update.model_dump(exclude_unset=True)
    if not fields:
        return fetch_station(station_cd)
 
    set_clause = ", ".join(f"{col} = %s" for col in fields)
    values = list(fields.values()) + [station_cd]
 
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(
                f"UPDATE stations SET {set_clause} WHERE station_cd = %s RETURNING *",
                values,
            )

            updated = cur.fetchone()
            conn.commit()
        return updated


def delete_line(line_cd: int) -> bool:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("DELETE FROM lines WHERE line_cd = %s", (line_cd,))
            deleted = cur.rowcount > 0
            conn.commit()
        return deleted


def delete_station(station_cd: int) -> bool:
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("DELETE FROM stations WHERE station_cd = %s", (station_cd,))
            deleted = cur.rowcount > 0
            conn.commit()
        return deleted
